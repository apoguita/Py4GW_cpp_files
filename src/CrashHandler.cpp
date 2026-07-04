// Py4GW crash handler implementation.
// Paths: A = SetUnhandledExceptionFilter, B = GWCA panic handler, C = GW crash-message detour.
//
// Design-safety invariants enforced here:
//   * Crash-time writers use a pre-opened path + static buffers + raw WriteFile. No
//     std::mutex, no heap, no MessageBox, no Py* calls. MiniDumpWriteDump is __try-guarded
//     (it HeapAllocs internally and can fault on a heap-corruption crash).
//   * A single re-entrancy guard (s_handling) prevents recursive dumps; fatal paths latch it
//     (process ends), the WndProc path resets it and is rate-capped.
//   * Path B self-dumps via RtlCaptureContext; it never relies on abort() reaching Path A.
//   * Path C uses the VERIFIED 7-arg __cdecl signature and forwards ALL args to the trampoline.
//   * The crash dir is resolved ONCE at Initialize() (non-crash time) from the process CWD,
//     so it sits next to Py4GW_injection_log.txt and the SEH path never touches a std::string.

#include "CrashHandler.h"
#include "Breadcrumbs.h"

#include <dbghelp.h>           // MiniDumpWriteDump; needs target_link_libraries(... dbghelp)
#include <cstdio>              // _vsnprintf_s / _snwprintf_s into static buffers only
#include <cstdarg>

#include <GWCA/Utilities/Debug.h>     // GW::RegisterPanicHandler
#include <GWCA/Utilities/Scanner.h>   // GW::Scanner::FindUseOfString / ToFunctionStart
#include <GWCA/Utilities/Hooker.h>    // GW::HookBase::CreateHook / EnableHooks / RemoveHook

// The Logger (GWCA-declared, project-implemented) is heap+mutex based; only safe to call
// OUTSIDE the crash path (install / teardown). Never from the SEH filter.
#include <GWCA/Logger/Logger.h>

// Project global: directory containing Py4GW.dll. Used ONLY as an init-time fallback.
extern std::string dllDirectory;

namespace {

// ---- re-entrancy guard ------------------------------------------------------
volatile LONG s_handling = 0;                 // 0 = idle, 1 = a fault is being handled
volatile LONG s_wndproc_dumps = 0;            // rate-cap for the recoverable WndProc path
constexpr LONG kMaxWndProcDumps = 5;

// ---- install/teardown state -------------------------------------------------
bool s_installed = false;
LPTOP_LEVEL_EXCEPTION_FILTER s_prev_filter = nullptr;
uintptr_t s_append_stack_fn = 0;              // resolved GW function (Path C)
void* s_append_stack_orig = nullptr;          // Path C trampoline
DWORD s_prev_policy = 0;                       // saved PROCESS_CALLBACK_FILTER policy
bool s_policy_changed = false;

// ---- pre-resolved crash dir (wide + utf8) ----------------------------------
wchar_t s_crash_dir[MAX_PATH] = {0};          // <cwd>\crashes  (next to Py4GW_injection_log.txt)
bool s_crash_dir_ready = false;
std::string s_crash_dir_utf8;

// ---- crash text channels (static; filled before the dump) -------------------
char s_assert_text[512] = {0};                // Path B assertion text
char s_gw_text[32768] = {0};                  // Path C: GW's full crash report (Crash/System/DllList/Stack)

// Map a Windows exception code to a stable label for the sidecar.
const char* exception_label(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:    return "access_violation";
        case EXCEPTION_STACK_OVERFLOW:      return "stack_overflow";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal_instruction";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:  return "int_divide_by_zero";
        case EXCEPTION_PRIV_INSTRUCTION:    return "priv_instruction";
        case EXCEPTION_IN_PAGE_ERROR:       return "in_page_error";
        case 0xC0000409:                    return "stack_buffer_overrun"; // FAST_FAIL
        case 0xE06D7363:                    return "cpp_exception";
        case 0x80000003:                    return "breakpoint";
        case 0xE0000001:                    return "gwca_assert";
        default:                            return "exception";
    }
}

