#pragma once
#include "Headers.h"
#include <MinHook.h>
#include <intrin.h>
#include <condition_variable>

namespace py = pybind11;

// CreateUIComponent callback binding is intentionally disabled for now.
// It is crash-prone during interactive validation and should only be
// restored when callback-specific work is resumed.
#if 0
struct CreateUIComponentCallbackState {
    uint64_t handle = 0;
    GW::HookEntry entry;
    py::function callback;
};

inline std::mutex g_create_ui_component_callback_mutex;
inline uint64_t g_next_create_ui_component_callback_handle = 1;
inline std::unordered_map<uint64_t, std::shared_ptr<CreateUIComponentCallbackState>> g_create_ui_component_callbacks;
#endif

inline std::mutex g_created_text_label_payloads_mutex;
inline std::unordered_map<uint32_t, std::wstring> g_created_text_label_payloads;

// ============================================================================
// Shared Function Resolvers
// ============================================================================

// RATIONALE: The Ui_CreateEncodedText byte-pattern (wildcarded CALL displacements)
// matches 2 locations in every EXE build. Three namespaces (UIManagerTitleHook,
// UIManagerDialogTitle, UIManagerCNonclient) AND the SetFrameTitleByFrameId/
// AttachCompositeRootToFrame helpers all resolve the same function independently.
// This shared resolver eliminates 5 duplicate pattern scans and replaces them
// with 1 scan + 4 trivial static-cache hits.
//
// STATIC-CACHE NOTE: The `static` local variable creates one cache per
// translation unit. py_ui.h is included from py_ui.cpp only (single TU), so
// there is exactly one cache. If this header is ever included from additional
// translation units, the pattern scan will repeat once per TU — each getting
// the same correct pointer, so correctness is unaffected.
//
// ORDERING REQUIREMENT: This function calls GW::Scanner::Find, which may
// internally use MinHook or touch memory that MinHook manages. Callers
// MUST ensure MinHook is initialized BEFORE invoking this resolver.
using SharedCreateEncodedText_pt = uint32_t(__cdecl*)(uint32_t, uint32_t, const wchar_t*, uint32_t);

inline SharedCreateEncodedText_pt ResolveCreateEncodedText()
{
    static SharedCreateEncodedText_pt cached = nullptr;
    if (cached)
        return cached;

    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x51\x56\x57\xE8\x00\x00\x00\x00\x8B\x48\x18\xE8\x00\x00\x00\x00\x8B\xF8",
        "xxxxxxx????xxxx????xx");
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveCreateEncodedText — pattern not found");
        return nullptr;
    }

    // Prologue validation: the first 6 bytes MUST be 55 8B EC 51 56 57
    // (PUSH EBP; MOV EBP,ESP; PUSH ECX; PUSH ESI; PUSH EDI).
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(addr);
    if (bytes[0] != 0x55 || bytes[1] != 0x8B || bytes[2] != 0xEC ||
        bytes[3] != 0x51 || bytes[4] != 0x56 || bytes[5] != 0x57) {
        GWCA_ERR("[SCAN] ResolveCreateEncodedText — found 0x%08X but prologue validation failed "
                 "(bytes: %02X %02X %02X %02X %02X %02X)",
                 addr, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
        return nullptr;
    }

    cached = reinterpret_cast<SharedCreateEncodedText_pt>(addr);
    return cached;
}

// Forward declare UIManagerTitleHook::ResolveDevTextStringUse() so ResolveSetFrameText
// can call it before the namespace is defined. (MSVC permits unqualified forward
// references under /Zc:twoPhase-; this explicit forward declaration makes the code
// standards-conformant and portable to other compilers.)
namespace UIManagerTitleHook {
    uintptr_t ResolveDevTextStringUse();
}

struct SetFrameTextResolved {
    SharedCreateEncodedText_pt create_text_fn = nullptr;
    void(__cdecl* set_frame_text_fn)(uint32_t, uintptr_t) = nullptr;
};

// Resolves both Ui_CreateEncodedText and Ui_SetFrameText using the shared
// CreateEncodedText resolver and the DevText call-site derivation approach.
//
// The Ui_SetFrameText byte pattern matches 16 functions and cannot be used
// directly. Instead, this function resolves the "DlgDevText" string use
// address, walks forward looking for the first CALL (which targets
// Ui_CreateEncodedText), and then takes the NEXT CALL as Ui_SetFrameText —
// structurally stable across all known EXE builds.
inline bool ResolveSetFrameText(SetFrameTextResolved& out)
{
    out.create_text_fn = ResolveCreateEncodedText();
    if (!out.create_text_fn)
        return false;

    if (out.set_frame_text_fn)
        return true;

    const uintptr_t use_addr = UIManagerTitleHook::ResolveDevTextStringUse();
    if (!use_addr)
        return false;

    bool found_ct = false;
    for (uintptr_t a = use_addr; a < use_addr + 0x60; ++a) {
        const auto opcode = *reinterpret_cast<const uint8_t*>(a);
        if (opcode != 0xE8) continue;
        const int32_t rel = *reinterpret_cast<const int32_t*>(a + 1);
        const uintptr_t target = a + 5 + rel;

        if (!found_ct && target == reinterpret_cast<uintptr_t>(out.create_text_fn)) {
            found_ct = true;
            continue;
        }
        if (found_ct) {
            out.set_frame_text_fn = reinterpret_cast<void(__cdecl*)(uint32_t, uintptr_t)>(target);
            break;
        }
    }

    return out.set_frame_text_fn != nullptr;
}

// ============================================================================
// Window Contents — Shared Resolvers (2026-06-04)
// ============================================================================
// CtlFrameListCreateItem  @ EXE 0x00612900 — sends msg 0x57 to frame list
// FrameNewSubclass         @ EXE 0x0062f150 — registers subclass proc on frame
// TextLabelFrameCallback   @ EXE 0x00610c40 — CtlTextProc (GWCA assertion)
//
// Patterns verified against Gw.exe build 05-30-2026:
//   CtlFrameListCreateItem: pattern "C7 45 0C 00 00 00 00 50 6A 57 FF 75 08"
//                           → unique match at 0x00612925, offset -0x25 to entry
//   FrameNewSubclass:       pattern "8D B8 A8 00 00 00 8B CF"
//                           → unique match at 0x0062f17d, offset -0x2D to entry
//   TextLabelFrameCallback: assertion "CtlText.cpp" / "FrameTestStyles(...)"
//                           → ToFunctionStart(0xFFF)

using CtlFrameListCreateItem_pt = uint32_t(__cdecl*)(uint32_t, uint32_t, uint32_t, void*, void*);
using FrameNewSubclass_pt = uint32_t(__cdecl*)(uint32_t, void*, uint32_t);
using TextLabelFrameCallback_pt = void*(__cdecl*)(uint32_t, uint32_t, void*, void*);

inline CtlFrameListCreateItem_pt ResolveCtlFrameListCreateItem()
{
    static CtlFrameListCreateItem_pt cached = nullptr;
    if (cached)
        return cached;

    const uintptr_t addr = GW::Scanner::Find(
        "\xC7\x45\x0C\x00\x00\x00\x00\x50\x6A\x57\xFF\x75\x08",
        "xxxxxxxxxxxx", -0x25);
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveCtlFrameListCreateItem — pattern not found");
        return nullptr;
    }
    cached = reinterpret_cast<CtlFrameListCreateItem_pt>(addr);
    return cached;
}

inline FrameNewSubclass_pt ResolveFrameNewSubclass()
{
    static FrameNewSubclass_pt cached = nullptr;
    if (cached)
        return cached;

    const uintptr_t addr = GW::Scanner::Find(
        "\x8D\xB8\xA8\x00\x00\x00\x8B\xCF",
        "xxxxxxxx", -0x2D);
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveFrameNewSubclass — pattern not found");
        return nullptr;
    }
    cached = reinterpret_cast<FrameNewSubclass_pt>(addr);
    return cached;
}

inline TextLabelFrameCallback_pt ResolveTextLabelFrameCallback()
{
    static TextLabelFrameCallback_pt cached = nullptr;
    if (cached)
        return cached;

    const uintptr_t addr = GW::Scanner::FindAssertion(
        "CtlText.cpp",
        "FrameTestStyles(hdr.frameId, CTLTEXT_STYLE_MODEL)",
        0, 0);
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveTextLabelFrameCallback — assertion not found");
        return nullptr;
    }
    const uintptr_t func_start = GW::Scanner::ToFunctionStart(addr, 0xFFF);
    if (!func_start) {
        GWCA_ERR("[SCAN] ResolveTextLabelFrameCallback — ToFunctionStart failed");
        return nullptr;
    }
    cached = reinterpret_cast<TextLabelFrameCallback_pt>(func_start);
    return cached;
}

// Clone-time title overrides need to intercept the same native title path that
// DevText uses when Guild Wars builds a composite window. The game can attach:
// 1. a dynamic encoded text payload via Ui_SetFrameText, and
// 2. a resource-backed caption via Ui_SetFrameEncodedTextResource.
// Suppressing only the text path leaves the original resource caption visible.

// ============================================================================
// UIManagerTitleHook — DevText Clone Title Overrides (Vector B)
// ============================================================================
namespace UIManagerTitleHook {
    using UiSetFrameText_pt = void(__cdecl*)(uint32_t frame, uint32_t text_resource_or_string);
    using UiSetFrameEncodedTextResource_pt = void(__cdecl*)(uint32_t frame, uint32_t resource_ptr);
    using UiCreateEncodedText_pt = uint32_t(__cdecl*)(uint32_t style_id, uint32_t layout_profile, const wchar_t* wide_text, uint32_t reserved);

    struct PendingTitleOverride {
        uint32_t parent_frame_id = 0;
        uint32_t child_index = 0;
        std::wstring title;
    };

    inline std::mutex g_window_title_hook_mutex;
    inline std::vector<PendingTitleOverride> g_pending_title_overrides;
    inline std::wstring g_next_created_window_title;
    inline std::wstring g_last_applied_title;
    inline uint32_t g_last_applied_frame_id = 0;
    inline bool g_hook_installed = false;
    inline bool g_expect_next_title_set = false;
    inline bool g_expect_next_title_resource_set = false;

    inline UiSetFrameText_pt UiSetFrameText_Func = nullptr;
    inline UiSetFrameText_pt UiSetFrameText_Ret = nullptr;
    inline UiSetFrameEncodedTextResource_pt UiSetFrameEncodedTextResource_Func = nullptr;
    inline UiSetFrameEncodedTextResource_pt UiSetFrameEncodedTextResource_Ret = nullptr;
    inline UiCreateEncodedText_pt UiCreateEncodedText_Func = nullptr;
    inline UiCreateEncodedText_pt UiCreateEncodedText_Ret = nullptr;

    inline uintptr_t g_devtext_title_create_return = 0;
    inline uintptr_t g_devtext_title_set_return = 0;
    inline uintptr_t g_devtext_title_resource_set_return = 0;

    inline uintptr_t ResolveDevTextStringUse()
    {
        for (uint32_t xref_index = 0; xref_index < 8; ++xref_index) {
            uintptr_t use_addr = 0;
            try {
                use_addr = GW::Scanner::FindNthUseOfString(L"DlgDevText", xref_index, 0, GW::ScannerSection::Section_TEXT);
            }
            catch (...) {
                use_addr = 0;
            }
            if (use_addr)
                return use_addr;
        }
        return 0;
    }

    inline uintptr_t ResolveRelativeCallTarget(uintptr_t call_addr)
    {
        const auto opcode = *reinterpret_cast<const uint8_t*>(call_addr);
        if (opcode != 0xE8)
            return 0;
        const int32_t rel = *reinterpret_cast<const int32_t*>(call_addr + 1);
        return call_addr + 5 + rel;
    }

    inline bool ResolveSupportFunctions()
    {
        // Step 1: Resolve Ui_CreateEncodedText via shared resolver (deduplicated).
        if (!UiCreateEncodedText_Func) {
            UiCreateEncodedText_Func = reinterpret_cast<UiCreateEncodedText_pt>(ResolveCreateEncodedText());
            if (!UiCreateEncodedText_Func)
                return false;
        }

        // Step 2: Derive Ui_SetFrameText from DevText's call site.
        // The Ui_SetFrameText byte pattern matches 16 functions — unusable.
        // Instead, scan from the "DlgDevText" string: the FIRST CALL after
        // Ui_CreateEncodedText is Ui_SetFrameText (structurally stable across builds).
        if (!UiSetFrameText_Func || !g_devtext_title_create_return || !g_devtext_title_set_return) {
            const uintptr_t use_addr = ResolveDevTextStringUse();
            if (!use_addr)
                return false;

            bool found_create = false;
            for (uintptr_t addr = use_addr; addr < use_addr + 0x60; ++addr) {
                const uintptr_t target = ResolveRelativeCallTarget(addr);
                if (!target)
                    continue;

                if (!found_create && target == reinterpret_cast<uintptr_t>(UiCreateEncodedText_Func)) {
                    g_devtext_title_create_return = addr + 5;
                    found_create = true;
                    continue;
                }

                // The NEXT CALL after Ui_CreateEncodedText is Ui_SetFrameText
                if (found_create && !UiSetFrameText_Func) {
                    UiSetFrameText_Func = reinterpret_cast<UiSetFrameText_pt>(target);
                    g_devtext_title_set_return = addr + 5;
                    break;
                }
            }

            if (!UiSetFrameText_Func || !g_devtext_title_create_return || !g_devtext_title_set_return)
                return false;
        }

        // Step 3: Derive Ui_SetFrameEncodedTextResource from Ui_SetFrameText + 0x70.
        // Stable offset across Symbols EXE and 05-30-2026 EXE.
        if (!UiSetFrameEncodedTextResource_Func) {
            constexpr int ENCODED_TEXT_RESOURCE_OFFSET = 0x70;
            const uintptr_t candidate = reinterpret_cast<uintptr_t>(UiSetFrameText_Func) + ENCODED_TEXT_RESOURCE_OFFSET;
            UiSetFrameEncodedTextResource_Func = reinterpret_cast<UiSetFrameEncodedTextResource_pt>(candidate);
        }

        // g_devtext_title_resource_set_return is NOT required — Ui_SetFrameEncodedTextResource
        // is not called from DevText's OnCreate. The hook still works without it
        // (just won't suppress resource captions during clone creation).

        return UiSetFrameText_Func &&
            UiSetFrameEncodedTextResource_Func &&
            UiCreateEncodedText_Func &&
            g_devtext_title_create_return != 0 &&
            g_devtext_title_set_return != 0;
    }

    inline int FindPendingOverrideIndex(uint32_t parent_frame_id, uint32_t child_index)
    {
        for (size_t i = 0; i < g_pending_title_overrides.size(); ++i) {
            const auto& pending = g_pending_title_overrides[i];
            if (pending.parent_frame_id == parent_frame_id && pending.child_index == child_index)
                return static_cast<int>(i);
        }
        return -1;
    }

    inline uint32_t __cdecl OnUiCreateEncodedText(uint32_t style_id, uint32_t layout_profile, const wchar_t* wide_text, uint32_t reserved)
    {
        const uintptr_t return_address = reinterpret_cast<uintptr_t>(_ReturnAddress());
        std::wstring replacement_title;

        if (return_address == g_devtext_title_create_return) {
            std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
            if (!g_pending_title_overrides.empty()) {
                replacement_title = g_pending_title_overrides.front().title;
                g_pending_title_overrides.erase(g_pending_title_overrides.begin());
                g_last_applied_title = replacement_title;
                g_expect_next_title_set = true;
                g_expect_next_title_resource_set = true;
            }
        }

        if (!replacement_title.empty() && UiCreateEncodedText_Ret)
            return UiCreateEncodedText_Ret(style_id, layout_profile, replacement_title.c_str(), reserved);

        if (UiCreateEncodedText_Ret)
            return UiCreateEncodedText_Ret(style_id, layout_profile, wide_text, reserved);
        return 0;
    }

    inline void __cdecl OnUiSetFrameText(uint32_t frame, uint32_t text_resource_or_string)
    {
        const uintptr_t return_address = reinterpret_cast<uintptr_t>(_ReturnAddress());
        if (return_address == g_devtext_title_set_return) {
            std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
            if (g_expect_next_title_set) {
                g_last_applied_frame_id = frame;
                g_expect_next_title_set = false;
            }
        }

        if (UiSetFrameText_Ret)
            UiSetFrameText_Ret(frame, text_resource_or_string);
    }

    inline void __cdecl OnUiSetFrameEncodedTextResource(uint32_t frame, uint32_t resource_ptr)
    {
        const uintptr_t return_address = reinterpret_cast<uintptr_t>(_ReturnAddress());
        if (return_address == g_devtext_title_resource_set_return) {
            std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
            if (g_expect_next_title_resource_set) {
                g_last_applied_frame_id = frame;
                g_expect_next_title_resource_set = false;
                return;
            }
        }

        if (UiSetFrameEncodedTextResource_Ret)
            UiSetFrameEncodedTextResource_Ret(frame, resource_ptr);
    }

    inline bool EnsureInstalled()
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        if (g_hook_installed)
            return true;
        if (!ResolveSupportFunctions())
            return false;

        int success = GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&UiCreateEncodedText_Func),
            reinterpret_cast<void*>(OnUiCreateEncodedText),
            reinterpret_cast<void**>(&UiCreateEncodedText_Ret));
        if (success != MH_OK)
            return false;

        success = GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&UiSetFrameText_Func),
            reinterpret_cast<void*>(OnUiSetFrameText),
            reinterpret_cast<void**>(&UiSetFrameText_Ret));
        if (success != MH_OK)
            return false;

        success = GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&UiSetFrameEncodedTextResource_Func),
            reinterpret_cast<void*>(OnUiSetFrameEncodedTextResource),
            reinterpret_cast<void**>(&UiSetFrameEncodedTextResource_Ret));
        if (success != MH_OK)
            return false;

        GW::HookBase::EnableHooks(UiCreateEncodedText_Func);
        GW::HookBase::EnableHooks(UiSetFrameText_Func);
        GW::HookBase::EnableHooks(UiSetFrameEncodedTextResource_Func);
        g_hook_installed = true;
        return true;
    }

    inline bool SetNextCreatedWindowTitle(const std::wstring& title)
    {
        if (title.empty())
            return false;
        if (!EnsureInstalled())
            return false;

        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        g_next_created_window_title = title;
        return true;
    }

    inline void ClearNextCreatedWindowTitle()
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        g_next_created_window_title.clear();
        g_pending_title_overrides.clear();
        g_expect_next_title_set = false;
        g_expect_next_title_resource_set = false;
    }

    inline bool HasNextCreatedWindowTitle()
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        return !g_next_created_window_title.empty();
    }

    inline void ArmNextCreatedWindowTitle(uint32_t parent_frame_id, uint32_t child_index)
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        if (g_next_created_window_title.empty())
            return;
        g_pending_title_overrides.push_back({ parent_frame_id, child_index, g_next_created_window_title });
        g_next_created_window_title.clear();
    }

    inline void CancelArmedWindowTitle(uint32_t parent_frame_id, uint32_t child_index)
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        const int pending_index = FindPendingOverrideIndex(parent_frame_id, child_index);
        if (pending_index >= 0)
            g_pending_title_overrides.erase(g_pending_title_overrides.begin() + pending_index);
    }

    inline uint32_t GetLastAppliedFrameId()
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        return g_last_applied_frame_id;
    }

    inline std::wstring GetLastAppliedTitle()
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        return g_last_applied_title;
    }

    inline bool IsInstalled()
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        return g_hook_installed;
    }
}

// ============================================================================
// UIManagerDialogTitle — Dialog Descriptor Table Hijack (Vector A)
// ============================================================================
// Strategy B (Recommended): Hook Ui_CreateEncodedTextFromStringId (18-byte thunk)
// to intercept the title resource ID lookup during DialogShow creation.
// When DialogShow reads entry 7 (title ID 0x337), the hook returns custom encoded
// text instead of the resource-based title. This uses the native Path A title
// rendering chain: TextEncode → FrameSetTitle → CNonclient::SetTitle →
// OnTitleResolved → CContent::Invalidate.
//
// Dialog descriptor table layout (9 DWORDs = 0x24 per entry):
//   +0x00: FrameProc pointer
//   +0x04: Label string
//   +0x08: Subclass flags
//   +0x0C: FrameCreate flags
//   +0x10: Bitflags
//   +0x14: Title string resource ID  ← hook intercepts this lookup
//   +0x18: Hotkey key enum
//   +0x1C: Unknown
//   +0x20: PrefWindow enum
//
// Entry 7 (05-30-2026):
//   FrameProc = FUN_004FF0E0 (simple container, msg 4 + msg 9 only)
//   Title ID = 0x337 (valid, renders title bar)
//   Subclass = 0x159 (titled + closeable + resizable + chrome)
//   No callers → safe to hijack without conflicts
namespace UIManagerDialogTitle {
    using DialogShow_pt = uint32_t(__cdecl*)(uint32_t parent, uint32_t dialog_enum, int32_t create_flag, void* param4);
    using CreateEncodedTextFromStrId_pt = uintptr_t(__cdecl*)(uint32_t string_id);
    using CreateEncodedText_pt = uintptr_t(__cdecl*)(uint32_t style_id, uint32_t layout_profile, const wchar_t* wide_text, uint32_t reserved);

