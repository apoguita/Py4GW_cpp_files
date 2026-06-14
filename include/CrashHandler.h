#pragma once
// Py4GW crash handler: native minidump (Paths A/B/C) + Python last-frame breadcrumb.
//
// Crash-time contract (design-safety invariants enforced in the .cpp):
//   - The SEH writer path is allocation-free, lock-free, and Python-API-free.
//   - A single re-entrancy guard serializes Paths A/B/C.
//   - Path B self-dumps via RtlCaptureContext (GWCA FatalAssert -> abort() may not reach Path A).
//   - Path C detours GW's own internal crash-message builder. Verified on the live build
//     (Gw.exe 2026-06-02, 0x00488fe0): target is __cdecl, 7 args, debug-info text @ +0x20c,
//     arg5 = CONTEXT*, Eip @ 0xB8. Resolved at runtime by string anchor (re-anchors per patch).
//   - x86 only (CONTEXT.Eip). static_assert guards the assumption.
//   - Python full stack is NOT available here; we emit a pre-captured last-frame breadcrumb
//     only. Full tracebacks come from the Python excepthooks.

#include <windows.h>
#include <cstdint>
#include <string>

// Build-time version stamp. No version macro exists in the project; override via
// target_compile_definitions(Py4GW PRIVATE PY4GW_VERSION="x.y.z").
#ifndef PY4GW_VERSION
#define PY4GW_VERSION "0.0.0-dev"
#endif

class CrashHandler {
public:
    static CrashHandler& Instance();

    // Install Paths A/B (+ C if scanner hits). Idempotent. Call AFTER GW::Initialize()
    // succeeds (dllmain.cpp:70) and BEFORE AttachRenderHook (dllmain.cpp:88).
    void Initialize();

    // Disable+remove Path C, restore SEH filter + exception policy, null panic handler.
    // Call at dllmain.cpp:131 BEFORE Py4GW::Terminate() (tears down GW Scanner/Hook). Idempotent.
    void Terminate();

    // Shared crash sink. recoverable=false (fatal A/B/C) latches the guard; recoverable=true
    // (WndProc) writes a dump then resets the guard so the game can resume. Returns true if
    // it wrote (acquired the guard), false if another fault is already being handled.
    bool OnException(EXCEPTION_POINTERS* info, const char* source, bool recoverable);

    // Optional recoverable-fault sink. NOT currently wired: SafeWndProc's __except is left as a
    // silent swallow on purpose (GW raises survivable WndProc exceptions routinely). Kept for
    // future use -- writes a dump then returns EXCEPTION_EXECUTE_HANDLER to fall through.
    LONG WndProcFilter(EXCEPTION_POINTERS* info);

    // Resolved crash directory as UTF-8. Safe at NON-crash time only (e.g. the Python
    // binding at startup). Empty until Initialize() has run. Lets the Python half write its
    // *-pytrace.txt next to the .dmp/.json so one crash = one correlated set.
    std::string CrashDirUtf8() const;

private:
    CrashHandler() = default;
    CrashHandler(const CrashHandler&) = delete;
    CrashHandler& operator=(const CrashHandler&) = delete;

    static LONG WINAPI TopLevelFilter(EXCEPTION_POINTERS* info);                 // Path A
    static void GwcaPanic(void* ctx, const char* expr, const char* file,        // Path B (5-arg)
                          unsigned int line, const char* func);
    // Path C detour over GW's internal crash-message builder (verified __cdecl, 7-arg signature).
    static uintptr_t __cdecl AppendStackDetour(void* debug_info, uint32_t a2, uint32_t a3,
                                               uint32_t a4, CONTEXT* ctx, uint32_t a6,
                                               uint32_t a7);

    void InstallPathA();
    void InstallPathB();
    void InstallPathC();
    void ClearCallbackFilterPolicy();   // clear PROCESS_CALLBACK_FILTER_ENABLED (bit 0)
    void RestoreCallbackFilterPolicy(); // restore the saved policy at teardown

    // Crash-time-safe writers (no heap, no locks, no Py API). Paths are pre-built stems.
    void WriteSidecar(EXCEPTION_POINTERS* info, const wchar_t* json_path,
                      const wchar_t* dmp_name, const wchar_t* gwtext_name, const char* source);
    void WriteDump(EXCEPTION_POINTERS* info, const wchar_t* dmp_path, const char* comment);
    bool EnsureCrashDir();              // GetCurrentDirectoryW base + "\\crashes"; init-time only
};