// Append-into-bounded-buffer helper. On truncation it stops cleanly (no overrun, no -1 math).
struct JBuf { char* p; char* const e; };
void jappend(JBuf& b, const char* fmt, ...) {
    if (b.p >= b.e) return;
    va_list ap; va_start(ap, fmt);
    int n = _vsnprintf_s(b.p, static_cast<size_t>(b.e - b.p), _TRUNCATE, fmt, ap);
    va_end(ap);
    b.p = (n >= 0) ? b.p + n : b.e;           // n<0 => truncated => stop (n==0 is a valid write)
}

// Escape a UTF-8 string into dst for JSON embedding (\\, ", control chars). Static, no heap.
void json_escape(char* dst, size_t cap, const char* src) {
    if (cap == 0) return;
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 2 < cap; ++i) {
        unsigned char c = static_cast<unsigned char>(src[i]);
        if (c == '\\' || c == '"')      { dst[j++] = '\\'; dst[j++] = c; }
        else if (c == '\n')             { dst[j++] = '\\'; dst[j++] = 'n'; }
        else if (c == '\r')             { dst[j++] = '\\'; dst[j++] = 'r'; }
        else if (c == '\t')             { dst[j++] = '\\'; dst[j++] = 't'; }
        else if (c >= 0x20)             { dst[j++] = c; }
        // other control chars are dropped
    }
    dst[j] = 0;
}

// Recursive CreateDirectory for a wide path (no error if it exists). Init-time only.
void make_dir_tree(const wchar_t* path) {
    wchar_t tmp[MAX_PATH];
    wcsncpy_s(tmp, path, _TRUNCATE);
    for (wchar_t* p = tmp + 3; *p; ++p) {     // skip drive "C:\"
        if (*p == L'\\') { *p = 0; CreateDirectoryW(tmp, nullptr); *p = L'\\'; }
    }
    CreateDirectoryW(tmp, nullptr);
}

// Build "<dir>\py4gw-YYYYMMDD-HHMMSS-pid-tid" (no extension). Crash-safe (no heap).
void build_stem(wchar_t* out, size_t cap) {
    SYSTEMTIME st; GetLocalTime(&st);
    _snwprintf_s(out, cap, _TRUNCATE,
        L"%s\\py4gw-%04u%02u%02u-%02u%02u%02u-%lu-%lu",
        s_crash_dir, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
        GetCurrentProcessId(), GetCurrentThreadId());
}

// Append one CRASH line to Py4GW_injection_log.txt (relative -> CWD, same place the Logger
// writes). Crash-safe: CreateFileW(FILE_APPEND_DATA) + WriteFile, no heap, shared access.
void append_injection_log(const char* line) {
    HANDLE h = CreateFileW(L"Py4GW_injection_log.txt", FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0; WriteFile(h, line, static_cast<DWORD>(strlen(line)), &w, nullptr);
    CloseHandle(h);
}

// Write GW's full crash report (s_gw_text) to a human-readable sidecar. Crash-safe: raw WriteFile,
// no heap/escaping (the JSON keeps only a preview, this file keeps the whole thing verbatim).
void write_gwtext(const wchar_t* path, const char* text) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0; WriteFile(h, text, static_cast<DWORD>(strlen(text)), &w, nullptr);
    CloseHandle(h);
}

} // namespace

CrashHandler& CrashHandler::Instance() {
    static CrashHandler inst;
    return inst;
}

std::string CrashHandler::CrashDirUtf8() const { return s_crash_dir_utf8; }

bool CrashHandler::EnsureCrashDir() {
    if (s_crash_dir_ready) return true;
    wchar_t base[MAX_PATH] = {0};
    // Match the Logger's PER-CLIENT behavior. The injection log is written via a RELATIVE path,
    // so it lands in the process CWD. Initialize() runs during DLL init -- BEFORE Py4GW's
    // first-frame ChangeWorkingDirectory(dllDirectory) (Py4GW.cpp:1987) -- so the CWD here is
    // still the GW client's OWN directory. Capturing it now puts crashes/ inside each client's
    // directory, next to that client's Py4GW_injection_log.txt. (The native get_crash_dir()
    // binding hands this same per-client path to the Python side so all artifacts stay together,
    // even though Python runs after the CWD flips to the shared DLL folder.)
    DWORD n = GetCurrentDirectoryW(MAX_PATH, base);
    if (n == 0 || n >= MAX_PATH) {              // fallback: the Py4GW.dll directory
        int m = MultiByteToWideChar(CP_UTF8, 0, dllDirectory.c_str(), -1, base, MAX_PATH);
        if (m <= 0 || base[0] == 0) return false;
    }
    _snwprintf_s(s_crash_dir, MAX_PATH, _TRUNCATE, L"%s\\crashes", base);
    make_dir_tree(s_crash_dir);
    s_crash_dir_ready = true;
    char u8[MAX_PATH * 2];
    if (WideCharToMultiByte(CP_UTF8, 0, s_crash_dir, -1, u8, sizeof(u8), nullptr, nullptr) > 0)
        s_crash_dir_utf8 = u8;
    return true;
}