    static DialogShow_pt DialogShow_Func = nullptr;
    static CreateEncodedTextFromStrId_pt CreateEncodedTextFromStrId_Func = nullptr;
    static CreateEncodedTextFromStrId_pt CreateEncodedTextFromStrId_Ret = nullptr;
    static CreateEncodedText_pt CreateEncodedText_Func = nullptr;

    // Target dialog: entry 8, title resource ID 0x2C2
    // Entry 7 (FrameProc FUN_004FF0E0) crashed — msg 4 writes to null param_2[3].
    // Entry 8 uses standard FrameProc 0x008A1380 with subclass 0x159.
    static constexpr uint32_t TARGET_DIALOG_ENUM = 8;
    static constexpr uint32_t TARGET_DIALOG_TITLE_ID = 0x2C2;

    // State — accessed only from the game thread (single-threaded)
    static std::wstring g_dialog_custom_title;
    static bool g_dialog_hook_active = false;
    static bool g_hook_installed = false;

    // Resolves all three function pointers via byte-pattern scanning.
    // Patterns verified unique (single match) in 05-30-2026 EXE via Ghidra MCP.
    //
    // DialogShow:    55 8B EC 53 56 57 8B 7D 0C 8D 34 FF C1 E6 02 83 FF 3A 72 2C
    //                20 bytes, all exact — uniquely matches 0x004e1210
    //
    // CreateEncodedTextFromStrId (thunk):
    //                55 8B EC 6A 00 FF 75 08 E8 ?? ?? ?? ?? 83 C4 08 5D C3
    //                18 bytes, wildcarded CALL displacement — uniquely matches 0x007c3bc0
    //
    // CreateEncodedText (raw encoder):
    //                Uses shared ResolveCreateEncodedText() — see Shared Function Resolvers above.
    inline bool ResolveFunctions()
    {
        if (!DialogShow_Func) {
            const uintptr_t addr = GW::Scanner::Find(
                "\x55\x8B\xEC\x53\x56\x57\x8B\x7D\x0C\x8D\x34\xFF\xC1\xE6\x02\x83\xFF\x3A\x72\x2C",
                "xxxxxxxxxxxxxxxxxxxx");
            if (!addr)
                return false;
            DialogShow_Func = reinterpret_cast<DialogShow_pt>(addr);
        }

        if (!CreateEncodedTextFromStrId_Func) {
            const uintptr_t addr = GW::Scanner::Find(
                "\x55\x8B\xEC\x6A\x00\xFF\x75\x08\xE8\x00\x00\x00\x00\x83\xC4\x08\x5D\xC3",
                "xxxxxxxx????xxxxx");
            if (!addr)
                return false;
            CreateEncodedTextFromStrId_Func = reinterpret_cast<CreateEncodedTextFromStrId_pt>(addr);
        }

        if (!CreateEncodedText_Func) {
            CreateEncodedText_Func = reinterpret_cast<CreateEncodedText_pt>(ResolveCreateEncodedText());
            if (!CreateEncodedText_Func)
                return false;
        }

        return DialogShow_Func && CreateEncodedTextFromStrId_Func && CreateEncodedText_Func;
    }

    // MinHook handler — intercepts Ui_CreateEncodedTextFromStringId(uint32_t string_id).
    // When the hook is active and string_id matches the target dialog title ID (0x337),
    // encodes the custom title via Ui_CreateEncodedText(8, 7, title, 0) and returns it.
    // Otherwise passes through to the original thunk (which calls the raw encoder with
    // the resource ID).
    inline uintptr_t __cdecl OnCreateEncodedTextFromStrId(uint32_t string_id)
    {
        if (g_dialog_hook_active && string_id == TARGET_DIALOG_TITLE_ID && !g_dialog_custom_title.empty() && CreateEncodedText_Func) {
            const std::wstring title = g_dialog_custom_title;
            g_dialog_custom_title.clear();
            g_dialog_hook_active = false;
            return CreateEncodedText_Func(8, 7, title.c_str(), 0);
        }
        if (CreateEncodedTextFromStrId_Ret)
            return CreateEncodedTextFromStrId_Ret(string_id);
        return 0;
    }

    // Installs the MinHook on Ui_CreateEncodedTextFromStringId if not already installed.
    // Returns true if the hook is active (or was already active).
    inline bool EnsureInstalled()
    {
        if (g_hook_installed)
            return true;
        if (!ResolveFunctions())
            return false;

        int success = GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&CreateEncodedTextFromStrId_Func),
            reinterpret_cast<void*>(OnCreateEncodedTextFromStrId),
            reinterpret_cast<void**>(&CreateEncodedTextFromStrId_Ret));
        if (success != MH_OK)
            return false;

        GW::HookBase::EnableHooks(CreateEncodedTextFromStrId_Func);
        g_hook_installed = true;
        return true;
    }

    // Creates a native floating dialog window with a custom title via the dialog
    // descriptor table hijack approach.
    //
    // MUST be called from the game thread. The hook intercepts the title resource ID
    // lookup inside DialogShow and substitutes the custom encoded text.
    //
    // Parameters:
    //   parent  — parent frame ID (0 = root, 9 = game root container)
    //   title   — wide-string custom title text
    // Returns: frame_id of the created dialog window, or 0 on failure.
    inline uint32_t CreateDialogWithTitle(uint32_t parent, const std::wstring& title)
    {
        if (title.empty())
            return 0;
        if (!EnsureInstalled())
            return 0;

        g_dialog_custom_title = title;
        g_dialog_hook_active = true;

        const uint32_t frame_id = DialogShow_Func(parent, TARGET_DIALOG_ENUM, 1, nullptr);

        // Clean up in case the hook didn't fire (dialog creation may have failed)
        g_dialog_hook_active = false;
        g_dialog_custom_title.clear();

        return frame_id;
    }

    // Reports whether the hook has been installed successfully.
    inline bool IsInstalled()
    {
        return g_hook_installed;
    }
}

template <typename T>
// Copies a GWCA dynamic array into a std::vector for Python-friendly return values.
std::vector<T> ConvertArrayToVector(const GW::Array<T>& arr) {
    return std::vector<T>(arr.begin(), arr.end());
}


// Wrapper for function pointers
struct UIInteractionCallbackWrapper {
    uintptr_t callback_address;  // Store function pointer as an integer
    uintptr_t uictl_context;
    uint32_t h0008;

    UIInteractionCallbackWrapper(GW::UI::UIInteractionCallback callback)
        : callback_address(reinterpret_cast<uintptr_t>(callback)),
          uictl_context(0),
          h0008(0) {
    }

    UIInteractionCallbackWrapper(const GW::UI::FrameInteractionCallback& callback)
        : callback_address(reinterpret_cast<uintptr_t>(callback.callback)),
          uictl_context(reinterpret_cast<uintptr_t>(callback.uictl_context)),
          h0008(callback.h0008) {
    }

    uintptr_t get_address() const { return callback_address; }
};

struct FramePositionWrapper {
    uint32_t top;
    uint32_t left;
    uint32_t bottom;
    uint32_t right;

    uint32_t content_top;
    uint32_t content_left;
    uint32_t content_bottom;
    uint32_t content_right;

    float unknown;
    float scale_factor;
    float viewport_width;
    float viewport_height;

    float screen_top;
    float screen_left;
    float screen_bottom;
    float screen_right;

    uint32_t top_on_screen;
    uint32_t left_on_screen;
    uint32_t bottom_on_screen;
    uint32_t right_on_screen;

	uint32_t width_on_screen;
	uint32_t height_on_screen;

    float viewport_scale_x;
    float viewport_scale_y;
};

// Coord2f and Rect4f match the native game types used by FrameSetPosition,
// FrameSetSize, and FrameGetClientBorder.  Coord2f is 8 bytes ({float x; float y});
// Rect4f is 16 bytes ({float top; float left; float right; float bottom}).
struct Coord2f {
    float x;
    float y;
};

struct Rect4f {
    float top;
    float left;
    float right;
    float bottom;
};

struct FrameRelationWrapper {
	uint32_t parent_id;
	uint32_t field67_0x124;
	uint32_t field68_0x128;
	uint32_t frame_hash_id;
    std::vector<uint32_t> siblings;
};

class UIFrame {
public:
    bool is_created;
    bool is_visible;

	uint32_t frame_id;
	uint32_t parent_id;
	uint32_t frame_hash;
    uint32_t field1_0x0;
    uint32_t field2_0x4;
    uint32_t frame_layout;
    uint32_t field3_0xc;
    uint32_t field4_0x10;
    uint32_t field5_0x14;
    uint32_t visibility_flags;
    uint32_t field7_0x1c;
    uint32_t type;
    uint32_t template_type;
    uint32_t field10_0x28;
    uint32_t field11_0x2c;
    uint32_t field12_0x30;
    uint32_t field13_0x34;
    uint32_t field14_0x38;
    uint32_t field15_0x3c;
    uint32_t field16_0x40;
    uint32_t field17_0x44;
    uint32_t field18_0x48;
    uint32_t field19_0x4c;
    uint32_t field20_0x50;
    uint32_t field21_0x54;
    uint32_t field22_0x58;
    uint32_t field23_0x5c;
    uint32_t field24_0x60;
    uint32_t field24a_0x64;
    uint32_t field24b_0x68;
    uint32_t field25_0x6c;
    uint32_t field26_0x70;
    uint32_t field27_0x74;
    uint32_t field28_0x78;
    uint32_t field29_0x7c;
    uint32_t field30_0x80;
    std::vector<uintptr_t> field31_0x84;
    uint32_t field32_0x94;
    uint32_t field33_0x98;
    uint32_t field34_0x9c;
    uint32_t field35_0xa0;
    uint32_t field36_0xa4;
    std::vector<UIInteractionCallbackWrapper> frame_callbacks;
    uint32_t child_offset_id;
    uint32_t field40_0xc0;
    uint32_t field41_0xc4;
    uint32_t field42_0xc8;
    uint32_t field43_0xcc;
    uint32_t field44_0xd0;
    uint32_t field45_0xd4;
	FramePositionWrapper position;
    uint32_t field63_0x11c;
    uint32_t field64_0x120;
    uint32_t field65_0x124;
    FrameRelationWrapper relation;
    uint32_t field73_0x144;
    uint32_t field74_0x148;
    uint32_t field75_0x14c;
    uint32_t field76_0x150;
    uint32_t field77_0x154;
    uint32_t field78_0x158;
    uint32_t field79_0x15c;
    uint32_t field80_0x160;
    uint32_t field81_0x164;
    uint32_t field82_0x168;
    uint32_t field83_0x16c;
    uint32_t field84_0x170;
    uint32_t field85_0x174;
    uint32_t field86_0x178;
    uint32_t field87_0x17c;
    uint32_t field88_0x180;
    uint32_t field89_0x184;
    uint32_t field90_0x188;
    uint32_t frame_state;
    uint32_t field92_0x190;
    uint32_t field93_0x194;
    uint32_t field94_0x198;
    uint32_t field95_0x19c;
    uint32_t field96_0x1a0;
    uint32_t field97_0x1a4;
    uint32_t field98_0x1a8;
    //TooltipInfo* tooltip_info;
    uint32_t field100_0x1b0;
    uint32_t field101_0x1b4;
    uint32_t field102_0x1b8;
    uint32_t field103_0x1bc;
    uint32_t field104_0x1c0;
    uint32_t field105_0x1c4;

    UIFrame(int pframe_id) {
		frame_id = pframe_id;
        GetContext();
    }
    // Refreshes the cached field snapshot from the live native frame.
	void GetContext();
};

class UIManager {
public:
    // UIManager is the native bridge layer exposed to Python. It groups:
    // frame discovery, UI message dispatch, reverse-engineered construction
    // helpers, and a small amount of instrumentation used while validating
    // native window behavior.
// Returns the current UI language used by the client text subsystem.
    static uint32_t GetTextLanguage() {
        return static_cast<uint32_t>(GW::UI::GetTextLanguage());
    }

    // Returns the recorded frame-log entries collected by GWCA instrumentation.
	static std::vector<std::tuple<uint64_t, uint32_t, std::string>> GetFrameLogs() {
		return GW::UI::GetFrameLogs();
	}

    // Clears the buffered frame-log entries.
	static void ClearFrameLogs() {
		GW::UI::ClearFrameLogs();
	}

    // Returns the buffered raw UI message payload log.
	static std::vector<std::tuple<
        uint64_t,               // tick
        uint32_t,               // msgid
        bool,                   // incoming
        bool,                   // is_frame_message
        uint32_t,               // frame_id
        std::vector<uint8_t>,   // w_bytes
        std::vector<uint8_t>    // l_bytes
        >> GetUIPayloads() {
		return GW::UI::GetUIPayloads();
	}

    // Clears the buffered raw UI message payload log.
	static void ClearUIPayloads() {
		GW::UI::ClearUIPayloads();
	}
	
    // Resolves a frame id from a string label.
    static uint32_t GetFrameIDByLabel(const std::string& label) {
        std::wstring wlabel(label.begin(), label.end()); // Convert to wide string
        return GW::UI::GetFrameIDByLabel(wlabel.c_str());
    }

    // Resolves a frame id from a frame hash.
    static uint32_t GetFrameIDByHash(uint32_t hash) {
        return GW::UI::GetFrameIDByHash(hash);
    }

    // Returns a direct child frame id from a parent frame id and child offset.
    static uint32_t GetChildFrameByFrameId(uint32_t parent_frame_id, uint32_t child_offset) {
        GW::UI::Frame* parent = GW::UI::GetFrameById(parent_frame_id);
        if (!parent)
            return 0;
        GW::UI::Frame* child = GW::UI::GetChildFrame(parent, child_offset);
        return child ? child->frame_id : 0;
    }

    // Walks a child-offset path and returns the resulting descendant frame id.
    static uint32_t GetChildFramePathByFrameId(
        uint32_t parent_frame_id,
        const std::vector<uint32_t>& child_offsets)
    {
        GW::UI::Frame* current = GW::UI::GetFrameById(parent_frame_id);
        if (!current)
            return 0;
        for (uint32_t child_offset : child_offsets) {
            current = GW::UI::GetChildFrame(current, child_offset);
            if (!current)
                return 0;
        }
        return current->frame_id;
    }

    // Returns the parent frame id for a given frame.
    static uint32_t GetParentFrameID(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame)
            return 0;
        GW::UI::Frame* parent = GW::UI::GetParentFrame(frame);
        return parent ? parent->frame_id : 0;
    }

    // Returns the native UI control context pointer for a frame.
    static uintptr_t GetFrameContext(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame)
            return 0;
        return reinterpret_cast<uintptr_t>(GW::UI::GetFrameContext(frame));
    }

    // Returns all direct child frame ids ordered by child offset.
    static std::vector<uint32_t> GetChildFrameIDs(uint32_t parent_frame_id) {
        std::vector<std::pair<uint32_t, uint32_t>> ordered;
        for (const auto frame_id : GW::UI::GetFrameArray()) {
            GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
            if (!frame)
                continue;
            GW::UI::Frame* parent = GW::UI::GetParentFrame(frame);
            if (!(parent && parent->frame_id == parent_frame_id))
                continue;
            ordered.emplace_back(frame->child_offset_id, frame->frame_id);
        }
        std::sort(ordered.begin(), ordered.end(),
            [](const auto& lhs, const auto& rhs) {
                if (lhs.first != rhs.first)
                    return lhs.first < rhs.first;
                return lhs.second < rhs.second;
            });
        std::vector<uint32_t> result;
        result.reserve(ordered.size());
        for (const auto& entry : ordered)
            result.push_back(entry.second);
        return result;
    }

    // Returns the first ordered child frame id for a parent.
    static uint32_t GetFirstChildFrameID(uint32_t parent_frame_id) {
        const auto children = GetChildFrameIDs(parent_frame_id);
        return children.empty() ? 0 : children.front();
    }

    // Returns the last ordered child frame id for a parent.
    static uint32_t GetLastChildFrameID(uint32_t parent_frame_id) {
        const auto children = GetChildFrameIDs(parent_frame_id);
        return children.empty() ? 0 : children.back();
    }

    // Returns the next sibling frame id in child-offset order.
    static uint32_t GetNextChildFrameID(uint32_t frame_id) {
        const uint32_t parent_frame_id = GetParentFrameID(frame_id);
        if (!parent_frame_id)
            return 0;
        const auto children = GetChildFrameIDs(parent_frame_id);
        for (size_t i = 0; i < children.size(); ++i) {
            if (children[i] == frame_id)
                return i + 1 < children.size() ? children[i + 1] : 0;
        }
        return 0;
    }

    // Returns the previous sibling frame id in child-offset order.
    static uint32_t GetPrevChildFrameID(uint32_t frame_id) {
        const uint32_t parent_frame_id = GetParentFrameID(frame_id);
        if (!parent_frame_id)
            return 0;
        const auto children = GetChildFrameIDs(parent_frame_id);
        for (size_t i = 0; i < children.size(); ++i) {
            if (children[i] == frame_id)
                return i > 0 ? children[i - 1] : 0;
        }
        return 0;
    }

    // Returns the Nth ordered child frame id under a parent.
    static uint32_t GetItemFrameID(uint32_t parent_frame_id, uint32_t index) {
        const auto children = GetChildFrameIDs(parent_frame_id);
        return index < children.size() ? children[index] : 0;
    }

    // Returns the Nth tab frame id under a parent.
    static uint32_t GetTabFrameID(uint32_t parent_frame_id, uint32_t index) {
        return GetItemFrameID(parent_frame_id, index);
    }

    // Traverses the frame tree using the native sibling/child relation walker.
    // Public relation_kind semantics:
    //   0 = first child of frame_id
    //   1 = last child of frame_id
    //   2 = next sibling of frame_id
    //   3 = previous sibling of frame_id
    // start_after: optional frame id to resume enumeration from (0 = start from beginning).
    static uint32_t GetRelatedFrameID(uint32_t frame_id, uint32_t relation_kind, uint32_t start_after = 0) {
        GW::UI::Frame* result = GW::UI::GetRelatedFrameById(
            frame_id,
            static_cast<GW::UI::FrameChild>(relation_kind),
            start_after);
        return result ? result->frame_id : 0;
    }

    // ── Frame property accessors ──

    // Reads the frame's z-layer value (field10_0x28 in Frame struct).
    static uint32_t GetFrameLayerByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        return GW::UI::GetFrameLayer(frame);
    }

    // Sets the frame's z-layer value (field10_0x28 in Frame struct).
    static bool SetFrameLayerByFrameId(uint32_t frame_id, uint32_t layer) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        return GW::UI::SetFrameLayer(frame, layer);
    }

    // Checks whether ancestor_id is an ancestor of frame_id by walking the parent chain.
    static bool IsAncestorOfByFrameId(uint32_t frame_id, uint32_t ancestor_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        GW::UI::Frame* ancestor = GW::UI::GetFrameById(ancestor_id);
        return GW::UI::IsAncestorOf(ancestor, frame);
    }

    // Returns the frame's runtime identifier code (same as frame_id).
    static uint32_t GetFrameCodeByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        return GW::UI::GetFrameCode(frame);
    }

    // Gets the minimum size the frame controller reports.
    static py::tuple GetFrameMinSizeByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        float w = 0, h = 0;
        if (GW::UI::GetFrameMinSize(frame, &w, &h))
            return py::make_tuple(w, h);
        return py::make_tuple(0.0f, 0.0f);
    }

    // Gets the frame's client border inset (left, top, right, bottom).
    static py::tuple GetFrameClientBorderByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        float l = 0, t = 0, r = 0, b = 0;
        if (GW::UI::GetFrameClientBorder(frame, &l, &t, &r, &b))
            return py::make_tuple(l, t, r, b);
        return py::make_tuple(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // Gets the frame's clip rectangle (left, top, right, bottom).
    static py::tuple GetFrameClipRectByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        float l = 0, t = 0, r = 0, b = 0;
        if (GW::UI::GetFrameClipRect(frame, &l, &t, &r, &b))
            return py::make_tuple(l, t, r, b);
        return py::make_tuple(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // Gets the raw frame position (x, y, width, height, flags) from the native FrApi function.
    static py::tuple GetFramePositionExByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        float x = 0, y = 0, w = 0, h = 0; uint32_t flags = 0;
        if (GW::UI::GetFramePositionEx(frame, &x, &y, &w, &h, &flags))
            return py::make_tuple(x, y, w, h, flags);
        return py::make_tuple(0.0f, 0.0f, 0.0f, 0.0f, 0u);
    }

    // Gets the frame's encoded title text (resource caption).
    static std::wstring GetFrameTitleByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame) return std::wstring();   // guard: null frame (was missing)
        const wchar_t* title = GW::UI::GetFrameTitle(frame);
        return title ? std::wstring(title) : std::wstring();
    }

    // Gets the frame's native/computed outer size.
    static py::tuple GetFrameNativeSizeByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        float w = 0, h = 0;
        if (GW::UI::GetFrameNativeSize(frame, &w, &h))
            return py::make_tuple(w, h);
        return py::make_tuple(0.0f, 0.0f);
    }

    // Returns the hash that Guild Wars derives from a frame label.
    static uint32_t GetHashByLabel(const std::string& label) {
        return GW::UI::GetHashByLabel(label);
    }

    // Returns the observed parent/child hierarchy snapshot for all frames.
    static std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> GetFrameHierarchy() {
        return GW::UI::GetFrameHierarchy();
    }

    // Returns coordinate pairs associated with a frame hash.
    static std::vector<std::pair<uint32_t, uint32_t>> GetFrameCoordsByHash(uint32_t frame_hash) {
        return GW::UI::GetFrameCoordsByHash(frame_hash);
    }

    // Resolves a descendant frame id from a parent hash and child-offset path.
	static uint32_t GetChildFrameID(uint32_t parent_hash, std::vector<uint32_t> child_offsets) {
		return GW::UI::GetChildFrameID(parent_hash, child_offsets);
	}

    // pybind11 signature: SendUIMessagePacked(msgid, layout, values, skip_hooks=false)
    // Sends a packed UI message using up to 16 uint32 payload words.
    static bool SendUIMessage(
        uint32_t msgid,
        std::vector<uint32_t> values,
        bool skip_hooks = false
    ) {
            struct UIPayload_POD {
                uint32_t words[16]; // 64 bytes max
            };

            UIPayload_POD payload{};
            // Zero-initialized -> important

			auto size = values.size();
			for (size_t i = 0; i < size; i++) {
				if (i < 16) {
					payload.words[i] = static_cast<uint32_t>(values[i]);
				}
			}

        // Call GW
			bool result = GW::UI::SendUIMessage(static_cast<GW::UI::UIMessage> (msgid),
				&payload,
				nullptr,
				skip_hooks
			);
		return result;

    }

    // Sends a raw UI message with explicit wparam and lparam values.
    static bool SendUIMessageRaw(
        uint32_t msgid,
        uintptr_t wparam,
        uintptr_t lparam = 0,
        bool skip_hooks = false
    ) {
        return GW::UI::SendUIMessage(
            static_cast<GW::UI::UIMessage> (msgid),
            reinterpret_cast<void*>(wparam),
            reinterpret_cast<void*>(lparam),
            skip_hooks
        );
    }

    // Sends a frame-targeted UI message to an existing frame.
    static bool SendFrameUIMessage(uint32_t frame_id, uint32_t message_id,
                                   uintptr_t wparam, uintptr_t lparam = 0) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame) return false;
        return GW::UI::SendFrameUIMessage(
            frame, static_cast<GW::UI::UIMessage>(message_id),
            reinterpret_cast<void*>(wparam), reinterpret_cast<void*>(lparam));
    }

    // Sends a frame-targeted UI message whose wparam is a temporary wide string payload.
    static bool SendFrameUIMessageWString(uint32_t frame_id, uint32_t message_id, const std::wstring& text) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame) return false;
        static std::mutex g_frame_message_text_mutex;
        static std::unordered_map<uint32_t, std::wstring> g_frame_message_text_payloads;
        wchar_t* payload = nullptr;
        if (!text.empty()) {
            std::lock_guard<std::mutex> lock(g_frame_message_text_mutex);
            g_frame_message_text_payloads[frame_id] = text;
            payload = const_cast<wchar_t*>(g_frame_message_text_payloads[frame_id].c_str());
        }
        return GW::UI::SendFrameUIMessage(
            frame,
            static_cast<GW::UI::UIMessage>(message_id),
            payload,
            nullptr);
    }

    // Creates a generic UI component with an encoded name payload.
    static uint32_t CreateUIComponentByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index,
        uintptr_t event_callback,
        const std::wstring& name_enc = L"",
        const std::wstring& component_label = L"")
    {
        GW::UI::Frame* parent = GW::UI::GetFrameById(parent_frame_id);
        if (!(parent && parent->IsCreated()))
            return 0;
        wchar_t* name_ptr = name_enc.empty() ? nullptr : const_cast<wchar_t*>(name_enc.c_str());
        wchar_t* label_ptr = component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str());
        return GW::UI::CreateUIComponent(
            parent_frame_id,
            component_flags,
            child_index,
            reinterpret_cast<GW::UI::UIInteractionCallback>(event_callback),
            name_ptr,
            label_ptr);
    }

    // Creates a generic UI component with a raw create-parameter pointer.
    static uint32_t CreateUIComponentRawByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index,
        uintptr_t event_callback,
        uintptr_t wparam = 0,
        const std::wstring& component_label = L"")
    {
        GW::UI::Frame* parent = GW::UI::GetFrameById(parent_frame_id);
        if (!(parent && parent->IsCreated()))
            return 0;
        wchar_t* label_ptr = component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str());
        return GW::UI::CreateUIComponent(
            parent_frame_id,
            component_flags,
            child_index,
            reinterpret_cast<GW::UI::UIInteractionCallback>(event_callback),
            reinterpret_cast<wchar_t*>(wparam),
            label_ptr);
    }

    // Creates a labeled frame using the low-level native create call.
    static uint32_t CreateLabeledFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t frame_flags,
        uint32_t child_index,
        uintptr_t frame_callback,
        uintptr_t create_param,
        const std::wstring& frame_label = L"")
    {
        GW::UI::Frame* parent = GW::UI::GetFrameById(parent_frame_id);
        if (!(parent && parent->IsCreated()))
            return 0;
        wchar_t* create_param_ptr = reinterpret_cast<wchar_t*>(create_param);
        wchar_t* label_ptr = frame_label.empty() ? nullptr : const_cast<wchar_t*>(frame_label.c_str());
        return GW::UI::CreateUIComponent(
            parent_frame_id,
            frame_flags,
            child_index,
            reinterpret_cast<GW::UI::UIInteractionCallback>(frame_callback),
            create_param_ptr,
            label_ptr);
    }

    // Creates a frame and immediately applies anchor margins and redraw.
    static uint32_t CreateWindowByFrameId(
        uint32_t parent_frame_id,
        uint32_t child_index,
        uintptr_t frame_callback,
        float x,
        float y,
        float width,
        float height,
        uint32_t frame_flags = 0,
        uintptr_t create_param = 0,
        const std::wstring& frame_label = L"",
        uint32_t anchor_flags = 0x6)
    {
        const uint32_t frame_id = CreateLabeledFrameByFrameId(
            parent_frame_id,
            frame_flags,
            child_index,
            frame_callback,
            create_param,
            frame_label);
        if (!frame_id)
            return 0;

        SetFrameControllerAnchorMarginsByFrameIdEx(
            frame_id,
            x,
            y,
            width,
            height,
            anchor_flags);
        TriggerFrameRedrawByFrameId(frame_id);
        return frame_id;
    }

    // Finds an unused child-offset slot under a parent frame.
    static uint32_t FindAvailableChildSlot(
        uint32_t parent_frame_id,
        uint32_t start_index = 0x20,
        uint32_t end_index = 0xFE)
    {
        if (!parent_frame_id || start_index > end_index)
            return 0;

        std::unordered_set<uint32_t> used;
        for (const auto frame_id : GW::UI::GetFrameArray()) {
            if (GetParentFrameID(frame_id) != parent_frame_id)
                continue;
            auto* frame = GW::UI::GetFrameById(frame_id);
            if (!frame)
                continue;
            used.insert(frame->child_offset_id);
        }

        for (uint32_t child_index = start_index; child_index <= end_index; ++child_index) {
            if (used.find(child_index) == used.end())
                return child_index;
        }
        return 0;
    }

    // Resolves the DevText dialog procedure by walking xrefs to the caption string.
    static uint32_t ResolveDevTextDialogProc()
    {
        static uint32_t cached_proc = 0;
        if (cached_proc)
            return cached_proc;

        for (uint32_t xref_index = 0; xref_index < 8; ++xref_index) {
            uintptr_t use_addr = 0;
            try {
                use_addr = GW::Scanner::FindNthUseOfString(L"DlgDevText", xref_index, 0, GW::ScannerSection::Section_TEXT);
            }
            catch (...) {
                use_addr = 0;
            }
            if (!use_addr)
                continue;

            const uintptr_t proc_addr = GW::Scanner::ToFunctionStart(use_addr, 0x1200);
            if (!proc_addr)
                continue;

            cached_proc = static_cast<uint32_t>(proc_addr);
            return cached_proc;
        }

        return 0;
    }

    // Resolves CContainerFrame::FrameProc via assertion anchoring.
    // Uses FindAssertion on the exact (file, message, line) tuple to avoid
    // ambiguity. ToFunctionStart walks back <=0x210 bytes.
    static uint32_t ResolveContainerFrameProc()
    {
        static uint32_t cached_proc = 0;
        if (cached_proc)
            return cached_proc;

        uintptr_t assertion_addr = 0;
        try {
            assertion_addr = GW::Scanner::FindAssertion(
                "UiPlacementContainer.cpp", "itemFrame", 0x43, 0);
        } catch (...) {
            assertion_addr = 0;
        }

        if (!assertion_addr) {
            GWCA_ERR("[SCAN] ResolveContainerFrameProc — FindAssertion failed");
            return 0;
        }

        const uintptr_t proc_addr = GW::Scanner::ToFunctionStart(assertion_addr, 0x210);

        if (!proc_addr) {
            GWCA_ERR("[SCAN] ResolveContainerFrameProc — ToFunctionStart failed");
            return 0;
        }

        cached_proc = static_cast<uint32_t>(proc_addr);
        Logger::AssertAddress("CContainerFrame::FrameProc", cached_proc, "UIModule");
        return cached_proc;
    }

    // Creates a minimal container window using CContainerFrame::FrameProc.
    // anchor_flags=0x6 = horizontal (0x2) + vertical (0x4) anchor.
    static uint32_t _create_container_window(
        float x, float y, float width, float height,
        const std::wstring& frame_label = L"",
        uint32_t parent_frame_id = 9,
        uint32_t child_index = 0,
        uint32_t frame_flags = 0,
        uintptr_t create_param = 0,
        uint32_t anchor_flags = 0x6)
    {
        const uint32_t callback = ResolveContainerFrameProc();
        if (!callback) {
            GWCA_ERR("[UI] _create_container_window — proc resolution failed");
            return 0;
        }

        const uint32_t resolved_child_index = child_index > 0
            ? child_index
            : FindAvailableChildSlot(parent_frame_id);
        if (!resolved_child_index) {
            GWCA_ERR("[UI] _create_container_window — no child slot available");
            return 0;
        }

        const uint32_t frame_id = CreateWindowByFrameId(
            parent_frame_id, resolved_child_index, callback,
            x, y, width, height,
            frame_flags, create_param, frame_label, anchor_flags);
        if (!frame_id) {
            GWCA_ERR("[UI] _create_container_window — CreateWindowByFrameId failed");
            return 0;
        }

        ProcessFrameControllerUpdateByFrameId(frame_id);
        return frame_id;
    }

    static constexpr uint32_t DEFAULT_SUBCLASS_FLAGS_COMPOSITE_ROOT = 0x59;

    // Resolves Ui_CompositeRootControlProc via two-layer strategy:
    //   1. Primary:   FindAssertion on the unique assertion inside CRProc
    //                 ("UiCtlDlg.cpp", "!s_imgList", 0, 0) — line 0 for portability
    //   2. Fallback:  Byte-pattern scan: SUB ESP,0x120 + stack cookie + register pushes
    // All paths validate the resolved address via prologue check (55 8B EC 81 EC 20 01 00 00).
    // No hardcoded addresses — all resolution is pattern/string-based at runtime.
    static uint32_t ResolveCompositeRootControlProc()
    {
        static uint32_t cached_proc = 0;
        if (cached_proc)
            return cached_proc;

        // Strategy 1: FindAssertion — most robust against EXE patches
        {
            uintptr_t addr = 0;
            try {
                addr = GW::Scanner::FindAssertion(
                    "\\Code\\Gw\\Ui\\Controls\\UiCtlDlg.cpp", "!s_imgList", 0, 0);
            } catch (...) {
                addr = 0;
            }
            if (addr) {
                const uintptr_t fn_start = GW::Scanner::ToFunctionStart(addr, 0x110);
                if (fn_start) {
                    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(fn_start);
                    if (GW::Scanner::IsValidPtr(fn_start, GW::ScannerSection::Section_TEXT) &&
                        ptr[0] == 0x55 && ptr[1] == 0x8B && ptr[2] == 0xEC &&
                        ptr[3] == 0x81 && ptr[4] == 0xEC &&
                        ptr[5] == 0x20 && ptr[6] == 0x01 &&
                        ptr[7] == 0x00 && ptr[8] == 0x00) {
                        cached_proc = static_cast<uint32_t>(fn_start);
                        Logger::AssertAddress("Ui_CompositeRootControlProc", cached_proc, "UIModule");
                        return cached_proc;
                    }
                    GWCA_ERR("[SCAN] ResolveCompositeRootControlProc — assertion resolved 0x%08X but prologue validation failed (bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X)",
                        fn_start, ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7], ptr[8]);
                }
            }
        }

        // Strategy 2: Byte-pattern scan — SUB ESP,0x120 + stack cookie + register pushes
        {
            uintptr_t proc_addr = GW::Scanner::Find(
                "\x81\xEC\x20\x01\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xFC\x8B\x45\x10\x53\x56\x8B\x75\x08",
                "xxxxxx????xxxxxxxxxxxxxx");
            if (proc_addr) {
                const uintptr_t fn_start = GW::Scanner::ToFunctionStart(proc_addr, 0x110);
                if (fn_start) {
                    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(fn_start);
                    if (GW::Scanner::IsValidPtr(fn_start, GW::ScannerSection::Section_TEXT) &&
                        ptr[0] == 0x55 && ptr[1] == 0x8B && ptr[2] == 0xEC &&
                        ptr[3] == 0x81 && ptr[4] == 0xEC &&
                        ptr[5] == 0x20 && ptr[6] == 0x01 &&
                        ptr[7] == 0x00 && ptr[8] == 0x00) {
                        cached_proc = static_cast<uint32_t>(fn_start);
                        Logger::AssertAddress("Ui_CompositeRootControlProc", cached_proc, "UIModule");
                        return cached_proc;
                    }
                    GWCA_ERR("[SCAN] ResolveCompositeRootControlProc — byte pattern resolved 0x%08X but prologue validation failed (bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X)",
                        fn_start, ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7], ptr[8]);
                }
            }
        }

        // Strategy 3: Removed — no hardcoded addresses. Resolution must use
        // FindAssertion or byte-pattern scans only.

        GWCA_ERR("[SCAN] ResolveCompositeRootControlProc — all strategies failed");
        return 0;
    }

    // Resolves FrameNewSubclass (Ui_AttachCurrentHandlerSlot) via assertion scan.
    static uint32_t ResolveFrameNewSubclass()
    {
        static uint32_t cached_proc = 0;
        if (cached_proc)
            return cached_proc;

        uintptr_t addr = 0;
        try {
            addr = GW::Scanner::FindAssertion(
                "\\Code\\Engine\\Frame\\FrApi.cpp", "frameId", 0x467, 0);
        } catch (...) {
            addr = 0;
        }
        if (addr) {
            const uintptr_t fn_start = GW::Scanner::ToFunctionStart(addr, 0x100);
            if (fn_start) {
                cached_proc = static_cast<uint32_t>(fn_start);
                Logger::AssertAddress("Ui_AttachCurrentHandlerSlot", cached_proc, "UIModule");
                return cached_proc;
            }
        }

        uintptr_t proc_addr = GW::Scanner::Find(
            "\xFF\x75\x10\x8B\xF0\x8B\xCF\xFF\x75\x0C\x56",
            "xxxxxxxxxxx");
        if (!proc_addr) {
            GWCA_ERR("[SCAN] ResolveFrameNewSubclass — both assertion and byte pattern failed");
            return 0;
        }
        const uintptr_t fn_start = GW::Scanner::ToFunctionStart(proc_addr, 0x100);
        if (!fn_start) {
            GWCA_ERR("[SCAN] ResolveFrameNewSubclass — ToFunctionStart failed");
            return 0;
        }
        cached_proc = static_cast<uint32_t>(fn_start);
        Logger::AssertAddress("Ui_AttachCurrentHandlerSlot", cached_proc, "UIModule");
        return cached_proc;
    }

    // Resolves FrameMouseEnable (WASM alias; EXE: Ui_UpdateFrameFlagMaskById).
    static uint32_t ResolveFrameMouseEnable()
    {
        static uint32_t cached_proc = 0;
        if (cached_proc)
            return cached_proc;

        uintptr_t addr = 0;
        try {
            addr = GW::Scanner::FindAssertion(
                "\\Code\\Engine\\Frame\\FrApi.cpp", "frameId", 0x540, 0);
        } catch (...) {
            addr = 0;
        }
        if (addr) {
            const uintptr_t fn_start = GW::Scanner::ToFunctionStart(addr, 0x100);
            if (fn_start) {
                cached_proc = static_cast<uint32_t>(fn_start);
                Logger::AssertAddress("FrameMouseEnable", cached_proc, "UIModule");
                return cached_proc;
            }
        }

        uintptr_t proc_addr = GW::Scanner::Find(
            "\x8D\x88\x94\x00\x00\x00\xFF\x75\x10\xFF\x75\x0C",
            "xxx???xxxxxx");
        if (!proc_addr) {
            GWCA_ERR("[SCAN] ResolveFrameMouseEnable — both assertion and byte pattern failed");
            return 0;
        }
        const uintptr_t fn_start = GW::Scanner::ToFunctionStart(proc_addr, 0x100);
        if (!fn_start) {
            GWCA_ERR("[SCAN] ResolveFrameMouseEnable — ToFunctionStart failed");
            return 0;
        }
        cached_proc = static_cast<uint32_t>(fn_start);
        Logger::AssertAddress("FrameMouseEnable", cached_proc, "UIModule");
        return cached_proc;
    }

    // ── Window Creation Pipeline Polish (2026-06-03) ──────────────────────
    // Three new resolvers derived from the FrApi.cpp assertion-anchoring
    // technique.  Each targets a unique assertion (file + "frameId" + line
    // number) inside FrApi.cpp, then walks back to the function prologue.

    // Resolves FrameSetLayer (EXE 0x0062f5a0) — sets a frame's Z-layer.
    // WASM: FrameSetLayer(unsigned int, int) @ ram:809b060f
    // Assertion: FrApi.cpp, "frameId", line 0xbfb
    static uint32_t ResolveFrameSetLayer()
    {
        static uint32_t cached_proc = 0;
        if (cached_proc)
            return cached_proc;

        uintptr_t addr = 0;
        try {
            addr = GW::Scanner::FindAssertion(
                "\\Code\\Engine\\Frame\\FrApi.cpp", "frameId", 0xbfb, 0);
        } catch (...) {
            addr = 0;
        }
        if (addr) {
            const uintptr_t fn_start = GW::Scanner::ToFunctionStart(addr, 0x100);
            if (fn_start) {
                cached_proc = static_cast<uint32_t>(fn_start);
                Logger::AssertAddress("FrameSetLayer", cached_proc, "UIModule");
                return cached_proc;
            }
        }

        GWCA_ERR("[SCAN] ResolveFrameSetLayer — assertion not found");
        return 0;
    }

    // Resolves FrameSetPosition(Coord2f const&) (EXE 0x0062f7f0).
    // WASM: FrameSetPosition(unsigned int, Coord2f const&) @ ram:809a97bb
    // Assertion: FrApi.cpp, "frameId", line 0x85c
    // Takes a frameId + pointer to {float x, float y} (8 bytes).
    static uint32_t ResolveFrameSetPositionCoord2f()
    {
        static uint32_t cached_proc = 0;
        if (cached_proc)
            return cached_proc;

        uintptr_t addr = 0;
        try {
            addr = GW::Scanner::FindAssertion(
                "\\Code\\Engine\\Frame\\FrApi.cpp", "frameId", 0x85c, 0);
        } catch (...) {
            addr = 0;
        }
        if (addr) {
            const uintptr_t fn_start = GW::Scanner::ToFunctionStart(addr, 0x100);
            if (fn_start) {
                cached_proc = static_cast<uint32_t>(fn_start);
                Logger::AssertAddress("FrameSetPosition(Coord2f)", cached_proc, "UIModule");
                return cached_proc;
            }
        }

        GWCA_ERR("[SCAN] ResolveFrameSetPositionCoord2f — assertion not found");
        return 0;
    }

    // Resolves FrameSetSize (EXE 0x0062f9a0) — sets a frame's dimensions.
    // WASM: FrameSetSize(unsigned int, Coord2f const&) @ ram:809a9c14
    // Assertion: FrApi.cpp, "frameId", line 0x880
    // Takes (uint32_t frame_id, Coord2f* size).
    static uint32_t ResolveFrameSetSize()
    {
        static uint32_t cached_proc = 0;
        if (cached_proc)
            return cached_proc;

        uintptr_t addr = 0;
        try {
            addr = GW::Scanner::FindAssertion(
                "\\Code\\Engine\\Frame\\FrApi.cpp", "frameId", 0x880, 0);
        } catch (...) {
            addr = 0;
        }
        if (addr) {
            const uintptr_t fn_start = GW::Scanner::ToFunctionStart(addr, 0x100);
            if (fn_start) {
                cached_proc = static_cast<uint32_t>(fn_start);
                Logger::AssertAddress("FrameSetSize", cached_proc, "UIModule");
                return cached_proc;
            }
        }

        GWCA_ERR("[SCAN] ResolveFrameSetSize — assertion not found");
        return 0;
    }

    // Resolves FrameGetClientBorder (EXE 0x0062D000).
    // WASM: FrameGetClientBorder(unsigned int) @ ram:809a8164
    // Assertion: FrApi.cpp, "frameId", line 0x7dd
    // Prototype: Rect4f*(Rect4f* out, uint frameId)
    static uint32_t ResolveFrameGetClientBorder()
    {
        static uint32_t cached_proc = 0;
        if (cached_proc)
            return cached_proc;

        uintptr_t addr = 0;
        try {
            addr = GW::Scanner::FindAssertion(
                "\\Code\\Engine\\Frame\\FrApi.cpp", "frameId", 0x7dd, 0);
        } catch (...) {
            addr = 0;
        }
        if (addr) {
            const uintptr_t fn_start = GW::Scanner::ToFunctionStart(addr, 0x100);
            if (fn_start) {
                cached_proc = static_cast<uint32_t>(fn_start);
                Logger::AssertAddress("FrameGetClientBorder", cached_proc, "UIModule");
                return cached_proc;
            }
        }

        GWCA_ERR("[SCAN] ResolveFrameGetClientBorder — assertion not found");
        return 0;
    }

    // Resolves FrameActivate (EXE 0x0062b000) — brings window to front and sets
    // up the click-to-raise popup mechanism.  Tail-calls CRelation::Activate().
    // WASM: FrameActivate(unsigned int) @ ram:809b0e7f
    // Assertion: FrApi.cpp, "frameId", line 0xC3E
    // Prologue: 55 8B EC 8B 45 08 85 C0
    static uint32_t ResolveFrameActivate()
    {
        static uint32_t cached_proc = 0;
        if (cached_proc)
            return cached_proc;

        uintptr_t addr = 0;
        try {
            addr = GW::Scanner::FindAssertion(
                "\\Code\\Engine\\Frame\\FrApi.cpp", "frameId", 0xC3E, 0);
        } catch (...) {
            addr = 0;
        }
        if (addr) {
            const uintptr_t fn_start = GW::Scanner::ToFunctionStart(addr, 0x80);
            if (fn_start) {
                const uint8_t* ptr = reinterpret_cast<const uint8_t*>(fn_start);
                if (ptr && ptr[0] == 0x55 && ptr[1] == 0x8B && ptr[2] == 0xEC &&
                    ptr[3] == 0x8B && ptr[4] == 0x45 && ptr[5] == 0x08 &&
                    ptr[6] == 0x85 && ptr[7] == 0xC0) {
                    cached_proc = static_cast<uint32_t>(fn_start);
                    Logger::AssertAddress("FrameActivate", cached_proc, "UIModule");
                    return cached_proc;
                }
            }
        }

        GWCA_ERR("[SCAN] ResolveFrameActivate — assertion not found");
        return 0;
    }

    // Resolves Ui_InvalidateFrameContent (WASM: FrameContentInvalidate) via byte-pattern scan.
    // Pattern: 8D 48 04 53 6A 04 E8 (at +0x57 from function start).
    // Offset -0x57 applied to locate the function prologue.
    // Returns function pointer: void(__cdecl*)(uint32_t frameId, uint32_t flags)
    static uint32_t ResolveFrameContentInvalidate()
    {
        static uint32_t cached_proc = 0;
        if (cached_proc)
            return cached_proc;

        // Byte pattern: LEA ECX, [EAX+0x4] ; PUSH EBX ; PUSH 0x4 ; CALL ...
        // This is unique in the EXE and anchors at +0x57 from the function prologue.
        uintptr_t proc_addr = GW::Scanner::Find(
            "\x8D\x48\x04\x53\x6A\x04\xE8",
            "xxxxxxx");
        if (!proc_addr) {
            GWCA_ERR("[SCAN] ResolveFrameContentInvalidate — byte pattern not found");
            return 0;
        }
        const uintptr_t fn_start = GW::Scanner::ToFunctionStart(proc_addr, 0x80);
        if (!fn_start) {
            GWCA_ERR("[SCAN] ResolveFrameContentInvalidate — ToFunctionStart failed");
            return 0;
        }
        cached_proc = static_cast<uint32_t>(fn_start);
        Logger::AssertAddress("Ui_InvalidateFrameContent", cached_proc, "UIModule");
        return cached_proc;
    }

    // NOTE: Returns true based on GameThread::Enqueue success, NOT on lambda execution
    // success. The lambda silently fails if any resolved function pointer is null, or if
    // the frame is destroyed between enqueue and execution. This is acceptable for POC
    // usage since the lambda's operations (subclass, mouse enable, title set, layout,
    // show, redraw) are best-effort chrome installation.
    // Installs the composite root control subclass on a frame, then sets title,
    // applies position override, sets Z-layer, and shows/redraws — all in a single
    // game-thread lambda.
    //
    // 2026-06-03 — Window Polish: Added position_x, position_y, layer parameters.
    // FrameSetPosition(Coord2f) overrides the anchor-system positioning to prevent
    // the UiGenerateFramePositionLockFlags centering drift.  FrameSetLayer sets a
    // unique Z-layer to prevent z-fighting between co-planar windows.
    static bool AttachCompositeRootToFrame(
        uint32_t frame_id,
        const std::wstring& title = std::wstring(),
        uint32_t subclass_flags = DEFAULT_SUBCLASS_FLAGS_COMPOSITE_ROOT,
        float position_x = 0.0f,
        float position_y = 0.0f,
        int layer = 0)
    {
        using FrameNewSubclass_pt = void*(__cdecl*)(uint32_t, void*, void*);
        using FrameMouseEnable_pt = void(__cdecl*)(uint32_t, uint32_t, uint32_t);

        static FrameNewSubclass_pt subclass_fn = nullptr;
        if (!subclass_fn) {
            subclass_fn = reinterpret_cast<FrameNewSubclass_pt>(ResolveFrameNewSubclass());
            if (!subclass_fn) return false;
        }

        const uint32_t proc = ResolveCompositeRootControlProc();
        if (!proc) return false;

        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated())) return false;

        // Shared resolver — deduplicates CreateEncodedText + DevText walk.
        static SetFrameTextResolved resolved;
        ResolveSetFrameText(resolved);

        static FrameMouseEnable_pt mouse_enable_fn = nullptr;
        static bool me_scan_attempted = false;
        if (!me_scan_attempted) {
            me_scan_attempted = true;
            const auto me_addr = ResolveFrameMouseEnable();
            if (me_addr)
                mouse_enable_fn = reinterpret_cast<FrameMouseEnable_pt>(me_addr);
        }

        // ★ NEW — Window Polish resolvers (2026-06-03)
        using FrameSetPositionCoord2f_pt = void(__cdecl*)(uint32_t, Coord2f*);
        using FrameSetLayer_pt = void(__cdecl*)(uint32_t, int);
        using FrameActivate_pt = void(__cdecl*)(uint32_t);

        static FrameSetPositionCoord2f_pt frame_set_pos_fn = nullptr;
        if (!frame_set_pos_fn) {
            const auto addr = ResolveFrameSetPositionCoord2f();
            if (addr)
                frame_set_pos_fn = reinterpret_cast<FrameSetPositionCoord2f_pt>(addr);
        }

        static FrameSetLayer_pt frame_set_layer_fn = nullptr;
        if (!frame_set_layer_fn) {
            const auto addr = ResolveFrameSetLayer();
            if (addr)
                frame_set_layer_fn = reinterpret_cast<FrameSetLayer_pt>(addr);
        }

        // ★ FrameActivate — bring-to-front / click-to-raise (2026-06-03)
        static FrameActivate_pt frame_activate_fn = nullptr;
        if (!frame_activate_fn) {
            const auto addr = ResolveFrameActivate();
            if (addr)
                frame_activate_fn = reinterpret_cast<FrameActivate_pt>(addr);
        }

        const uint32_t target_fid = frame_id;
        const uint32_t target_proc = proc;
        const uint32_t target_flags = subclass_flags;
        const std::wstring target_title = title;
        const auto s_fn = subclass_fn;
        const auto ct_fn = resolved.create_text_fn;
        const auto st_fn = resolved.set_frame_text_fn;
        const auto me_fn = mouse_enable_fn;
        const auto pos_fn = frame_set_pos_fn;
        const auto lay_fn = frame_set_layer_fn;
        const auto act_fn = frame_activate_fn;
        const float px = position_x;
        const float py = position_y;
        const int alayer = layer;
        GW::GameThread::Enqueue([target_fid, target_proc, target_flags, target_title,
                                 s_fn, ct_fn, st_fn, me_fn, pos_fn, lay_fn, act_fn, px, py, alayer]() {
            // 1. FrameNewSubclass — attach CRProc
            s_fn(target_fid, reinterpret_cast<void*>(target_proc),
                 reinterpret_cast<void*>(static_cast<uintptr_t>(target_flags)));

            // 2. FrameMouseEnable — enable mouse input
            if (me_fn) {
                me_fn(target_fid, 0xFFFFFFFF, 0);
            }

            // 3. Title set — CreateEncodedText → SetFrameText
            if (!target_title.empty() && ct_fn && st_fn) {
                const uintptr_t payload = ct_fn(8, 7, target_title.c_str(), 0);
                if (payload) {
                    st_fn(target_fid, payload);
                }
            }

            // 4. ProcessFrameControllerUpdateByFrameId — apply anchor margins
            UIManager::ProcessFrameControllerUpdateByFrameId(target_fid);

            // 5. FrameSetPosition — override anchor positioning (bypass centering drift)
            if (pos_fn) {
                Coord2f pos = { px, py };
                pos_fn(target_fid, &pos);
            }

            // 6. FrameSetLayer — set Z-layer (prevent z-fighting)
            if (lay_fn) {
                lay_fn(target_fid, alayer);
            }

            // 7. FrameActivate — bring to front / enable click-to-raise popup mechanism
            if (act_fn) {
                act_fn(target_fid);
            }

            // 8-9. ShowFrame + TriggerFrameRedraw
            GW::UI::Frame* f = GW::UI::GetFrameById(target_fid);
            if (f && f->IsCreated()) {
                GW::UI::ShowFrame(f, true);
                GW::UI::TriggerFrameRedraw(f);
            }
        });
        return true;
    }

    // Creates a standalone native window from content-space coordinates.
    // Inputs are pixel-space content bounds with a top-left origin, matching overlay/UI usage.
    // The binding expands the bounds to include native chrome, converts them into the game's
    // logical bottom-left coordinate space, then installs the composite root subclass.
    static uint32_t CreateNativeWindow(
        float content_x, float content_y, float content_width, float content_height,
        const std::wstring& title = L"")
    {
        constexpr float kLeftBorder = 32.0f;
        constexpr float kTopTitle = 20.0f;
        constexpr float kRightBorder = 32.0f;
        constexpr float kBottomBorder = 32.0f;
        constexpr uintptr_t kCreateParam = 0;
        constexpr uint32_t kAnchorFlags = 0x6;
        constexpr uint32_t kSubclassFlags = DEFAULT_SUBCLASS_FLAGS_COMPOSITE_ROOT;
        constexpr uint32_t kParentFrameId = 9;
        constexpr uint32_t kChildIndex = 0;
        constexpr uint32_t kFrameFlags = 0x20;
        static int next_layer = 1;

        GW::UI::Frame* root = GW::UI::GetRootFrame();
        if (!root) {
            GWCA_ERR("[UI] CreateNativeWindow — root frame unavailable");
            return 0;
        }

        const auto viewport_scale = root->position.GetViewportScale(root);
        const float scale_x = viewport_scale.x != 0.0f ? viewport_scale.x : 1.0f;
        const float scale_y = viewport_scale.y != 0.0f ? viewport_scale.y : 1.0f;
        const float pixel_height = root->position.viewport_height * scale_y;

        const float frame_px_x = content_x - kLeftBorder;
        const float frame_px_y = pixel_height - content_y - content_height - kBottomBorder;
        const float frame_px_w = content_width + kLeftBorder + kRightBorder;
        const float frame_px_h = content_height + kTopTitle + kBottomBorder;

        const float engine_x = frame_px_x / scale_x;
        const float engine_y = frame_px_y / scale_y;
        const float engine_w = frame_px_w / scale_x;
        const float engine_h = frame_px_h / scale_y;
        const int layer = next_layer++;

        const uint32_t frame_id = _create_container_window(
            engine_x, engine_y, engine_w, engine_h, title,
            kParentFrameId, kChildIndex, kFrameFlags, kCreateParam, kAnchorFlags);

        if (!frame_id) {
            GWCA_ERR("[UI] CreateTitledContainerWindow — _create_container_window failed");
            return 0;
        }

        if (!AttachCompositeRootToFrame(frame_id, title, kSubclassFlags, engine_x, engine_y, layer)) {
            GWCA_ERR("[UI] CreateTitledContainerWindow — AttachCompositeRootToFrame failed, frame_id=%u", frame_id);
            GW::GameThread::Enqueue([frame_id]() {
                GW::UI::Frame* f = GW::UI::GetFrameById(frame_id);
                if (f && f->IsCreated()) {
                    GW::UI::DestroyUIComponent(f);
                }
            });
            return 0;
        }
        return frame_id;
    }

    // ── Diagnostic: native FrameGetClientBorder (2026-06-03) ──────────
    // Calls the raw EXE FrameGetClientBorder (0x0062D000) directly,
    // bypassing the GWCA wrapper.  Useful for verifying chrome dimensions
    // returned by the native msg-0x15 handler vs the GWCA cache.
    // Returns Rect4f {top, left, right, bottom} in pixels.
    static Rect4f GetFrameClientBorderNative(uint32_t frame_id)
    {
        using FrameGetClientBorder_pt = Rect4f*(__cdecl*)(uint32_t, Rect4f*);
        static FrameGetClientBorder_pt fn = nullptr;
        if (!fn) {
            const auto addr = ResolveFrameGetClientBorder();
            if (!addr) {
                Rect4f zero = { 0.0f, 0.0f, 0.0f, 0.0f };
                return zero;
            }
            fn = reinterpret_cast<FrameGetClientBorder_pt>(addr);
        }

        Rect4f out = { 0.0f, 0.0f, 0.0f, 0.0f };
        fn(frame_id, &out);
        return out;
    }

    // Ensures that the DevText specimen window exists and reports whether it was opened here.
    static std::pair<uint32_t, bool> EnsureDevTextSource()
    {
        uint32_t frame_id = GetFrameIDByLabel("DevText");
        auto* frame = frame_id ? GW::UI::GetFrameById(frame_id) : nullptr;
        if (frame && frame->IsCreated())
            return { frame_id, false };

        KeyPress(0x25, 0);
        frame_id = GetFrameIDByLabel("DevText");
        frame = frame_id ? GW::UI::GetFrameById(frame_id) : nullptr;
        return { frame_id, frame && frame->IsCreated() };
    }

    // Opens the DevText specimen window and returns its frame id if available.
    static uint32_t OpenDevTextWindow()
    {
        KeyPress(0x25, 0);
        const uint32_t frame_id = GetFrameIDByLabel("DevText");
        auto* frame = frame_id ? GW::UI::GetFrameById(frame_id) : nullptr;
        return (frame && frame->IsCreated()) ? frame_id : 0;
    }

    // Returns the current DevText specimen frame id when the window exists.
    static uint32_t GetDevTextFrameID()
    {
        const uint32_t frame_id = GetFrameIDByLabel("DevText");
        auto* frame = frame_id ? GW::UI::GetFrameById(frame_id) : nullptr;
        return (frame && frame->IsCreated()) ? frame_id : 0;
    }

    // Restores the DevText specimen to its prior visibility state when we opened it temporarily.
    static void RestoreDevTextSource(bool opened_temporarily)
    {
        if (!opened_temporarily)
            return;

        const uint32_t frame_id = GetFrameIDByLabel("DevText");
        auto* frame = frame_id ? GW::UI::GetFrameById(frame_id) : nullptr;
        if (frame && frame->IsCreated())
            KeyPress(0x25, 0);
    }

    // Resolves the inner content host used by cloned composite windows.
    static uint32_t ResolveObservedContentHostByFrameId(uint32_t root_frame_id)
    {
        if (!root_frame_id)
            return 0;
        return GetChildFramePathByFrameId(root_frame_id, { 0, 0, 0 });
    }

    // Clears all descendant children of a frame by calling the native recursive helper.
    // CRITICAL: The frame validity check must happen inside the enqueued lambda, not
    // before enqueuing. Frame state can change between enqueue and execution ticks.
    // A destroyed frame passed to the native clear function causes a crash.
    static bool ClearFrameChildrenRecursiveByFrameId(uint32_t frame_id)
    {
        using ClearFrameChildrenRecursive_pt = void(__cdecl*)(uint32_t);

        static ClearFrameChildrenRecursive_pt fn = nullptr;
        if (!fn) {
            const auto func_addr = GW::Scanner::Find(
                "\x55\x8B\xEC\x83\xEC\x08\x56\x8B\x75\x08\x85\xF6\x75\x19",
                "xxxxxxxxxxxxxx");
            if (!func_addr)
                return false;
            fn = reinterpret_cast<ClearFrameChildrenRecursive_pt>(func_addr);
        }

        // Validate frame exists at enqueue time to fail early for obviously
        // invalid IDs, but the real guard is inside the lambda.
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;

        const auto clear_fn = fn;
        GW::GameThread::Enqueue([frame_id, clear_fn]() {
            if (!clear_fn)
                return;
            // Re-validate at execution time — frame may have been destroyed
            // between the enqueue and this tick.
            GW::UI::Frame* f = GW::UI::GetFrameById(frame_id);
            if (!(f && f->IsCreated()))
                return;
            clear_fn(frame_id);
        });
        return true;
    }

    // Clears the content host of a cloned window and redraws the root.
    // The redraw is chained inside the enqueued lambda so it runs AFTER the
    // recursive clear completes, avoiding race conditions where the render
    // pipeline draws children that are about to be destroyed.
    static bool ClearWindowContentsByFrameId(uint32_t root_frame_id)
    {
        const uint32_t host_frame_id = ResolveObservedContentHostByFrameId(root_frame_id);
        if (!host_frame_id)
            return false;
        if (!ClearFrameChildrenRecursiveByFrameId(host_frame_id))
            return false;

        // Chain the redraw after the clear so it executes on the same tick
        // as the recursive destruction, not before it.
        GW::GameThread::Enqueue([root_frame_id]() {
            GW::UI::Frame* f = GW::UI::GetFrameById(root_frame_id);
            if (f && f->IsCreated())
                GW::UI::TriggerFrameRedraw(f);
        });
        return true;
    }

    // Clones a DevText-backed composite window and optionally arms a title override first.
    static uint32_t CreateWindowClone(
        float x,
        float y,
        float width,
        float height,
        const std::wstring& frame_label = L"",
        uint32_t parent_frame_id = 9,
        uint32_t child_index = 0,
        uint32_t frame_flags = 0,
        uintptr_t create_param = 0,
        uintptr_t frame_callback = 0,
        uint32_t anchor_flags = 0x6,
        bool ensure_devtext_source = true)
    {
        bool opened_temporarily = false;
        if (!frame_callback) {
            if (ensure_devtext_source) {
                const auto ensure_result = EnsureDevTextSource();
                opened_temporarily = ensure_result.second;
            }
            frame_callback = ResolveDevTextDialogProc();
            if (!frame_callback) {
                RestoreDevTextSource(opened_temporarily);
                return 0;
            }
        }

        const uint32_t resolved_child_index = child_index > 0
            ? child_index
            : FindAvailableChildSlot(parent_frame_id);
        if (!resolved_child_index) {
            RestoreDevTextSource(opened_temporarily);
            return 0;
        }

        // Clone creation can inherit the specimen's caption resource unless we
        // arm the title override before the source dialog proc runs.
        const bool arm_title_override = UIManagerTitleHook::HasNextCreatedWindowTitle();
        if (arm_title_override)
            UIManagerTitleHook::ArmNextCreatedWindowTitle(parent_frame_id, resolved_child_index);

        const uint32_t frame_id = CreateWindowByFrameId(
            parent_frame_id,
            resolved_child_index,
            frame_callback,
            x,
            y,
            width,
            height,
            frame_flags,
            create_param,
            frame_label,
            anchor_flags);
        if (!frame_id && arm_title_override)
            UIManagerTitleHook::CancelArmedWindowTitle(parent_frame_id, resolved_child_index);
        RestoreDevTextSource(opened_temporarily);
        return frame_id;
    }

    // Clones a DevText-backed window and clears its contents immediately after creation.
    static uint32_t CreateEmptyWindowClone(
        float x,
        float y,
        float width,
        float height,
        const std::wstring& frame_label = L"",
        uint32_t parent_frame_id = 9,
        uint32_t child_index = 0,
        uint32_t frame_flags = 0,
        uintptr_t create_param = 0,
        uintptr_t frame_callback = 0,
        uint32_t anchor_flags = 0x6,
        bool ensure_devtext_source = true)
    {
        const uint32_t frame_id = CreateWindowClone(
            x,
            y,
            width,
            height,
            frame_label,
            parent_frame_id,
            child_index,
            frame_flags,
            create_param,
            frame_callback,
            anchor_flags,
            ensure_devtext_source);
        if (!frame_id)
            return 0;

        // ClearWindowContentsByFrameId now enqueues both the recursive clear AND the
        // follow-up redraw inside the same lambda, eliminating the race between
        // synchronous redraw (old content) and asynchronous clear (destroys children).
        ClearWindowContentsByFrameId(frame_id);
        return frame_id;
    }

    // Creates a DevText-backed composite window with a custom title via clone-title override.
    // This is the PATH A approach: the title is substituted during FrameCreate so that
    // FrameSetTitle → CNonclient::SetTitle → OnTitleResolved → CContent::Invalidate fires,
    // giving proper per-frame invalidation and title-bar rendering.
    // After creation, DevText's body content is cleared, leaving an empty window with a working title.
    static uint32_t CreateTitledWindowClone(
        const std::wstring& title,
        float x,
        float y,
        float width,
        float height,
        const std::wstring& frame_label = L"")
    {
        constexpr uint32_t kParentFrameId = 9;
        constexpr uint32_t kChildIndex = 0;
        constexpr uint32_t kFrameFlags = 0;
        constexpr uintptr_t kCreateParam = 0;
        constexpr uintptr_t kFrameCallback = 0;
        constexpr uint32_t kAnchorFlags = 0x6;
        constexpr bool kEnsureDevTextSource = true;

        if (title.empty())
            return 0;

        if (!UIManagerTitleHook::SetNextCreatedWindowTitle(title)) {
            GWCA_ERR("[UI] CreateTitledWindowClone — SetNextCreatedWindowTitle failed");
            return 0;
        }

        const uint32_t frame_id = CreateWindowClone(
            x, y, width, height,
            frame_label,
            kParentFrameId, kChildIndex,
            kFrameFlags, kCreateParam,
            kFrameCallback, kAnchorFlags,
            kEnsureDevTextSource);

        if (!frame_id) {
            UIManagerTitleHook::ClearNextCreatedWindowTitle();
            GWCA_ERR("[UI] CreateTitledWindowClone — CreateWindowClone failed");
            return 0;
        }

        ClearWindowContentsByFrameId(frame_id);
        return frame_id;  // redraw is enqueued inside ClearWindowContentsByFrameId
    }

    // Creates a titled empty composite window via CreateEmptyWindowClone with title override.
    // Same Path A title rendering as CreateTitledWindowClone but uses the simpler
    // CreateEmptyWindowClone path which clears content immediately as part of creation.
    static uint32_t CreateTitledEmptyWindowClone(
        const std::wstring& title,
        float x,
        float y,
        float width,
        float height,
        const std::wstring& frame_label = L"CustomWindow")
    {
        constexpr uint32_t kParentFrameId = 9;
        constexpr uint32_t kChildIndex = 0;
        constexpr uint32_t kFrameFlags = 0;
        constexpr uintptr_t kCreateParam = 0;
        constexpr uintptr_t kFrameCallback = 0;
        constexpr uint32_t kAnchorFlags = 0x6;
        constexpr bool kEnsureDevTextSource = true;

        if (title.empty())
            return 0;

        if (!UIManagerTitleHook::SetNextCreatedWindowTitle(title)) {
            GWCA_ERR("[UI] CreateTitledEmptyWindow — SetNextCreatedWindowTitle failed");
            return 0;
        }

        const uint32_t frame_id = CreateEmptyWindowClone(
            x, y, width, height,
            frame_label,
            kParentFrameId, kChildIndex,
            kFrameFlags, kCreateParam,
            kFrameCallback, kAnchorFlags,
            kEnsureDevTextSource);

        if (!frame_id) {
            UIManagerTitleHook::ClearNextCreatedWindowTitle();
            GWCA_ERR("[UI] CreateTitledEmptyWindow — CreateEmptyWindowClone failed");
            return 0;
        }

        return frame_id;
    }

    // Applies frame-controller margins to place and size a frame relative to its parent.
    static bool SetFrameControllerAnchorMarginsByFrameIdEx(
        uint32_t frame_id,
        float x,
        float y,
        float width,
        float height,
        uint32_t flags = 0x6)
    {
        using SetFrameControllerAnchorMarginsByIdEx_pt =
            void(__cdecl*)(uint32_t, const float*, const float*, uint32_t);

        static SetFrameControllerAnchorMarginsByIdEx_pt fn = nullptr;
        if (!fn) {
            auto use_addr = GW::Scanner::Find(
                "\x50\xe8\x00\x00\x00\x00\x83\xc4\x04\x8d\x88\xd0\x00\x00\x00\xff\x75\x14\xff\x75\x10\xff\x75\x0c\xe8\x00\x00\x00\x00\x5d\xc3",
                "xx????xxxxxxxxxxxxxxxxxxx????xx"
            );
            if (!use_addr)
                return false;
            const auto func_addr = GW::Scanner::ToFunctionStart(use_addr, 0x80);
            if (!func_addr)
                return false;
            fn = reinterpret_cast<SetFrameControllerAnchorMarginsByIdEx_pt>(func_addr);
        }

        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;

        const float pos[2] = { x, y };
        const float size[2] = { width, height };
        fn(frame_id, pos, size, flags);
        return true;
    }

    // Queues a frame-controller layout update for the given frame.
    static bool QueueFrameControllerUpdateByFrameId(uint32_t frame_id)
    {
        using QueueFrameControllerUpdateById_pt = void(__cdecl*)(uint32_t);

        static QueueFrameControllerUpdateById_pt fn = nullptr;
        if (!fn) {
            auto use_addr = GW::Scanner::Find(
                "\x6a\x01\xe8\x00\x00\x00\x00\x5d\xc3",
                "xxx????xx");
            if (!use_addr)
                return false;
            const auto func_addr = GW::Scanner::ToFunctionStart(use_addr, 0x80);
            if (!func_addr)
                return false;
            fn = reinterpret_cast<QueueFrameControllerUpdateById_pt>(func_addr);
        }

        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;
        fn(frame_id);
        return true;
    }

    // Immediately processes a queued frame-controller layout update for the given frame.
    static bool ProcessFrameControllerUpdateByFrameId(uint32_t frame_id)
    {
        using ProcessFrameControllerUpdateById_pt = void(__cdecl*)(uint32_t);

        static ProcessFrameControllerUpdateById_pt fn = nullptr;
        if (!fn) {
            auto use_addr = GW::Scanner::Find(
                "\xe8\x00\x00\x00\x00\x5d\xc3",
                "x????xx");
            if (!use_addr)
                return false;
            const auto func_addr = GW::Scanner::ToFunctionStart(use_addr, 0x80);
            if (!func_addr)
                return false;
            fn = reinterpret_cast<ProcessFrameControllerUpdateById_pt>(func_addr);
        }

        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;
        fn(frame_id);
        return true;
    }

    // Chooses anchor flags that best match the requested rectangle within a parent area.
    static uint32_t ChooseAnchorFlagsForDesiredRect(
        float x,
        float y,
        float width,
        float height,
        float parent_width,
        float parent_height,
        bool disable_center = false)
    {
        using ChooseAnchorFlagsForDesiredRect_pt =
            uint32_t(__cdecl*)(const float*, const float*, const float*, uint32_t);

        static ChooseAnchorFlagsForDesiredRect_pt fn = nullptr;
        if (!fn) {
            auto use_addr = GW::Scanner::Find(
                "\x55\x8b\xec\x8b\x45\x10\xba\x02\x00\x00\x00\x53\x8b\x5d\x0c\x57\x8b\x7d\x08\xd9\x07\xd9\x5d\x08",
                "xxxxxxxxxxxxxxxxxxxxxxxx"
            );
            if (!use_addr)
                return 0;
            fn = reinterpret_cast<ChooseAnchorFlagsForDesiredRect_pt>(use_addr);
        }

        const float pos[2] = { x, y };
        const float size[2] = { width, height };
        const float parent_size[2] = { parent_width, parent_height };
        return fn(pos, size, parent_size, disable_center ? 1u : 0u);
    }

    // Collapses a window to a minimal rectangle while preserving anchor behavior.
    static bool CollapseWindowByFrameId(uint32_t frame_id)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;
        const uint32_t parent_frame_id = GetParentFrameID(frame_id);
        const auto parent_frame = parent_frame_id ? GW::UI::GetFrameById(parent_frame_id) : nullptr;
        const float parent_width = parent_frame
            ? static_cast<float>(abs(static_cast<int>(parent_frame->position.right) - static_cast<int>(parent_frame->position.left)))
            : 0.0f;
        const float parent_height = parent_frame
            ? static_cast<float>(abs(static_cast<int>(parent_frame->position.bottom) - static_cast<int>(parent_frame->position.top)))
            : 0.0f;
        uint32_t flags = 0x6;
        if (parent_frame) {
            const uint32_t chosen_flags = ChooseAnchorFlagsForDesiredRect(
                0.0f, 0.0f, 1.0f, 1.0f, parent_width, parent_height, true);
            if (chosen_flags)
                flags = chosen_flags;
        }
        return SetFrameControllerAnchorMarginsByFrameIdEx(frame_id, 0.0f, 0.0f, 1.0f, 1.0f, flags);
    }

    // Restores a frame to a desired rectangle and optionally recomputes anchor flags.
    static bool RestoreWindowRectByFrameId(
        uint32_t frame_id,
        float x,
        float y,
        float width,
        float height,
        uint32_t flags = 0,
        bool use_auto_flags = true,
        bool disable_center = true)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;

        uint32_t resolved_flags = flags;
        if (use_auto_flags) {
            const uint32_t parent_frame_id = GetParentFrameID(frame_id);
            const auto parent_frame = parent_frame_id ? GW::UI::GetFrameById(parent_frame_id) : nullptr;
            if (parent_frame) {
                const float parent_width = static_cast<float>(
                    abs(static_cast<int>(parent_frame->position.right) - static_cast<int>(parent_frame->position.left)));
                const float parent_height = static_cast<float>(
                    abs(static_cast<int>(parent_frame->position.bottom) - static_cast<int>(parent_frame->position.top)));
                const uint32_t chosen_flags = ChooseAnchorFlagsForDesiredRect(
                    x, y, width, height, parent_width, parent_height, disable_center);
                if (chosen_flags)
                    resolved_flags = chosen_flags;
            }
            if (!resolved_flags)
                resolved_flags = 0x6;
        }
        return SetFrameControllerAnchorMarginsByFrameIdEx(frame_id, x, y, width, height, resolved_flags);
    }

    // Applies explicit anchor flags and margins to a frame.
    static bool SetFrameMarginsByFrameId(
        uint32_t frame_id,
        uint32_t flags,
        float x,
        float y,
        float width,
        float height)
    {
        return SetFrameControllerAnchorMarginsByFrameIdEx(frame_id, x, y, width, height, flags);
    }

    // Toggles the hidden state bit on a frame and redraws it.
    static bool SetFrameVisibleByFrameId(uint32_t frame_id, bool is_visible)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;
        if (is_visible)
            frame->frame_state &= ~0x200u;
        else
            frame->frame_state |= 0x200u;
        TriggerFrameRedrawByFrameId(frame_id);
        return true;
    }

    // Toggles the disabled state bit on a frame and redraws it.
    static bool SetFrameDisabledByFrameId(uint32_t frame_id, bool is_disabled)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;
        if (is_disabled)
            frame->frame_state |= 0x10u;
        else
            frame->frame_state &= ~0x10u;
        TriggerFrameRedrawByFrameId(frame_id);
        return true;
    }

    // Tests a specific bit in the frame's state word (frame_state at +0x18C).
    // Useful bits: 0x2=visible, 0x4=created, 0x10=disabled, 0x200=hidden.
    static bool GetFrameStateBitByFrameId(uint32_t frame_id, uint32_t bit) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        return GW::UI::GetFrameStateBit(frame, bit);
    }

    // Sets frame opacity (0.0–1.0) with optional fade time.
    static bool SetFrameOpacityByFrameId(uint32_t frame_id, float opacity, float fade_time = 0.0f) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated())) return false;
        return GW::UI::SetFrameOpacity(frame, opacity, fade_time);
    }

    // Shows or hides a frame via the native msg 0xC dispatch.
    static bool ShowFrameByFrameId(uint32_t frame_id, bool show) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated())) return false;
        return GW::UI::ShowFrame(frame, show);
    }

    // Gets the parent frame_id directly (no Frame object needed).
    static uint32_t GetParentFrameIdDirect(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        return GW::UI::GetParentFrameId(frame);
    }

    // Gets frame opacity (0.0–1.0) from CContent embedded at Frame+4.
    static float GetFrameOpacityByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        return GW::UI::GetFrameOpacity(frame);
    }

    // Gets frame user param pointer (Frame+0x1C4).
    static uint32_t GetFrameUserParamByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        return GW::UI::GetFrameUserParam(frame);
    }

    // O(1) hash-table lookup: finds a child of parent by its name hash.
    // Uses the internal FrRelation hash table (DAT_ram_005a03d4 / EXE 0x00bd0d0c).
    static uint32_t GetChildFrameIdFromNameHash(uint32_t parent_frame_id, uint32_t name_hash) {
        GW::UI::Frame* parent = GW::UI::GetFrameById(parent_frame_id);
        if (!parent) return 0;
        GW::UI::Frame* child = GW::UI::GetChildFromNameHash(parent, name_hash);
        return child ? child->frame_id : 0;
    }

    // Returns all overlay frame IDs from the global overlay linked list.
    static std::vector<uint32_t> GetOverlayFrameIDs() {
        return GW::UI::GetOverlayFrames();
    }

    // Returns all popup frame IDs from the global popup linked list.
    static std::vector<uint32_t> GetPopupFrameIDs() {
        return GW::UI::GetPopupFrames();
    }

    static bool SetFrameTitleByFrameId(uint32_t frame_id, const std::wstring& title)
    {
        static SetFrameTextResolved resolved;
        if (!ResolveSetFrameText(resolved))
            return false;

        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()) || title.empty())
            return false;

        const uint32_t target_frame_id = frame_id;
        const std::wstring requested_title = title;
        const auto create_fn = resolved.create_text_fn;
        const auto set_fn = resolved.set_frame_text_fn;
        GW::GameThread::Enqueue([target_frame_id, requested_title, create_fn, set_fn]() {
            if (!(create_fn && set_fn))
                return;
            const uintptr_t payload = create_fn(8, 7, requested_title.c_str(), 0);
            if (!payload)
                return;
            set_fn(target_frame_id, payload);
        });
        return true;
    }

    // Invalidates per-frame CContent by element id and flags, causing a full redraw.
    // This is the missing per-frame dirty-list enqueue (Path A equivalent). Default
    // flags=0xFFFFFFFF for full invalidation. Combine with SetFrameTitleByFrameId
    // to achieve Path A title rendering (FrameSetTitle → CContent::Invalidate).
    static bool FrameContentInvalidate(uint32_t frame_id, uint32_t flags = 0xFFFFFFFF)
    {
        using InvalidateFrameContent_pt = void(__cdecl*)(uint32_t, uint32_t);

        static InvalidateFrameContent_pt fn = nullptr;
        if (!fn) {
            const auto addr = ResolveFrameContentInvalidate();
            if (!addr)
                return false;
            fn = reinterpret_cast<InvalidateFrameContent_pt>(addr);
        }

        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;

        const auto invalidate_fn = fn;
        GW::GameThread::Enqueue([frame_id, flags, invalidate_fn]() {
            if (invalidate_fn)
                invalidate_fn(frame_id, flags);
        });
        return true;
    }

    // Convenience wrapper that performs a full content redraw on a frame.
    static bool FrameContentRedraw(uint32_t frame_id)
    {
        return FrameContentInvalidate(frame_id, 0xFFFFFFFF);
    }

    // One-stop fix for title rendering: stores the title text (Path B text storage)
    // then triggers per-frame CContent invalidation (Path A dirty-list enqueue).
    // This combined approach gives cold-created windows visible titles without
    // needing the (currently unknown) EXE address of FrameSetTitle.
    static bool SetFrameTitleAndInvalidate(uint32_t frame_id, const std::wstring& title)
    {
        if (!SetFrameTitleByFrameId(frame_id, title))
            return false;
        return FrameContentInvalidate(frame_id, 0xFFFFFFFF);
    }

    // Helper: converts null-terminated wchar_t* to UTF-8 std::string.
    // Returns empty string on failure or empty input.
    static std::string WCharToUTF8(const wchar_t* wstr) {
        if (!wstr || !wstr[0])
            return std::string();
        int len = WideCharToMultiByte(CP_UTF8, 0,
                                      wstr, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0)
            return std::string();
        std::string result(static_cast<size_t>(len), '\0');
        int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                          wstr, -1, &result[0], len, nullptr, nullptr);
        if (written <= 0)
            return std::string();
        result.resize(static_cast<size_t>(written - 1));
        return result;
    }

    // Returns the frame's decoded title label as a UTF-8 std::string.
    // Uses GetFrameTitle → safe BinarySearch (no assertion-abort on empty title).
    static std::string GetFrameLabelByFrameId(uint32_t frame_id)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame)
            return std::string();
        const wchar_t* title = GW::UI::GetFrameTitle(frame);
        if (!title || !title[0])
            return std::string();
        return WCharToUTF8(title);
    }

    // Returns the encoded label string currently attached to a text-label frame.
    static std::wstring GetTextLabelEncodedByFrameId(uint32_t frame_id)
    {
        auto* frame = reinterpret_cast<GW::TextLabelFrame*>(GW::UI::GetFrameById(frame_id));
        if (!frame)
            return std::wstring();
        const wchar_t* text = frame->GetEncodedLabel();
        return text ? std::wstring(text) : std::wstring();
    }

    // Returns the encoded label payload as raw wchar bytes including the terminator.
    static py::bytes GetTextLabelEncodedBytesByFrameId(uint32_t frame_id)
    {
        auto* frame = reinterpret_cast<GW::TextLabelFrame*>(GW::UI::GetFrameById(frame_id));
        if (!frame)
            return py::bytes();
        const wchar_t* text = frame->GetEncodedLabel();
        if (!text)
            return py::bytes();
        size_t len = 0;
        while (text[len] != 0x0000) {
            ++len;
        }
        ++len;
        return py::bytes(reinterpret_cast<const char*>(text), len * sizeof(wchar_t));
    }

    // Returns the decoded label text currently rendered by a text-label frame.
    static std::wstring GetTextLabelDecodedByFrameId(uint32_t frame_id)
    {
        auto* frame = reinterpret_cast<GW::TextLabelFrame*>(GW::UI::GetFrameById(frame_id));
        if (!frame)
            return std::wstring();
        const wchar_t* text = frame->GetDecodedLabel();
        return text ? std::wstring(text) : std::wstring();
    }

    // ── DEBUG HELPERS: Title diagnostics (Option A + Option B verification) ──────────

    // Returns the raw runtime pointer address of a frame, for direct memory inspection.
    // Python can use this with ctypes to read frame+0x18 (paint mask) etc.
    static uintptr_t GetFrameBaseAddress(uint32_t frame_id)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        return reinterpret_cast<uintptr_t>(frame);
    }

    // Resolves Ui_GetFrameTextCaptionText @ EXE 0x0060e850 → wchar_t*(frame_id).
    // Returns the dynamic text caption (Path B attached-text table).
    // Scanner: FindAssertion("FrApi.cpp", "frameId", 0x608, 0) or byte pattern.
    static uint32_t ResolveGetFrameTextCaptionText()
    {
        static uint32_t cached = 0;
        if (cached) return cached;
        // Byte pattern: function prologue + unique call sequence
        uint32_t addr = GW::Scanner::Find("\x55\x8B\xEC\x56\x8B\x75\x08\x85\xF6\x74\x1C\x8B\x06"
                                           "\x85\xC0\x74\x16\x8B\x50\x08\x8B\x42\x04", 
                                           "xxxxxxxxxxxxxxxxxxxxxxxxx", 0);
        if (addr) {
            cached = GW::Scanner::ToFunctionStart(addr, 0x40);
            Logger::AssertAddress("Ui_GetFrameTextCaptionText", cached);
        }
        return cached;
    }

    // Queries the dynamic text caption for a frame (Path B table).
    static std::wstring GetFrameTextCaptionText(uint32_t frame_id)
    {
        uint32_t addr = ResolveGetFrameTextCaptionText();
        if (!addr) return std::wstring();
        typedef const wchar_t*(__cdecl *FuncT)(uint32_t);
        auto fn = reinterpret_cast<FuncT>(addr);
        const wchar_t* text = fn(frame_id);
        return text ? std::wstring(text) : std::wstring();
    }

    // Resolves Ui_GetFrameResourceCaptionText @ EXE 0x0060e810 → wchar_t*(frame_id).
    static uint32_t ResolveGetFrameResourceCaptionText()
    {
        static uint32_t cached = 0;
        if (cached) return cached;
        uint32_t addr = GW::Scanner::Find("\x55\x8B\xEC\x56\x8B\x75\x08\x85\xF6\x74\x0D"
                                           "\x8B\x06\x8B\x48\x08\x85\xC9", 
                                           "xxxxxxxxxxxxxxxxxxx", 0);
        if (addr) {
            cached = GW::Scanner::ToFunctionStart(addr, 0x40);
            Logger::AssertAddress("Ui_GetFrameResourceCaptionText", cached);
        }
        return cached;
    }

    // Queries the resource caption for a frame (Path B table).
    static std::wstring GetFrameResourceCaptionText(uint32_t frame_id)
    {
        uint32_t addr = ResolveGetFrameResourceCaptionText();
        if (!addr) return std::wstring();
        typedef const wchar_t*(__cdecl *FuncT)(uint32_t);
        auto fn = reinterpret_cast<FuncT>(addr);
        const wchar_t* text = fn(frame_id);
        return text ? std::wstring(text) : std::wstring();
    }

    // Sets the label on a button frame.
    static bool SetLabelByFrameId(uint32_t frame_id, const std::wstring& label)
    {
        auto* frame = reinterpret_cast<GW::ButtonFrame*>(GW::UI::GetFrameById(frame_id));
        if (!(frame && frame->IsCreated()) || label.empty())
            return false;
        return frame->SetLabel(label.c_str());
    }

    // Sets the encoded label on a text-label frame.
    static bool SetTextLabelByFrameId(uint32_t frame_id, const std::wstring& label)
    {
        auto* frame = reinterpret_cast<GW::TextLabelFrame*>(GW::UI::GetFrameById(frame_id));
        if (!(frame && frame->IsCreated()) || label.empty())
            return false;
        return frame->SetLabel(label.c_str());
    }

    // Sets the encoded label from a raw wchar-byte payload.
    static bool SetTextLabelBytesByFrameId(uint32_t frame_id, const py::bytes& label_bytes)
    {
        auto* frame = reinterpret_cast<GW::TextLabelFrame*>(GW::UI::GetFrameById(frame_id));
        if (!(frame && frame->IsCreated()))
            return false;
        const std::string raw = label_bytes;
        if (raw.empty() || (raw.size() % sizeof(wchar_t)) != 0)
            return false;
        const auto* wide = reinterpret_cast<const wchar_t*>(raw.data());
        const size_t wide_len = raw.size() / sizeof(wchar_t);
        if (wide[wide_len - 1] != 0x0000)
            return false;
        return frame->SetLabel(wide);
    }

    // Appends an already-encoded suffix to a text-label frame.
    static bool AppendTextLabelEncodedSuffixByFrameId(uint32_t frame_id, const std::wstring& encoded_suffix)
    {
        auto* frame = reinterpret_cast<GW::TextLabelFrame*>(GW::UI::GetFrameById(frame_id));
        if (!(frame && frame->IsCreated()) || encoded_suffix.empty())
            return false;
        const wchar_t* current_text = frame->GetEncodedLabel();
        if (!current_text || !current_text[0])
            return false;
        std::wstring new_text(current_text);
        new_text += encoded_suffix;
        return frame->SetLabel(new_text.c_str());
    }

    // Appends plain text by wrapping it in the literal-text encoded control sequence.
    static bool AppendTextLabelPlainSuffixByFrameId(uint32_t frame_id, const std::wstring& plain_text)
    {
        auto* frame = reinterpret_cast<GW::TextLabelFrame*>(GW::UI::GetFrameById(frame_id));
        if (!(frame && frame->IsCreated()) || plain_text.empty())
            return false;
        const wchar_t* current_text = frame->GetEncodedLabel();
        if (!current_text || !current_text[0])
            return false;
        std::wstring new_text(current_text);
        new_text.push_back(static_cast<wchar_t>(0x0002));
        new_text.push_back(static_cast<wchar_t>(0x0108));
        new_text.push_back(static_cast<wchar_t>(0x0107));
        for (const wchar_t ch : plain_text) {
            if (ch == L'[' || ch == L']' || ch == L'\\') {
                new_text.push_back(L'\\');
            }
            new_text.push_back(ch);
        }
        new_text.push_back(static_cast<wchar_t>(0x0001));
        return frame->SetLabel(new_text.c_str());
    }

    // Builds an encoded literal-text payload from plain text.
    static std::wstring BuildStandaloneLiteralEncodedTextPayload(const std::wstring& plain_text)
    {
        std::wstring encoded_name_enc;
        if (plain_text.empty())
            return encoded_name_enc;
        encoded_name_enc.push_back(static_cast<wchar_t>(0x0108));
        encoded_name_enc.push_back(static_cast<wchar_t>(0x0107));
        for (const wchar_t ch : plain_text) {
            if (ch == L'[' || ch == L']' || ch == L'\\') {
                encoded_name_enc.push_back(L'\\');
            }
            encoded_name_enc.push_back(ch);
        }
        encoded_name_enc.push_back(static_cast<wchar_t>(0x0001));
        return encoded_name_enc;
    }

    // Sets the label on a multi-line text-label frame.
    static bool SetMultilineLabelByFrameId(uint32_t frame_id, const std::wstring& label)
    {
        auto* frame = reinterpret_cast<GW::MultiLineTextLabelFrame*>(GW::UI::GetFrameById(frame_id));
        if (!(frame && frame->IsCreated()) || label.empty())
            return false;
        return frame->SetLabel(label.c_str());
    }

    // Changes the font id used by a text-label frame.
    static bool SetTextLabelFontByFrameId(uint32_t frame_id, uint32_t font_id)
    {
        auto* frame = reinterpret_cast<GW::TextLabelFrame*>(GW::UI::GetFrameById(frame_id));
        if (!(frame && frame->IsCreated()))
            return false;
        return frame->SetFont(font_id);
    }

    // Sends the read-only UI message to a frame.
    static bool SetReadOnlyByFrameId(uint32_t frame_id, bool is_read_only)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;
        return GW::UI::SendFrameUIMessage(frame, static_cast<GW::UI::UIMessage>(0x5b), reinterpret_cast<void*>(static_cast<uintptr_t>(is_read_only ? 1u : 0u)), nullptr);
    }

    // Queries a frame's read-only state through the native UI message path.
    static bool IsReadOnlyByFrameId(uint32_t frame_id)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;
        uint32_t scratch = frame_id & 0x00ffffffu;
        auto* result_ptr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&scratch) + 3);
        GW::UI::SendFrameUIMessage(frame, static_cast<GW::UI::UIMessage>(0x56), result_ptr, nullptr);
        return ((scratch >> 24) & 0xffu) != 0;
    }

    // Arms the next DevText-backed window creation to replace its title.
    static bool SetNextCreatedWindowTitle(const std::wstring& title)
    {
        return UIManagerTitleHook::SetNextCreatedWindowTitle(title);
    }

    // Clears any pending clone-time title override state.
    static void ClearNextCreatedWindowTitle()
    {
        UIManagerTitleHook::ClearNextCreatedWindowTitle();
    }

    // Reports whether a clone-time title override is currently pending.
    static bool HasNextCreatedWindowTitle()
    {
        return UIManagerTitleHook::HasNextCreatedWindowTitle();
    }

    // Reports whether the clone-time title hook has been installed successfully.
    static bool IsWindowTitleHookInstalled()
    {
        return UIManagerTitleHook::IsInstalled();
    }

    // Returns the frame id that last received a clone-time title override.
    static uint32_t GetLastAppliedWindowTitleFrameId()
    {
        return UIManagerTitleHook::GetLastAppliedFrameId();
    }

    // Returns the last clone-time title text that was applied.
    static std::wstring GetLastAppliedWindowTitle()
    {
        return UIManagerTitleHook::GetLastAppliedTitle();
    }

    // Reports whether the dialog descriptor table hijack hook is active.
    static bool IsDialogTitleHookInstalled()
    {
        return UIManagerDialogTitle::IsInstalled();
    }

    // Creates a native floating dialog (entry 7) with a custom title via the
    // dialog descriptor table hijack approach (Vector A).
    //
    // MUST be called from the game thread (the Python wrapper dispatches via
    // Game.enqueue, so this is always the case). The hook intercepts
    // Ui_CreateEncodedTextFromStringId during dialog creation, substituting
    // the custom title for resource ID 0x337.
    //
    // NOTE: This wrapper previously used GW::GameThread::Enqueue + cv.wait,
    // which caused a deadlock when called from the game thread (the thread
    // would block waiting for itself to process its own queue). Fixed by
    // calling UIManagerDialogTitle::CreateDialogWithTitle directly since we
    // are already on the game thread.
    //
    // Parameters:
    //   parent — parent frame ID (0 = root, 9 = game root container)
    //   title  — wide-string custom title text
    // Returns: frame_id of the created dialog window, or 0 on failure.
    static uint32_t CreateDialogWithTitle(uint32_t parent, const std::wstring& title)
    {
        if (title.empty())
            return 0;
        return UIManagerDialogTitle::CreateDialogWithTitle(parent, title);
    }


    // Destroys a live UI component by frame id.
    static bool DestroyUIComponentByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame)
            return false;
        return GW::UI::DestroyUIComponent(frame);
    }

    // Adds a frame interaction callback to an existing frame.
    static bool AddFrameUIInteractionCallbackByFrameId(
        uint32_t frame_id,
        uintptr_t event_callback,
        uintptr_t wparam = 0)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame)
            return false;
        return GW::UI::AddFrameUIInteractionCallback(
            frame,
            reinterpret_cast<GW::UI::UIInteractionCallback>(event_callback),
            reinterpret_cast<void*>(wparam));
    }

    // Requests a redraw for an existing frame.
    static bool TriggerFrameRedrawByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame)
            return false;
        return GW::UI::TriggerFrameRedraw(frame);
    }

    // Creates a native button frame.
    static uint32_t CreateButtonFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index = 0,
        const std::wstring& name_enc = L"",
        const std::wstring& component_label = L"")
    {
        auto* frame = GW::UI::CreateButtonFrame(
            parent_frame_id,
            component_flags,
            child_index,
            name_enc.empty() ? nullptr : const_cast<wchar_t*>(name_enc.c_str()),
            component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str()));
        return frame ? frame->frame_id : 0;
    }

    // Creates a native text button frame using CtlTextBtnProc (engine-level, no IUi:: wrapper).
    // Unlike IUi::UiCtlBtnProc, this FrameProc has no image list dependency and handles its
    // own creation cleanly. Caption is set from name_enc during creation (msg 0x09 case 9).
    // Uses msg 0x5F for SetText, msg 0x5B for color, msg 0x5D for hover color.
    static uint32_t CreateTextButtonFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index = 0,
        const std::wstring& caption = L"",
        const std::wstring& component_label = L"")
    {
        auto* frame = GW::UI::CreateTextButtonFrame(
            parent_frame_id,
            component_flags,
            child_index,
            caption.empty() ? nullptr : const_cast<wchar_t*>(caption.c_str()),
            component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str()));
        return frame ? frame->frame_id : 0;
    }

    // Creates a native checkbox frame.
    static uint32_t CreateCheckboxFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index = 0,
        const std::wstring& name_enc = L"",
        const std::wstring& component_label = L"")
    {
        auto* frame = GW::UI::CreateCheckboxFrame(
            parent_frame_id,
            component_flags,
            child_index,
            name_enc.empty() ? nullptr : const_cast<wchar_t*>(name_enc.c_str()),
            component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str()));
        return frame ? frame->frame_id : 0;
    }

    // Creates a native scrollable frame.
    static uint32_t CreateScrollableFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index = 0,
        uintptr_t page_context = 0,
        const std::wstring& component_label = L"")
    {
        auto* frame = GW::UI::CreateScrollableFrame(
            parent_frame_id,
            component_flags,
            child_index,
            reinterpret_cast<void*>(page_context),
            component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str()));
        return frame ? frame->frame_id : 0;
    }

    // Creates a native text-label frame from an encoded label payload.
    static uint32_t CreateTextLabelFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index = 0,
        const std::wstring& name_enc = L"",
        const std::wstring& component_label = L"")
    {
        std::wstring owned_name_enc = name_enc;
        auto* frame = GW::UI::CreateTextLabelFrame(
            parent_frame_id,
            component_flags,
            child_index,
            owned_name_enc.empty() ? nullptr : const_cast<wchar_t*>(owned_name_enc.c_str()),
            component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str()));
        if (!frame)
            return 0;
        if (!owned_name_enc.empty()) {
            std::lock_guard<std::mutex> lock(g_created_text_label_payloads_mutex);
            g_created_text_label_payloads[frame->frame_id] = std::move(owned_name_enc);
        }
        return frame->frame_id;
    }

    // Creates a native dropdown frame.
    static uint32_t CreateDropdownFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags = 0x300,
        uint32_t child_index = 0,
        const std::wstring& component_label = L"")
    {
        auto* frame = GW::DropdownFrame::Create(
            parent_frame_id,
            component_flags,
            child_index,
            component_label.empty() ? nullptr : component_label.c_str());
        return frame ? frame->frame_id : 0;
    }

    // Creates a native slider frame.
    static uint32_t CreateSliderFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags = 0,
        uint32_t child_index = 0,
        const std::wstring& component_label = L"")
    {
        auto* frame = GW::SliderFrame::Create(
            parent_frame_id,
            component_flags,
            child_index,
            component_label.empty() ? nullptr : component_label.c_str());
        return frame ? frame->frame_id : 0;
    }

    // Creates a native editable text frame.
    static uint32_t CreateEditableTextFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags = 0,
        uint32_t child_index = 0,
        const std::wstring& component_label = L"")
    {
        auto* frame = GW::EditableTextFrame::Create(
            parent_frame_id,
            component_flags,
            child_index,
            component_label.empty() ? nullptr : component_label.c_str());
        return frame ? frame->frame_id : 0;
    }

    // Creates a native progress bar frame.
    static uint32_t CreateProgressBarByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags = 0x300,
        uint32_t child_index = 0,
        const std::wstring& component_label = L"")
    {
        auto* frame = GW::ProgressBar::Create(
            parent_frame_id,
            component_flags,
            child_index,
            component_label.empty() ? nullptr : component_label.c_str());
        return frame ? frame->frame_id : 0;
    }

    // Creates a native tabs frame.
    static uint32_t CreateTabsFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags = 0x40000,
        uint32_t child_index = 0,
        const std::wstring& component_label = L"")
    {
        auto* frame = GW::TabsFrame::Create(
            parent_frame_id,
            component_flags,
            child_index,
            component_label.empty() ? nullptr : component_label.c_str());
        return frame ? frame->frame_id : 0;
    }

    static std::wstring GetButtonLabelByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::ButtonFrame*>(GW::UI::GetFrameById(frame_id));
        if (!frame) {
            return {};
        }
        const wchar_t* label = nullptr;
        return frame->GetLabel(&label) && label ? std::wstring(label) : std::wstring();
    }

    static bool SetButtonLabelByFrameId(uint32_t frame_id, const std::wstring& enc_label) {
        auto* frame = reinterpret_cast<GW::ButtonFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetLabel(enc_label.empty() ? nullptr : enc_label.c_str());
    }

    static bool ButtonMouseActionByFrameId(uint32_t frame_id, uint32_t action) {
        auto* frame = reinterpret_cast<GW::ButtonFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->MouseAction(static_cast<GW::UI::UIPacket::ActionState>(action));
    }

    // Queries whether a button frame is currently in the pushed (pressed) state.
    // Sends FrameMsg 0x59 (CtlBtnIsPushed) to the button's FrameProc.
    // Returns true if the button is currently pushed down, false otherwise.
    static bool IsButtonPushedByFrameId(uint32_t frame_id) {
        auto* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;
        uint32_t result = 0;
        GW::UI::SendFrameUIMessage(
            frame,
            GW::UI::UIMessage(static_cast<uint32_t>(0x59)),
            nullptr,
            &result);
        return result != 0;
    }

    static uint32_t AddTabByFrameId(uint32_t tabs_frame_id, const std::wstring& tab_name_enc, uint32_t flags, uint32_t child_index, uintptr_t callback = 0, uintptr_t wparam = 0) {
        auto* frame = reinterpret_cast<GW::TabsFrame*>(GW::UI::GetFrameById(tabs_frame_id));
        if (!frame) {
            return 0;
        }
        auto* tab = frame->AddTab(
            tab_name_enc.empty() ? nullptr : tab_name_enc.c_str(),
            flags,
            child_index,
            reinterpret_cast<GW::UI::UIInteractionCallback>(callback),
            reinterpret_cast<void*>(wparam));
        return tab ? tab->frame_id : 0;
    }

    static bool DisableTabByFrameId(uint32_t tabs_frame_id, uint32_t tab_id) {
        auto* frame = reinterpret_cast<GW::TabsFrame*>(GW::UI::GetFrameById(tabs_frame_id));
        return frame && frame->DisableTab(tab_id);
    }

    static bool EnableTabByFrameId(uint32_t tabs_frame_id, uint32_t tab_id) {
        auto* frame = reinterpret_cast<GW::TabsFrame*>(GW::UI::GetFrameById(tabs_frame_id));
        return frame && frame->EnableTab(tab_id);
    }

    static bool RemoveTabByFrameId(uint32_t tabs_frame_id, uint32_t tab_id) {
        auto* frame = reinterpret_cast<GW::TabsFrame*>(GW::UI::GetFrameById(tabs_frame_id));
        return frame && frame->RemoveTab(tab_id);
    }

    static uint32_t GetCurrentTabIndexByFrameId(uint32_t tabs_frame_id) {
        auto* frame = reinterpret_cast<GW::TabsFrame*>(GW::UI::GetFrameById(tabs_frame_id));
        uint32_t tab_id = 0;
        return frame && frame->GetCurrentTabIndex(&tab_id) ? tab_id : 0;
    }

    static uint32_t GetTabFrameIdByFrameId(uint32_t tabs_frame_id, uint32_t tab_id) {
        auto* frame = reinterpret_cast<GW::TabsFrame*>(GW::UI::GetFrameById(tabs_frame_id));
        uint32_t frame_id = 0;
        return frame && frame->GetTabFrameId(tab_id, &frame_id) ? frame_id : 0;
    }

    static uint32_t GetIsTabEnabledByFrameId(uint32_t tabs_frame_id, uint32_t tab_id) {
        auto* frame = reinterpret_cast<GW::TabsFrame*>(GW::UI::GetFrameById(tabs_frame_id));
        uint32_t enabled = 0;
        return frame && frame->GetIsTabEnabled(tab_id, &enabled) ? enabled : 0;
    }

    static uint32_t GetTabByLabelByFrameId(uint32_t tabs_frame_id, const std::wstring& label) {
        auto* frame = reinterpret_cast<GW::TabsFrame*>(GW::UI::GetFrameById(tabs_frame_id));
        auto* tab = frame ? frame->GetTabByLabel(label.empty() ? nullptr : label.c_str()) : nullptr;
        return tab ? tab->frame_id : 0;
    }

    static uint32_t GetCurrentTabByFrameId(uint32_t tabs_frame_id) {
        auto* frame = reinterpret_cast<GW::TabsFrame*>(GW::UI::GetFrameById(tabs_frame_id));
        auto* tab = frame ? frame->GetCurrentTab() : nullptr;
        return tab ? tab->frame_id : 0;
    }

    static bool ChooseTabByTabFrameId(uint32_t tabs_frame_id, uint32_t tab_frame_id) {
        auto* frame = reinterpret_cast<GW::TabsFrame*>(GW::UI::GetFrameById(tabs_frame_id));
        auto* tab = GW::UI::GetFrameById(tab_frame_id);
        return frame && frame->ChooseTab(tab);
    }

    static bool ChooseTabByIndexByFrameId(uint32_t tabs_frame_id, uint32_t tab_index) {
        auto* frame = reinterpret_cast<GW::TabsFrame*>(GW::UI::GetFrameById(tabs_frame_id));
        return frame && frame->ChooseTab(tab_index);
    }

    static uint32_t GetTabButtonByFrameId(uint32_t tabs_frame_id, uint32_t tab_frame_id) {
        auto* frame = reinterpret_cast<GW::TabsFrame*>(GW::UI::GetFrameById(tabs_frame_id));
        auto* tab = GW::UI::GetFrameById(tab_frame_id);
        auto* button = frame ? frame->GetTabButton(tab) : nullptr;
        return button ? button->frame_id : 0;
    }

    static bool SetScrollableSortHandlerByFrameId(uint32_t frame_id, uintptr_t handler) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetSortHandler(reinterpret_cast<GW::ScrollableFrame::SortHandler_pt>(handler));
    }

    static uintptr_t GetScrollableSortHandlerByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        return frame ? reinterpret_cast<uintptr_t>(frame->GetSortHandler()) : 0;
    }

    static bool ClearScrollableItemsByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->ClearItems();
    }

    static bool RemoveScrollableItemByFrameId(uint32_t frame_id, uint32_t child_index) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->RemoveItem(child_index);
    }

    static bool AddScrollableItemByFrameId(uint32_t frame_id, uint32_t flags, uint32_t child_index, uintptr_t callback = 0) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->AddItem(flags, child_index, reinterpret_cast<GW::UI::UIInteractionCallback>(callback));
    }

    static uint32_t GetScrollableItemFrameIdByFrameId(uint32_t frame_id, uint32_t child_index) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        return frame ? frame->GetItemFrameId(child_index) : 0;
    }

    static uint32_t GetScrollableSelectedValueByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        uint32_t value = 0;
        return frame && frame->GetSelectedValue(&value) ? value : 0;
    }

    static uint32_t GetScrollableFirstChildFrameIdByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        return frame ? frame->GetFirstChildFrameId() : 0;
    }

    static uint32_t GetScrollableNextChildFrameIdByFrameId(uint32_t frame_id, uint32_t current_child_frame_id) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        return frame ? frame->GetNextChildFrameId(current_child_frame_id) : 0;
    }

    static uint32_t GetScrollableLastChildFrameIdByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        return frame ? frame->GetLastChildFrameId() : 0;
    }

    static uint32_t GetScrollablePrevChildFrameIdByFrameId(uint32_t frame_id, uint32_t current_child_frame_id) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        return frame ? frame->GetPrevChildFrameId(current_child_frame_id) : 0;
    }

    static std::vector<float> GetScrollableItemRectByFrameId(uint32_t frame_id, uint32_t child_index) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        float rect[4] = {};
        if (!(frame && frame->GetItemRect(child_index, rect))) {
            return {};
        }
        return { rect[0], rect[1], rect[2], rect[3] };
    }

    static uint32_t GetScrollableCountByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        uint32_t count = 0;
        return frame && frame->GetCount(&count) ? count : 0;
    }

    static std::vector<uint32_t> GetScrollableItemsByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        if (!frame) {
            return {};
        }
        uint32_t count = frame->GetItems(nullptr, 0);
        std::vector<uint32_t> items(count);
        if (count) {
            frame->GetItems(items.data(), count);
        }
        return items;
    }

    static uint32_t GetScrollablePageByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        auto* page = frame ? frame->GetPage() : nullptr;
        return page ? page->frame_id : 0;
    }

    static uint32_t SetScrollablePageByFrameId(uint32_t frame_id, uintptr_t page_context) {
        auto* frame = reinterpret_cast<GW::ScrollableFrame*>(GW::UI::GetFrameById(frame_id));
        auto* page = frame ? frame->SetPage(reinterpret_cast<GW::ScrollableFrame::ScrollablePageContext*>(page_context)) : nullptr;
        return page ? page->frame_id : 0;
    }

    static std::wstring GetEditableTextValueByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::EditableTextFrame*>(GW::UI::GetFrameById(frame_id));
        const auto* value = frame ? frame->GetValue() : nullptr;
        return value ? std::wstring(value) : std::wstring();
    }

    static bool SetEditableTextValueByFrameId(uint32_t frame_id, const std::wstring& value) {
        auto* frame = reinterpret_cast<GW::EditableTextFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetValue(value.empty() ? nullptr : value.c_str());
    }

    static bool SetEditableTextMaxLengthByFrameId(uint32_t frame_id, uint32_t max_length) {
        auto* frame = reinterpret_cast<GW::EditableTextFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetMaxLength(max_length);
    }

    static bool IsEditableTextReadOnlyByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::EditableTextFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->IsReadOnly();
    }

    static bool SetEditableTextReadOnlyByFrameId(uint32_t frame_id, bool read_only) {
        auto* frame = reinterpret_cast<GW::EditableTextFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetReadOnly(read_only);
    }

    static uint32_t GetProgressBarValueByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::ProgressBar*>(GW::UI::GetFrameById(frame_id));
        return frame ? frame->GetValue() : 0;
    }

    static bool SetProgressBarValueByFrameId(uint32_t frame_id, uint32_t value) {
        auto* frame = reinterpret_cast<GW::ProgressBar*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetValue(value);
    }

    static bool SetProgressBarMaxByFrameId(uint32_t frame_id, uint32_t value) {
        auto* frame = reinterpret_cast<GW::ProgressBar*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetMax(value);
    }

    static bool SetProgressBarColorIdByFrameId(uint32_t frame_id, uint32_t color_id) {
        auto* frame = reinterpret_cast<GW::ProgressBar*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetColorId(color_id);
    }

    static bool SetProgressBarStyleByFrameId(uint32_t frame_id, uint32_t style) {
        auto* frame = reinterpret_cast<GW::ProgressBar*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetStyle(static_cast<GW::ProgressBar::ProgressBarStyle>(style));
    }

    static bool IsCheckboxCheckedByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::CheckboxFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->IsChecked();
    }

    static bool SetCheckboxCheckedByFrameId(uint32_t frame_id, bool checked) {
        auto* frame = reinterpret_cast<GW::CheckboxFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetChecked(checked);
    }

    static uint32_t GetCheckboxValueByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::CheckboxFrame*>(GW::UI::GetFrameById(frame_id));
        return frame ? frame->GetValue() : 0;
    }

    static bool SetCheckboxValueByFrameId(uint32_t frame_id, uint32_t value) {
        auto* frame = reinterpret_cast<GW::CheckboxFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetValue(value);
    }

    static std::vector<uint32_t> GetDropdownOptionsByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::DropdownFrame*>(GW::UI::GetFrameById(frame_id));
        return frame ? frame->GetOptions() : std::vector<uint32_t>();
    }

    static bool SelectDropdownOptionByFrameId(uint32_t frame_id, uint32_t value) {
        auto* frame = reinterpret_cast<GW::DropdownFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SelectOption(value);
    }

    static bool SelectDropdownIndexByFrameId(uint32_t frame_id, uint32_t index) {
        auto* frame = reinterpret_cast<GW::DropdownFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SelectIndex(index);
    }

    static bool AddDropdownOptionByFrameId(uint32_t frame_id, const std::wstring& label_enc, uint32_t value) {
        auto* frame = reinterpret_cast<GW::DropdownFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->AddOption(label_enc.empty() ? nullptr : label_enc.c_str(), value);
    }

    static uint32_t GetDropdownCountByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::DropdownFrame*>(GW::UI::GetFrameById(frame_id));
        uint32_t count = 0;
        return frame && frame->GetCount(&count) ? count : 0;
    }

    static uint32_t GetDropdownOptionValueByFrameId(uint32_t frame_id, uint32_t index) {
        auto* frame = reinterpret_cast<GW::DropdownFrame*>(GW::UI::GetFrameById(frame_id));
        uint32_t value = 0;
        return frame && frame->GetOptionValue(index, &value) ? value : 0;
    }

    static uint32_t GetDropdownOptionIndexByFrameId(uint32_t frame_id, uint32_t value) {
        auto* frame = reinterpret_cast<GW::DropdownFrame*>(GW::UI::GetFrameById(frame_id));
        uint32_t index = 0;
        return frame && frame->GetOptionIndex(value, &index) ? index : 0;
    }

    static uint32_t GetDropdownSelectedIndexByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::DropdownFrame*>(GW::UI::GetFrameById(frame_id));
        uint32_t index = 0;
        return frame && frame->GetSelectedIndex(&index) ? index : 0;
    }

    static bool DropdownHasValueMappingByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::DropdownFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->HasValueMapping();
    }

    static uint32_t GetDropdownValueByFrameId(uint32_t frame_id) {
        auto* frame = reinterpret_cast<GW::DropdownFrame*>(GW::UI::GetFrameById(frame_id));
        return frame ? frame->GetValue() : 0;
    }

    static bool SetDropdownValueByFrameId(uint32_t frame_id, uint32_t value) {
        auto* frame = reinterpret_cast<GW::DropdownFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetValue(value);
    }

    static uint32_t GetSliderValueByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame) return 0;
        uint32_t value = 0;
        GW::UI::SendFrameUIMessage(
            frame, static_cast<GW::UI::UIMessage>(0x58),
            &value, nullptr);
        return value;
    }

    static bool SetSliderValueByFrameId(uint32_t frame_id, uint32_t value) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame) return false;
        return GW::UI::SendFrameUIMessage(
            frame, static_cast<GW::UI::UIMessage>(0x57),
            reinterpret_cast<void*>(static_cast<uintptr_t>(value)), nullptr);
    }

    // Sets the valid integer range for a slider (msg 0x56).
    // MUST be called before SetSliderValueByFrameId to avoid divide-by-zero.
    static bool SetSliderRangeByFrameId(uint32_t frame_id, uint32_t min_val, uint32_t max_val) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame) return false;
        struct { uint32_t min; uint32_t max; } range = { min_val, max_val };
        return GW::UI::SendFrameUIMessage(
            frame, static_cast<GW::UI::UIMessage>(0x56),
            &range, nullptr);
    }

    // Sets a frame's size via the native FrameSetSize (EXE 0x0062f9a0).
    // Takes a frame_id and width/height in float. Uses Coord2f internally.
    // Resolved via FindAssertion("FrApi.cpp", "frameId", 0x880) — same FrApi anchoring
    // approach as FrameSetLayer and FrameSetPosition.
    static bool FrameSetSizeByFrameId(uint32_t frame_id, float width, float height)
    {
        using FrameSetSize_pt = void(__cdecl*)(uint32_t, Coord2f*);

        static FrameSetSize_pt fn = nullptr;
        if (!fn) {
            const auto addr = ResolveFrameSetSize();
            if (!addr)
                return false;
            fn = reinterpret_cast<FrameSetSize_pt>(addr);
        }

        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;

        Coord2f size = { width, height };
        fn(frame_id, &size);
        return true;
    }

    // Creates a slider frame with full initialization: position, size, range, and value.
    // One-step creation that avoids the divide-by-zero crash from calling SetValue before
    // SetRange. Uses direct SendFrameUIMessage for range/value to avoid GWCA struct cast.
    //
    // Steps:
    //   1. GW::SliderFrame::Create(parent_frame_id, 0x300, child_index, label)
    //   2. FrameSetPosition(frame_id, &{x, y}) via ResolveFrameSetPositionCoord2f
    //   3. FrameSetSize(frame_id, &{w, h}) via ResolveFrameSetSize
    //   4. SendFrameUIMessage(0x56, &{0, 100}) — SetRange(0, 100)
    // Creates a text-label frame by first encoding plain text into a literal payload.
    static uint32_t CreateTextLabelFrameWithPlainTextByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index = 0,
        const std::wstring& plain_text = L"",
        const std::wstring& component_label = L"")
    {
        std::wstring encoded_name_enc = BuildStandaloneLiteralEncodedTextPayload(plain_text);
        return CreateTextLabelFrameByFrameId(
            parent_frame_id,
            component_flags,
            child_index,
            encoded_name_enc,
            component_label);
    }
    // Creates a text-label frame using another text label as the encoded template source.
    static uint32_t CreateTextLabelFrameFromTemplateByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index,
        uint32_t template_frame_id,
        const std::wstring& plain_text = L"",
        const std::wstring& component_label = L"")
    {
        auto* template_frame = reinterpret_cast<GW::TextLabelFrame*>(GW::UI::GetFrameById(template_frame_id));
        if (!(template_frame && template_frame->IsCreated()))
            return 0;
        const wchar_t* current_text = template_frame->GetEncodedLabel();
        if (!current_text || !current_text[0])
            return 0;
        std::wstring encoded_name_enc(current_text);
        if (!plain_text.empty()) {
            encoded_name_enc.push_back(static_cast<wchar_t>(0x0002));
            encoded_name_enc.push_back(static_cast<wchar_t>(0x0108));
            encoded_name_enc.push_back(static_cast<wchar_t>(0x0107));
            encoded_name_enc += plain_text;
            encoded_name_enc.push_back(static_cast<wchar_t>(0x0001));
        }
        return CreateTextLabelFrameByFrameId(
            parent_frame_id,
            component_flags,
            child_index,
            encoded_name_enc,
            component_label);
    }
    // Returns a diagnostic dictionary describing how a template-derived text payload was built.
    static py::dict GetTextLabelCreatePayloadDiagnosticsByTemplateFrameId(
        uint32_t template_frame_id,
        const std::wstring& plain_text = L"")
    {
        py::dict result;
        result["template_frame_id"] = template_frame_id;
        result["plain_text"] = plain_text;
        auto* template_frame = reinterpret_cast<GW::TextLabelFrame*>(GW::UI::GetFrameById(template_frame_id));
        const bool template_exists = template_frame != nullptr;
        const bool template_created = template_exists && template_frame->IsCreated();
        result["template_exists"] = template_exists;
        result["template_created"] = template_created;
        if (!(template_exists && template_created)) {
            result["template_encoded"] = std::wstring();
            result["template_valid"] = false;
            result["constructed_encoded"] = std::wstring();
            result["constructed_valid"] = false;
            result["constructed_decoded"] = std::wstring();
            return result;
        }
        const wchar_t* current_text = template_frame->GetEncodedLabel();
        const bool template_has_text = current_text && current_text[0];
        result["template_has_text"] = template_has_text;
        std::wstring template_encoded = template_has_text ? std::wstring(current_text) : std::wstring();
        result["template_encoded"] = template_encoded;
        const bool template_valid = template_has_text && GW::UI::IsValidEncStr(current_text);
        result["template_valid"] = template_valid;
        std::wstring constructed_encoded = template_encoded;
        if (!plain_text.empty()) {
            constructed_encoded.push_back(static_cast<wchar_t>(0x0002));
            constructed_encoded.push_back(static_cast<wchar_t>(0x0108));
            constructed_encoded.push_back(static_cast<wchar_t>(0x0107));
            constructed_encoded += plain_text;
            constructed_encoded.push_back(static_cast<wchar_t>(0x0001));
        }
        result["constructed_encoded"] = constructed_encoded;
        const bool constructed_valid = !constructed_encoded.empty() && GW::UI::IsValidEncStr(constructed_encoded.c_str());
        result["constructed_valid"] = constructed_valid;
        std::wstring constructed_decoded;
        if (constructed_valid) {
            GW::UI::AsyncDecodeStr(constructed_encoded.c_str(), &constructed_decoded);
        }
        result["constructed_decoded"] = constructed_decoded;
        return result;
    }

    // Returns a diagnostic dictionary for a literal-text payload built without a template frame.
    static py::dict GetTextLabelLiteralCreatePayloadDiagnostics(const std::wstring& plain_text = L"")
    {
        py::dict result;
        result["plain_text"] = plain_text;
        std::wstring constructed_encoded = BuildStandaloneLiteralEncodedTextPayload(plain_text);
        result["constructed_encoded"] = constructed_encoded;
        const bool constructed_valid = !constructed_encoded.empty() && GW::UI::IsValidEncStr(constructed_encoded.c_str());
        result["constructed_valid"] = constructed_valid;
        std::wstring constructed_decoded;
        if (constructed_valid) {
            GW::UI::AsyncDecodeStr(constructed_encoded.c_str(), &constructed_decoded);
        }
        result["constructed_decoded"] = constructed_decoded;
        return result;
    }

    /*
    static uint64_t RegisterCreateUIComponentCallback(const py::function& callback, int altitude = -0x8000)
    {
        if (callback.is_none())
            return 0;
        auto state = std::make_shared<CreateUIComponentCallbackState>();
        {
            std::lock_guard<std::mutex> lock(g_create_ui_component_callback_mutex);
            state->handle = g_next_create_ui_component_callback_handle++;
            state->callback = callback;
            g_create_ui_component_callbacks[state->handle] = state;
        }
        GW::UI::RegisterCreateUIComponentCallback(
            &state->entry,
            [state](GW::UI::CreateUIComponentPacket* packet) {
                if (!(state && packet))
                    return;
                py::gil_scoped_acquire gil;
                try {
                    state->callback(
                        packet->frame_id,
                        packet->component_flags,
                        packet->tab_index,
                        reinterpret_cast<uintptr_t>(packet->event_callback),
                        packet->name_enc ? std::wstring(packet->name_enc) : std::wstring(),
                        packet->component_label ? std::wstring(packet->component_label) : std::wstring());
                } catch (const py::error_already_set&) {
                }
            },
            altitude);
        return state->handle;
    }

    static bool RemoveCreateUIComponentCallback(uint64_t handle)
    {
        std::shared_ptr<CreateUIComponentCallbackState> state;
        {
            std::lock_guard<std::mutex> lock(g_create_ui_component_callback_mutex);
            const auto it = g_create_ui_component_callbacks.find(handle);
            if (it == g_create_ui_component_callbacks.end())
                return false;
            state = it->second;
            g_create_ui_component_callbacks.erase(it);
        }
        GW::UI::RemoveCreateUIComponentCallback(&state->entry);
        return true;
    }
    */




    // Enqueues a native button click on a button frame.
	static void ButtonClick(uint32_t frame_id) {
        GW::GameThread::Enqueue([frame_id]() {
            auto* frame = reinterpret_cast<GW::ButtonFrame*>(GW::UI::GetFrameById(frame_id));
            if (frame)
                frame->Click();
            });
        
	}

    // Enqueues a native button double-click on a button frame.
    static void ButtonDoubleClick(uint32_t frame_id) {
        GW::GameThread::Enqueue([frame_id]() {
            auto* frame = reinterpret_cast<GW::ButtonFrame*>(GW::UI::GetFrameById(frame_id));
            if (frame)
                frame->DoubleClick();
            });
    }

    // Enqueues the low-level mouse-action test helper.
    static void TestMouseAction(uint32_t frame_id, uint32_t current_state, uint32_t wparam_value = 0, uint32_t lparam=0) {
        GW::GameThread::Enqueue([frame_id, current_state, wparam_value, lparam]() {
			GW::UI::TestMouseAction(frame_id, current_state, wparam_value, lparam);
            });

    }

    // Enqueues the low-level mouse-click-action test helper.
    static void TestMouseClickAction(uint32_t frame_id, uint32_t current_state, uint32_t wparam_value = 0, uint32_t lparam = 0) {
        GW::GameThread::Enqueue([frame_id, current_state, wparam_value, lparam]() {
            GW::UI::TestMouseClickAction(frame_id, current_state, wparam_value, lparam);
            });

    }

    // Returns the root UI frame id.
	static uint32_t GetRootFrameID() {
		GW::UI::Frame* frame = GW::UI::GetRootFrame();
		if (!frame) {
			return 0;
		}
		return frame->frame_id;
	}

    // Reports whether the world map UI is currently visible.
	static bool IsWorldMapShowing() {
		return GW::UI::GetIsWorldMapShowing();
	}

    // Reports whether the game is currently drawing the UI.
    static bool IsUIDrawn() {
        return GW::UI::GetIsUIDrawn();
    }

    // Decodes an encoded GW string through the game's async decoder.
    static std::string AsyncDecodeStr(const std::string& enc_str) {
        std::wstring winput(enc_str.begin(), enc_str.end());
        std::wstring output;
        GW::UI::AsyncDecodeStr(winput.c_str(), &output);
        return std::string(output.begin(), output.end());
    }

    // Validates an encoded GW string represented as a narrow string.
    static bool IsValidEncStr(const std::string& enc_str) {
        std::wstring winput(enc_str.begin(), enc_str.end());
        return GW::UI::IsValidEncStr(winput.c_str());
    }

    // Validates an encoded GW string represented as raw wchar bytes.
    static bool IsValidEncBytes(const py::bytes& enc_bytes) {
        const std::string raw = enc_bytes;
        if (raw.empty() || (raw.size() % sizeof(wchar_t)) != 0)
            return false;
        const auto* wide = reinterpret_cast<const wchar_t*>(raw.data());
        const size_t wide_len = raw.size() / sizeof(wchar_t);
        if (wide[wide_len - 1] != 0x0000)
            return false;
        return GW::UI::IsValidEncStr(wide);
    }

    // Encodes a uint32 value into Guild Wars' packed encoded-string form.
    static std::string UInt32ToEncStr(uint32_t value) {
        wchar_t buffer[8] = {0};
        if (!GW::UI::UInt32ToEncStr(value, buffer, _countof(buffer))) {
            return "";
        }
        std::wstring woutput(buffer);
        return std::string(woutput.begin(), woutput.end());
    }

    // Decodes a Guild Wars packed encoded-string value into uint32.
    static uint32_t EncStrToUInt32(const std::string& enc_str) {
        std::wstring winput(enc_str.begin(), enc_str.end());
        return GW::UI::EncStrToUInt32(winput.c_str());
    }

    // Toggles the client's open-links behavior on the game thread.
    static void SetOpenLinks(bool toggle) {
        GW::GameThread::Enqueue([toggle]() {
            GW::UI::SetOpenLinks(toggle);
        });
    }

    // Draws a polyline on the compass for a session id.
    static bool DrawOnCompass(
        uint32_t session_id,
        const std::vector<std::pair<int, int>>& points)
    {
        if (points.empty())
            return false;
        std::vector<GW::UI::CompassPoint> compass_points;
        compass_points.reserve(points.size());
        for (const auto& point : points) {
            compass_points.emplace_back(point.first, point.second);
        }
        return GW::UI::DrawOnCompass(
            session_id,
            static_cast<unsigned>(compass_points.size()),
            compass_points.data());
    }

    // Returns the current tooltip pointer for low-level inspection.
    static uintptr_t GetCurrentTooltipAddress() {
        return reinterpret_cast<uintptr_t>(GW::UI::GetCurrentTooltip());
    }

    // Returns the valid option values for an enum-style preference.
    static std::vector<uint32_t> GetPreferenceOptions(uint32_t pref) {
        GW::UI::EnumPreference pref_enum = static_cast<GW::UI::EnumPreference>(pref);

        uint32_t* options_ptr = nullptr;
        uint32_t count = GW::UI::GetPreferenceOptions(pref_enum, &options_ptr);

        std::vector<uint32_t> result;
        if (options_ptr && count > 0) {
            result.assign(options_ptr, options_ptr + count);
        }
        return result;
    }



    // Returns the current value of an enum preference.
	static uint32_t GetEnumPreference(uint32_t pref) {
		GW::UI::EnumPreference pref_enum = static_cast<GW::UI::EnumPreference>(pref);
		return GW::UI::GetPreference(pref_enum);
	}

    // Returns the current value of a numeric preference.
	static uint32_t GetIntPreference(uint32_t pref) {
		GW::UI::NumberPreference pref_enum = static_cast<GW::UI::NumberPreference>(pref);
		return GW::UI::GetPreference(pref_enum);
	}

    // Returns the current value of a string preference.
	static std::string GetStringPreference(uint32_t pref) {
		GW::UI::StringPreference pref_enum = static_cast<GW::UI::StringPreference>(pref);
		wchar_t* str = GW::UI::GetPreference(pref_enum);
		if (!str) {
			return "";
		}
		std::wstring wstr(str);
		std::string str_utf8(wstr.begin(), wstr.end());
		return str_utf8;

	}

    // Returns the current value of a flag preference.
	static bool GetBoolPreference(uint32_t pref) {
		GW::UI::FlagPreference pref_enum = static_cast<GW::UI::FlagPreference>(pref);
		return GW::UI::GetPreference(pref_enum);
	}

    // Sets an enum preference on the game thread.
    static void SetEnumPreference(uint32_t pref, uint32_t value) {
        GW::GameThread::Enqueue([pref, value]() {
            GW::UI::EnumPreference pref_enum = static_cast<GW::UI::EnumPreference>(pref);
            GW::UI::SetPreference(pref_enum, value);
            });	
	}

    // Sets a numeric preference on the game thread.
	static void SetIntPreference(uint32_t pref, uint32_t value) {
		GW::GameThread::Enqueue([pref, value]() {
			GW::UI::NumberPreference pref_enum = static_cast<GW::UI::NumberPreference>(pref);
			GW::UI::SetPreference(pref_enum, value);
			});
	}

    // Sets a string preference on the game thread.
	static void SetStringPreference(uint32_t pref, const std::string& value) {
		GW::GameThread::Enqueue([pref, value]() {
			GW::UI::StringPreference pref_enum = static_cast<GW::UI::StringPreference>(pref);
			std::wstring wstr(value.begin(), value.end());
			wchar_t* wstr_ptr = const_cast<wchar_t*>(wstr.c_str());
			GW::UI::SetPreference(pref_enum, wstr_ptr);
			});
	}

    // Sets a flag preference on the game thread.
	static void SetBoolPreference(uint32_t pref, bool value) {
		GW::GameThread::Enqueue([pref, value]() {
			GW::UI::FlagPreference pref_enum = static_cast<GW::UI::FlagPreference>(pref);
			GW::UI::SetPreference(pref_enum, value);
			});
	}

    // Returns the current global frame limit.
	static uint32_t GetFrameLimit() {
		return GW::UI::GetFrameLimit();
	}

    // Sets the global frame limit on the game thread.
    static void SetFrameLimit(uint32_t value) {
        GW::GameThread::Enqueue([value]() {
            GW::UI::SetFrameLimit(value);
            });

	}

    // Returns the raw key remapping table.
	static std::vector<uint32_t> GetKeyMappings() {
        // NB: This address is fond twice, we only care about the first.
        uint32_t* key_mappings_array = nullptr;
        uint32_t key_mappings_array_length = 0x75;
        uintptr_t address = GW::Scanner::FindAssertion("FrKey.cpp", "count == arrsize(s_remapTable)", 0, 0x13);
        Logger::AssertAddress("key_mappings", address);
        if (address && GW::Scanner::IsValidPtr(*(uintptr_t*)address)) {
            key_mappings_array = *(uint32_t**)address;
        }
		std::vector<uint32_t> result;
		if (key_mappings_array) {
			result.assign(key_mappings_array, key_mappings_array + key_mappings_array_length);
		}
		return result;
	}

    // Writes a replacement key remapping table.
	static void SetKeyMappings(const std::vector<uint32_t>& mappings) {
		GW::GameThread::Enqueue([mappings]() {
			// NB: This address is fond twice, we only care about the first.
			uint32_t* key_mappings_array = nullptr;
			uint32_t key_mappings_array_length = 0x75;
			uintptr_t address = GW::Scanner::FindAssertion("FrKey.cpp", "count == arrsize(s_remapTable)", 0, 0x13);
			Logger::AssertAddress("key_mappings", address);
			if (address && GW::Scanner::IsValidPtr(*(uintptr_t*)address)) {
				key_mappings_array = *(uint32_t**)address;
			}
			if (key_mappings_array) {
				size_t count = std::min(static_cast<size_t>(key_mappings_array_length), mappings.size());
				std::copy(mappings.begin(), mappings.begin() + count, key_mappings_array);
			}
			});
	}



    // Returns the raw frame id array tracked by the client.
	static std::vector <uint32_t> GetFrameArray() {
		return GW::UI::GetFrameArray();
	}

    // Press and hold a key (down only)
    static void KeyDown(uint32_t key, uint32_t frame_id) {
        GW::GameThread::Enqueue([key, frame_id]() {
            // Convert the integer into a ControlAction enum value
            GW::UI::ControlAction key_action = static_cast<GW::UI::ControlAction>(key);

            GW::UI::Frame* frame = nullptr;
            if (frame_id != 0) {
                frame = GW::UI::GetFrameById(frame_id);
            }

            // Call the actual UI function
            GW::UI::Keydown(key_action, frame);
            });
    }

    // Release a key (up only)
    static void KeyUp(uint32_t key, uint32_t frame_id) {
        GW::GameThread::Enqueue([key, frame_id]() {
            GW::UI::ControlAction key_action = static_cast<GW::UI::ControlAction>(key);

            GW::UI::Frame* frame = nullptr;
            if (frame_id != 0) {
                frame = GW::UI::GetFrameById(frame_id);
            }

            GW::UI::Keyup(key_action, frame);
            });
    }

    // Simulate a full keypress (down + up)
    static void KeyPress(uint32_t key, uint32_t frame_id) {
        GW::GameThread::Enqueue([key, frame_id]() {
            GW::UI::ControlAction key_action = static_cast<GW::UI::ControlAction>(key);

            GW::UI::Frame* frame = nullptr;
            if (frame_id != 0) {
                frame = GW::UI::GetFrameById(frame_id);
            }

            GW::UI::Keypress(key_action, frame);
            });
    }

    // Returns the stored window rectangle for a built-in window id.
    static std::vector<uintptr_t> GetWindowPosition(uint32_t window_id) {
        std::vector<uintptr_t> result;
        GW::UI::WindowPosition* position =
            GW::UI::GetWindowPosition(static_cast<GW::UI::WindowID>(window_id));
        if (position) {
            result.push_back(static_cast<uintptr_t>(position->left()));
            result.push_back(static_cast<uintptr_t>(position->top()));
            result.push_back(static_cast<uintptr_t>(position->right()));
            result.push_back(static_cast<uintptr_t>(position->bottom()));
        }
        return result;
    }

    // Reports whether a built-in window id is marked visible.
	static bool IsWindowVisible(uint32_t window_id) {
        GW::UI::WindowPosition* position = GW::UI::GetWindowPosition(static_cast<GW::UI::WindowID>(window_id));
		if (!position) {
			return false;
		}
        return (position->state & 0x1) != 0;
	}

    // Sets the visible state of a built-in window id.
	static void SetWindowVisible(uint32_t window_id, bool is_visible) {
		GW::GameThread::Enqueue([window_id, is_visible]() {
			GW::UI::SetWindowVisible(static_cast<GW::UI::WindowID>(window_id), is_visible);
			});
	}

    // Overwrites the stored window rectangle for a built-in window id.
    static void SetWindowPosition(uint32_t window_id, const std::vector<uintptr_t>& position) {
        GW::GameThread::Enqueue([window_id, position]() {
            if (position.size() < 4) return; // Ensure we have enough data
            GW::UI::WindowPosition* win_pos =
                GW::UI::GetWindowPosition(static_cast<GW::UI::WindowID>(window_id));
            if (!win_pos) return;

            // write back into p1/p2 from the values we accepted (left, top, right, bottom)
            win_pos->p1.x = static_cast<float>(position[0]);
            win_pos->p1.y = static_cast<float>(position[1]);
            win_pos->p2.x = static_cast<float>(position[2]);
            win_pos->p2.y = static_cast<float>(position[3]);

            GW::UI::SetWindowPosition(static_cast<GW::UI::WindowID>(window_id), win_pos);
            });
    }

    // Reports whether shift-screenshot mode is active.
	static bool IsShiftScreenShot() {
		return GW::UI::GetIsShiftScreenShot();
	}

    // =========================================================================
    // Window Contents — Frame List Item Management (2026-06-04)
    // =========================================================================
    // These methods enable filling a CContainerFrame window with scrollable
    // text content via the CtlFrameListCreateItem pipeline.
    //
    // Architecture: CContainerFrame → FrameList (child 0, type 0xAEA) → TextLabels
    //
    // CtlFrameListCreateItem sends msg 0x57 to the frame list, which creates a
    // child frame via FrameCreate with flags|0x300 and the given item proc.
    //
    // FrameNewSubclass registers a subclass proc on a frame for a given msg ID.
    // Used for scrollbar chrome (msg 0x59) on frame lists.

    // Calls the native CtlFrameListCreateItem (EXE 0x00612900) to add an item
    // to a frame list. The itemProc is resolved from GWCA's TextLabelFrame_Callback.
    // The userData must be an encoded text payload (see BuildStandaloneLiteralEncodedTextPayload).
    //
    // Returns the new item's frame ID, or 0 on failure.
    static uint32_t CtlFrameListCreateItemByFrameId(
        uint32_t parent_frame_list_id,
        uint32_t flags,
        uint32_t insert_index,
        uint32_t item_proc,
        const std::wstring& encoded_text)
    {
        const auto fn = ResolveCtlFrameListCreateItem();
        if (!fn) {
            GWCA_ERR("[UI] CtlFrameListCreateItemByFrameId — CtlFrameListCreateItem not resolved");
            return 0;
        }
        if (!parent_frame_list_id) {
            GWCA_ERR("[UI] CtlFrameListCreateItemByFrameId — invalid parent_frame_list_id");
            return 0;
        }
        if (!item_proc) {
            // Default: resolve TextLabelFrame_Callback
            const auto cb = ResolveTextLabelFrameCallback();
            if (!cb) {
                GWCA_ERR("[UI] CtlFrameListCreateItemByFrameId — TextLabelFrame_Callback not resolved");
                return 0;
            }
            item_proc = reinterpret_cast<uint32_t>(cb);
        }
        const void* user_data = encoded_text.empty() ? nullptr : encoded_text.c_str();
        return fn(parent_frame_list_id, flags, insert_index,
                  reinterpret_cast<void*>(static_cast<uintptr_t>(item_proc)),
                  const_cast<void*>(user_data));
    }

    // Calls the native FrameNewSubclass (EXE 0x0062f150) to register a subclass
    // proc on a frame. The subclass proc will be called for messages matching msg_id
    // before the frame's own proc.
    //
    // Returns the subclass handle, or 0 on failure.
    static uint32_t FrameNewSubclassByFrameId(
        uint32_t frame_id,
        uint32_t subclass_proc,
        uint32_t msg_id)
    {
        const auto fn = ::ResolveFrameNewSubclass();
        if (!fn) {
            GWCA_ERR("[UI] FrameNewSubclassByFrameId — FrameNewSubclass not resolved");
            return 0;
        }
        if (!frame_id || !subclass_proc) {
            GWCA_ERR("[UI] FrameNewSubclassByFrameId — invalid arguments");
            return 0;
        }
        return fn(frame_id,
                  reinterpret_cast<void*>(static_cast<uintptr_t>(subclass_proc)),
                  msg_id);
    }

    // Convenience: create a scrollable frame list as a child of the given window,
    // using GWCA's CreateScrollableFrame (which wraps the frame list in a
    // CtlViewProc scrollable container with automatic scrollbars).
    //
    // Returns the scrollable frame's ID, or 0 on failure.
    static uint32_t CreateScrollableContentByFrameId(
        uint32_t window_id,
        uint32_t child_index = 0,
        uint32_t component_flags = 0x20000,
        const std::wstring& component_label = L"")
    {
        return CreateScrollableFrameByFrameId(window_id, component_flags, child_index,
                                             0, component_label);
    }

    // Convenience: add a text label item to a frame list. Encodes the plain text
    // into GW's literal encoded format and calls CtlFrameListCreateItem.
    //
    // Returns the new text label item's frame ID, or 0 on failure.
    static uint32_t AddTextItemToFrameListByFrameId(
        uint32_t frame_list_id,
        const std::wstring& plain_text,
        uint32_t insert_index = 0,
        uint32_t item_flags = 0)
    {
        if (plain_text.empty()) {
            GWCA_ERR("[UI] AddTextItemToFrameListByFrameId — empty text");
            return 0;
        }
        std::wstring encoded = BuildStandaloneLiteralEncodedTextPayload(plain_text);
        return CtlFrameListCreateItemByFrameId(frame_list_id, item_flags,
                                               insert_index, 0, encoded);
    }

    // Convenience: create a titled container window with scrollable text content.
    // One-step: creates window, creates scrollable frame list as child 0,
    // and adds all text items.
    //
    // Returns the window frame ID, or 0 on failure.
    // (Item IDs can be queried via the frame list after creation.)
    static uint32_t CreateScrollableTextWindow(
        float x, float y, float width, float height,
        const std::wstring& title,
        const std::vector<std::wstring>& items)
    {
        // Step 1: Create container window
        const uint32_t window_id = CreateNativeWindow(x, y, width, height, title);
        if (!window_id) {
            GWCA_ERR("[UI] CreateScrollableTextWindow — window creation failed");
            return 0;
        }

        // Step 2: Create scrollable frame list as child 0
        const uint32_t frame_list_id = CreateScrollableContentByFrameId(window_id, 0);
        if (!frame_list_id) {
            GWCA_ERR("[UI] CreateScrollableTextWindow — scrollable content creation failed");
            return 0;
        }

        // Step 3: Add text items
        for (const auto& item : items) {
            AddTextItemToFrameListByFrameId(frame_list_id, item);
        }

        return window_id;
    }

};