// ---------------------------------------------------------------------------
// Install / teardown (runs at DLL init/term, NOT crash time -> Logger ok here)
// ---------------------------------------------------------------------------

void CrashHandler::Initialize() {
    if (s_installed) return;                  // idempotent: Initialize() runs more than once
    s_installed = true;
    EnsureCrashDir();                         // pre-cache wide + utf8 dir at NON-crash time
    ClearCallbackFilterPolicy();
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    InstallPathA();
    InstallPathB();
    InstallPathC();                           // best-effort; logs and continues on miss
    Logger::Instance().LogInfo("[CrashHandler] installed (A=SEH, B=panic, C=scanner).");
}

void CrashHandler::Terminate() {
    if (!s_installed) return;
    s_installed = false;
    if (s_append_stack_fn) {                  // Path C: disable before remove (MinHook rule)
        GW::HookBase::DisableHooks(reinterpret_cast<void*>(s_append_stack_fn));
        GW::HookBase::RemoveHook(reinterpret_cast<void*>(s_append_stack_fn));
        s_append_stack_fn = 0;
        s_append_stack_orig = nullptr;        // null trampoline after removal (no stale calls)
    }
    SetUnhandledExceptionFilter(s_prev_filter);   // restore Path A
    GW::RegisterPanicHandler(nullptr, nullptr);   // null Path B
    RestoreCallbackFilterPolicy();
    InterlockedExchange(&s_handling, 0);          // reset guards so a re-Initialize() is clean
    InterlockedExchange(&s_wndproc_dumps, 0);
    Logger::Instance().LogInfo("[CrashHandler] torn down.");
}

void CrashHandler::ClearCallbackFilterPolicy() {
    // Clear PROCESS_CALLBACK_FILTER_ENABLED (bit 0) so WoW64 kernel-callback faults (WndProc,
    // D3D Present) propagate to the user-mode filter. Resolved dynamically; saved for restore.
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (!k32) return;
    using GetFn = BOOL(WINAPI*)(LPDWORD);
    using SetFn = BOOL(WINAPI*)(DWORD);
    auto get = reinterpret_cast<GetFn>(GetProcAddress(k32, "GetProcessUserModeExceptionPolicy"));
    auto set = reinterpret_cast<SetFn>(GetProcAddress(k32, "SetProcessUserModeExceptionPolicy"));
    if (!get || !set) return;
    DWORD policy = 0;
    if (!get(&policy)) return;
    s_prev_policy = policy;
    if (set(policy & 0xFFFFFFFEu)) s_policy_changed = true;
}

void CrashHandler::RestoreCallbackFilterPolicy() {
    if (!s_policy_changed) return;
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (!k32) return;
    using SetFn = BOOL(WINAPI*)(DWORD);
    auto set = reinterpret_cast<SetFn>(GetProcAddress(k32, "SetProcessUserModeExceptionPolicy"));
    if (set) set(s_prev_policy);
    s_policy_changed = false;
}

void CrashHandler::InstallPathA() {
    s_prev_filter = SetUnhandledExceptionFilter(&CrashHandler::TopLevelFilter);
}

void CrashHandler::InstallPathB() {
    GW::RegisterPanicHandler(&CrashHandler::GwcaPanic, nullptr);
}