// ============================================================================
// UIManagerCNonclient — msg 0x5E Title Dispatch via CNonclient (Vector C)
// ============================================================================
// Instead of hooking the title-creation path (Vector B) or manipulating the
// ExtraData array directly, this namespace sends a frame message 0x5E to the
// CNonclient subobject at frame+0xCC. The native handler stores the encoded
// text, calls TextResolveIssue for async decode, and OnTitleResolved triggers
// CContent::Invalidate — the complete native title chain fires automatically.
//
// Functions resolved via byte patterns (build-independent).
//
// FrameMsg layout for msg 0x5E:
//   +0x00 = frame_ptr (native GW::UI::Frame*)
//   +0x04 = msg_id (0x5E)
//   +0x08 = pointer to CNonclient subobject at frame+0xCC
namespace UIManagerCNonclient {
    using CreateEncodedText_pt = uint32_t(__cdecl*)(int32_t, int32_t, const wchar_t*, int32_t);
    using CNonclientProc_pt = void(__cdecl*)(void*, void*, int32_t*);

    inline CreateEncodedText_pt CreateEncodedText_Func = nullptr;
    inline CNonclientProc_pt CNonclientProc_Func = nullptr;
    // TextResolveIssue — void(uint frame_id, wchar_t* encoded_text, uint context)
    // Starts async text decoding. OnTitleResolved dispatches msg 0x3A to the
    // CNonclient proc which copies decoded text and invalidates the frame.
    using TextResolveIssue_pt = void(__cdecl*)(uint32_t, const wchar_t*, uint32_t);
    inline TextResolveIssue_pt TextResolveIssue_Func = nullptr;

    inline bool ResolveFunctions()
    {
        // Only CreateEncodedText_Func is shared; CNonclientProc and TextResolveIssue
        // have unique patterns that must be resolved inline (no other callers share them).
        if (!CreateEncodedText_Func) {
            CreateEncodedText_Func = reinterpret_cast<CreateEncodedText_pt>(ResolveCreateEncodedText());
        }

        if (!CNonclientProc_Func) {
            // Unique pattern from the message dispatch inside the CNonclient proc:
            //   ADD  EAX, -0x4      ; adjust msg_id to zero-based index
            //   MOV  [EBP-0x58], ESI
            //   CMP  EAX, 0x5E      ; max handled message id
            //   JA   ...            ; jump if above (unhandled)
            // Verified: 1 match in Symbols EXE, 1 match in 05-30-2026 EXE.
            const uintptr_t addr = GW::Scanner::Find(
                "\x83\xC0\xFC\x89\x75\xA8\x83\xF8\x5E\x0F\x87",
                "xxxxxxxxxxx");
            if (addr) {
                CNonclientProc_Func = reinterpret_cast<CNonclientProc_pt>(
                    GW::Scanner::ToFunctionStart(addr, 0x200));
            }
        }

        if (!TextResolveIssue_Func) {
            // TextResolveIssue — void(uint frame_id, wchar_t* text, uint context)
            //   Pushes assertion line 0x3B0 (text null check), unique in FrText.cpp.
            //   Pattern matches at +3 from function start (after PUSH EBP; MOV EBP,ESP).
            const uintptr_t addr = GW::Scanner::Find(
                "\x83\x7D\x0C\x00\x75\x16\x68\xB0\x03\x00\x00",
                "xxxxxxxxxxx");
            if (addr) {
                TextResolveIssue_Func = reinterpret_cast<TextResolveIssue_pt>(addr - 3);
            }
        }

        return CreateEncodedText_Func && TextResolveIssue_Func;
    }