void CrashHandler::InstallPathC() {
    // Resolve GW's crash-message builder by its (stable) format-string anchor, then detour it.
    // VERIFIED live: anchor lives inside the target; ToFunctionStart lands on its prologue.
    uintptr_t use = GW::Scanner::FindUseOfString("%p  %08x %08x %08x %08x ");
    if (!use) { Logger::Instance().LogWarning("[CrashHandler] Path C anchor miss; A/B only."); return; }
    s_append_stack_fn = GW::Scanner::ToFunctionStart(use, 0xfff);
    if (!s_append_stack_fn) { Logger::Instance().LogWarning("[CrashHandler] Path C prologue miss."); return; }
    int status = GW::HookBase::CreateHook(
        reinterpret_cast<void**>(&s_append_stack_fn),
        reinterpret_cast<void*>(&CrashHandler::AppendStackDetour),
        &s_append_stack_orig);
    if (status != 0 || !s_append_stack_orig) {
        Logger::Instance().LogWarning("[CrashHandler] Path C CreateHook failed.");
        s_append_stack_fn = 0; s_append_stack_orig = nullptr; return;
    }
    GW::HookBase::EnableHooks(reinterpret_cast<void*>(s_append_stack_fn));
    Logger::Instance().LogInfo("[CrashHandler] Path C attached (GW crash-message hook).");
}

// ---------------------------------------------------------------------------
// Path A / WndProc filters
// ---------------------------------------------------------------------------

LONG WINAPI CrashHandler::TopLevelFilter(EXCEPTION_POINTERS* info) {
    Instance().OnException(info, "seh", /*recoverable=*/false);
    return EXCEPTION_EXECUTE_HANDLER;
}

LONG CrashHandler::WndProcFilter(EXCEPTION_POINTERS* info) {
    // Recoverable: historically these faults were swallowed and GW survived via CallWindowProc.
    // Dump (rate-capped so a per-frame fault can't spam dumps), then fall through as before.
    if (InterlockedIncrement(&s_wndproc_dumps) <= kMaxWndProcDumps)
        OnException(info, "wndproc", /*recoverable=*/true);
    return EXCEPTION_EXECUTE_HANDLER;
}

// ---------------------------------------------------------------------------
// Path B: GWCA panic handler (5-arg). Self-dumps; abort() may never reach Path A.
// ---------------------------------------------------------------------------

void CrashHandler::GwcaPanic(void* /*ctx*/, const char* expr, const char* file,
                             unsigned int line, const char* func) {
    static_assert(sizeof(void*) == 4, "Py4GW crash handler is x86 (CONTEXT.Eip) only");
    _snprintf_s(s_assert_text, sizeof(s_assert_text), _TRUNCATE,
                "GWCA_ASSERT(%s) at %s:%u in %s", expr ? expr : "?",
                file ? file : "?", line, func ? func : "?");
    CONTEXT ctx; RtlCaptureContext(&ctx);     // accurate panic-site context (not a throw site)
    EXCEPTION_RECORD rec = {0};
    rec.ExceptionCode = 0xE0000001;           // synthetic: GWCA assertion
    rec.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    rec.ExceptionAddress = reinterpret_cast<void*>(ctx.Eip);
    EXCEPTION_POINTERS info = { &rec, &ctx };
    Instance().OnException(&info, "gwca_assert", /*recoverable=*/false);
    // returns to FatalAssert -> abort(); the dump is already written.
}

// ---------------------------------------------------------------------------
// Path C: detour GW's internal crash-message builder (verified __cdecl, 7 args).
// Forward ALL args to the trampoline, capture GW's text, then self-dump from its CONTEXT.
// ---------------------------------------------------------------------------

uintptr_t __cdecl CrashHandler::AppendStackDetour(void* debug_info, uint32_t a2, uint32_t a3,
                                                  uint32_t a4, CONTEXT* ctx, uint32_t a6,
                                                  uint32_t a7) {
    static_assert(sizeof(void*) == 4, "Py4GW crash handler is x86 (CONTEXT.Eip) only");
    using Fn = uintptr_t(__cdecl*)(void*, uint32_t, uint32_t, uint32_t, CONTEXT*, uint32_t, uint32_t);
    // Defensive: the detour only runs while the hook is enabled (orig is set then, nulled only
    // after RemoveHook), but guard against a teardown race rather than call through null.
    if (!s_append_stack_orig) return 0;
    // Let GW fill its own crash text first (caller cleans the stack; we forward all 7 args).
    uintptr_t ret = reinterpret_cast<Fn>(s_append_stack_orig)(debug_info, a2, a3, a4, ctx, a6, a7);

    // Capture GW's text (VERIFIED: text region at buf+0x20c, len-counted). Guard the read.
    __try {
        const char* gw = reinterpret_cast<const char*>(
            reinterpret_cast<uintptr_t>(debug_info) + 0x20c);
        if (gw && *gw) {
            strncpy_s(s_gw_text, gw, _TRUNCATE);
            bc::breadcrumb_copy("gw_text", s_gw_text);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { /* enrichment only; ignore */ }

    // GW is fatally crashing: synthesize an EXCEPTION from its CONTEXT and dump it.
    if (ctx) {
        EXCEPTION_RECORD rec = {0};
        rec.ExceptionCode = 0x80000003;       // EXCEPTION_BREAKPOINT
        rec.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
        rec.ExceptionAddress = reinterpret_cast<void*>(ctx->Eip);
        EXCEPTION_POINTERS info = { &rec, ctx };
        Instance().OnException(&info, "gw_engine", /*recoverable=*/false);
    }
    return ret;
}

// ---------------------------------------------------------------------------
// Shared crash sink + crash-time-safe writers
// ---------------------------------------------------------------------------

bool CrashHandler::OnException(EXCEPTION_POINTERS* info, const char* source, bool recoverable) {
    if (InterlockedCompareExchange(&s_handling, 1, 0) != 0) {
        // A fault is already being handled (e.g. our writer itself faulted). Don't recurse.
        if (!recoverable) TerminateProcess(GetCurrentProcess(), 1);
        return false;
    }
    if (s_crash_dir_ready) {
        wchar_t stem[MAX_PATH], dmp[MAX_PATH], json[MAX_PATH];
        build_stem(stem, MAX_PATH);
        _snwprintf_s(dmp, MAX_PATH, _TRUNCATE, L"%s.dmp", stem);
        _snwprintf_s(json, MAX_PATH, _TRUNCATE, L"%s.json", stem);
        const wchar_t* slash = wcsrchr(dmp, L'\\');
        const wchar_t* dmp_name = slash ? slash + 1 : dmp;

        // Path C populates GW's full crash report -> give it its own -gwtext.txt sidecar.
        wchar_t gwt[MAX_PATH] = {0};
        const wchar_t* gwt_name = L"";
        if (s_gw_text[0]) {
            _snwprintf_s(gwt, MAX_PATH, _TRUNCATE, L"%s-gwtext.txt", stem);
            const wchar_t* gslash = wcsrchr(gwt, L'\\');
            gwt_name = gslash ? gslash + 1 : gwt;
        }

        bc::LastPyFrame f = bc::read_last_py_frame();   // POD copy, no Py API
        char comment[768];
        _snprintf_s(comment, sizeof(comment), _TRUNCATE,
            "Py4GW %s | %s | py:%s:%d %s | %s",
            PY4GW_VERSION, source, f.file[0] ? f.file : "?", f.line,
            f.func[0] ? f.func : "?", s_assert_text[0] ? s_assert_text : "");

        WriteSidecar(info, json, dmp_name, gwt_name, source);   // JSON first: survives a dump failure
        WriteDump(info, dmp, comment);
        if (gwt[0]) write_gwtext(gwt, s_gw_text);               // full GW report, verbatim

        char dmp_name_u8[MAX_PATH];           // narrow copy: avoid locale-dependent %S in printf
        if (WideCharToMultiByte(CP_UTF8, 0, dmp_name, -1, dmp_name_u8, sizeof(dmp_name_u8), nullptr, nullptr) <= 0)
            dmp_name_u8[0] = 0;
        char log_line[320];
        DWORD code = (info && info->ExceptionRecord) ? info->ExceptionRecord->ExceptionCode : 0;
        _snprintf_s(log_line, sizeof(log_line), _TRUNCATE,
            "CRASH %s 0x%08lX py:%s:%d -> see crashes\\%s\r\n",
            source, static_cast<unsigned long>(code), f.file[0] ? f.file : "?", f.line, dmp_name_u8);
        append_injection_log(log_line);
    }
    if (recoverable) InterlockedExchange(&s_handling, 0);   // allow future WndProc dumps
    return true;
}

void CrashHandler::WriteSidecar(EXCEPTION_POINTERS* info, const wchar_t* json_path,
                                const wchar_t* dmp_name, const wchar_t* gwtext_name,
                                const char* source) {
    HANDLE file = CreateFileW(json_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;

    bc::LastPyFrame f = bc::read_last_py_frame();
    DWORD code = (info && info->ExceptionRecord) ? info->ExceptionRecord->ExceptionCode : 0;
    uintptr_t addr = (info && info->ExceptionRecord)
        ? reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress) : 0;

    char efile[300], efunc[160], eassert[600], egw[1100], edmp[160];
    json_escape(efile, sizeof(efile), f.file);
    json_escape(efunc, sizeof(efunc), f.func);
    json_escape(eassert, sizeof(eassert), s_assert_text);
    json_escape(egw, sizeof(egw), s_gw_text);   // preview only; full report -> gw_text_file
    char dmp_u8[MAX_PATH];
    if (WideCharToMultiByte(CP_UTF8, 0, dmp_name, -1, dmp_u8, sizeof(dmp_u8), nullptr, nullptr) <= 0)
        dmp_u8[0] = 0;
    json_escape(edmp, sizeof(edmp), dmp_u8);
    char egwt[MAX_PATH] = {0};                   // -gwtext.txt name (empty unless Path C)
    if (gwtext_name && gwtext_name[0]) {
        char gwt_u8[MAX_PATH];
        if (WideCharToMultiByte(CP_UTF8, 0, gwtext_name, -1, gwt_u8, sizeof(gwt_u8), nullptr, nullptr) <= 0)
            gwt_u8[0] = 0;
        json_escape(egwt, sizeof(egwt), gwt_u8);
    }

    char buf[8192];   // generous: worst-case gw_text (~1.1K) + 16 escaped breadcrumbs (~4K) must fit
    JBuf b { buf, buf + sizeof(buf) };
    jappend(b, "{\"version\":\"%s\",\"source\":\"%s\",\"crash_class\":\"%s\",",
            PY4GW_VERSION, source, exception_label(code));
    jappend(b, "\"exception_code\":\"0x%08lX\",\"fault_address\":\"0x%08lX\",\"faulting_tid\":%lu,",
            static_cast<unsigned long>(code), static_cast<unsigned long>(addr), GetCurrentThreadId());
    jappend(b, "\"dump_file\":\"%s\",", edmp);
    if (egwt[0]) jappend(b, "\"gw_text_file\":\"%s\",", egwt);
    jappend(b, "\"python_last_frame\":{\"file\":\"%s\",\"line\":%d,\"func\":\"%s\"},",
            efile, f.line, efunc);
    jappend(b, "\"assert\":\"%s\",\"gw_text\":\"%s\",", eassert, egw);
    jappend(b, "\"breadcrumbs\":[");
    bool first = true;
    bc::drain_recent(16, [&](const char* m) {
        char em[256]; json_escape(em, sizeof(em), m);
        jappend(b, "%s\"%s\"", first ? "" : ",", em);
        first = false;
    });
    jappend(b, "]}\n");

    DWORD written = 0;
    WriteFile(file, buf, static_cast<DWORD>(b.p - buf), &written, nullptr);
    CloseHandle(file);
}

void CrashHandler::WriteDump(EXCEPTION_POINTERS* info, const wchar_t* dmp_path, const char* comment) {
    HANDLE file = CreateFileW(dmp_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    MINIDUMP_EXCEPTION_INFORMATION mei = {0};
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = info;
    mei.ClientPointers = FALSE;
    MINIDUMP_USER_STREAM us = {0};
    us.Type = CommentStreamA;                  // == 10 (MINIDUMP_STREAM_TYPE in dbghelp.h)
    us.BufferSize = static_cast<ULONG>(strlen(comment) + 1);
    us.Buffer = const_cast<char*>(comment);
    MINIDUMP_USER_STREAM_INFORMATION usi = { 1, &us };
    const MINIDUMP_TYPE flags = static_cast<MINIDUMP_TYPE>(0x1041);  // DataSegs|IndirectRefd|ThreadInfo
    __try {
        // dbghelp HeapAllocs internally; guard against a fault on heap-corruption crashes.
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, flags,
                          info ? &mei : nullptr, &usi, nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) { /* partial/absent dump; sidecar already written */ }
    CloseHandle(file);
}