    // Sets a custom title on a frame via Path B text storage + per-frame invalidation.
    //
    // Delegates to UIManager::SetFrameTitleAndInvalidate which uses:
    //   1. Ui_CreateEncodedText(8, 7, title, 0) → encoded wchar_t payload
    //   2. Ui_SetFrameText(frame_id, payload) → stores text directly in frame
    //      struct memory (CNonclient at frame+0xCC). Works on cold containers
    //      where the CNonclient subobject was never initialized by FrameCreate.
    //   3. PerFrameInvalidate(frame_id, 0xFFFFFFFF) → triggers full paint.
    //
    // This bypasses the async TextResolveIssue → msg 0x3A chain which silently
    // fails for cold containers (CNonclient not set up during FrameCreate, so
    // msg 0x3A dispatch to the CNonclient proc is ignored).
    inline bool SendTitleMsg5E(uint32_t frame_id, const std::wstring& title)
    {
        if (title.empty() || !frame_id)
            return false;

        return UIManager::SetFrameTitleAndInvalidate(frame_id, title);
    }

    // Exposes CreateEncodedText to Python for diagnostic / manual dispatch.
    // Returns the encoded-text pointer (pass to ctypes for raw inspection).
    // Must be called from the game thread.
    inline uint32_t CreateEncodedText(int32_t style_id, int32_t layout_profile,
                                      const std::wstring& text, int32_t flags)
    {
        if (!ResolveFunctions())
            return 0;
        return CreateEncodedText_Func(style_id, layout_profile, text.c_str(), flags);
    }
}

