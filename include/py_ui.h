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

// CtlTextBtnProc @ EXE 0x00616c00 — engine TEXT BUTTON FrameProc. Self-renders its
// caption via FrameContentAddText, self-sizes (msg 0x38), handles hover/click, and
// has NO image-list/store dependency (unlike the styled UiCtlBtnProc). It is the
// button analogue of TextLabelFrame_Callback (CtlTextProc).
//
// Resolution: the jump-table prologue "ADD EAX,-4; CMP EAX,0x5C; JA"
// ("\x83\xC0\xFC\x83\xF8\x5C\x0F\x87") is unique; walk back to the function entry.
// Verified unique at match 0x00616c12 → entry 0x00616c00 in Gw.exe build 06-14-2026.
inline uintptr_t ResolveCtlTextButtonProc()
{
    static uintptr_t cached = 0;
    if (cached)
        return cached;

    const uintptr_t hit = GW::Scanner::Find(
        "\x83\xC0\xFC\x83\xF8\x5C\x0F\x87",
        "xxxxxxxx", 0);
    if (!hit) {
        GWCA_ERR("[SCAN] ResolveCtlTextButtonProc — pattern not found");
        return 0;
    }
    const uintptr_t func_start = GW::Scanner::ToFunctionStart(hit, 0x20);
    if (!func_start) {
        GWCA_ERR("[SCAN] ResolveCtlTextButtonProc — ToFunctionStart failed");
        return 0;
    }
    cached = func_start;
    return cached;
}

// CCtlTextSelectable::FrameProc @ EXE 0x00617df0 — the SELECTABLE text-row proc the engine's own
// selectable list constructor (FUN_00619b70) uses for clickable rows. UNLIKE CtlTextBtnProc
// (0x00616c00), its highlight/getter path is NULL-safe: when the selectable list highlights a row it
// sends child msg 0x57 with a NULL out-ptr; CtlTextBtnProc case 0x57 WRITES through that null
// (*param_2 = colour) → access-violation on hover/select, whereas this proc's case 0x57 only sets a
// bit (no wparam deref). Using it is the fix for the hyperlink hover crash.
// Anchor: standard security-cookie prologue (SUB ESP,0x68; load cookie) + the distinctive param load
// MOV EBX,[EBP+8]; PUSH ESI; MOV ESI,[EBP+0x10]. The 4 cookie-global bytes are masked so the pattern
// survives a rebased build; the remainder is unique in Gw.exe 06-14 (verified). Match IS the entry.
inline uintptr_t ResolveCtlTextSelectableProc()
{
    static uintptr_t cached = 0;
    if (cached)
        return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x83\xEC\x68\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xFC\x53\x8B\x5D\x08\x56\x8B\x75\x10",
        "xxxxxxx????xxxxxxxxxxxxx", 0);
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveCtlTextSelectableProc — pattern not found");
        return 0;
    }
    cached = addr;
    return cached;
}

// CUiCtlPage::FrameProc (styled tabbed-page proc) @ EXE 0x00885590 — the TEXTURED page proc the game
// uses for real tab widgets. GW::TabsFrame::Create installs only the BASE CtlPageProc (0x0061a950),
// whose msg-0x5e config table has zero button/body subclass slots, so tab buttons stay flat
// CtlBtnProc = the "generic element" (no GW tab texture). Layering this styled proc makes OnCtlAddItem
// subclass each tab button with FUN_00885340 (case 8 = FrameContentAddImageTemplate → real tab
// texture). Its case 4 re-installs the base proc as sub-handler so switching/body/AddTab still work.
// Anchor: prologue MOV EDX,[EBP+8]; SUB ESP,0x10; MOV ECX,[EBP+0xC]; MOV EAX,[EDX+4]; CMP EAX,4; JZ;
// CMP EAX,0x15 — addressless/stable, unique in Gw.exe 06-14. Match IS the entry.
inline uintptr_t ResolveUiCtlPageProc()
{
    static uintptr_t cached = 0;
    if (cached)
        return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x8B\x55\x08\x83\xEC\x10\x8B\x4D\x0C\x8B\x42\x04\x83\xF8\x04\x74\x56\x83\xF8\x15",
        "xxxxxxxxxxxxxxxxxxxxxxx", 0);
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveUiCtlPageProc — pattern not found");
        return 0;
    }
    cached = addr;
    return cached;
}

// UiCtlSliderProc (styled slider WRAPPER) @ EXE 0x0087f440 — the textured bar+thumb proc. Must be the
// PRIMARY create proc (its case 4 installs base CtlSliderProc 0x00615fe0 as sub-handler). Creating base-
// as-primary + FrameNewSubclass(wrapper) — GWCA's SliderFrame::Create path — is WRONG: the wrapper never
// gets msg 4, and FrameNewSubclass re-fires msg 9/0xb, freeing then re-alloc'ing the instance with garbage
// range read from the proc-pointer bytes → paint/measure on half-init state → crash on create (this was
// the "slider immediate crash"). Anchor: prologue MOV EAX,[EBX+4]; DEC EAX; CMP EAX,0x58 — addressless,
// unique in Gw.exe 06-14. Match IS the entry.
inline uintptr_t ResolveUiCtlSliderProc()
{
    static uintptr_t cached = 0;
    if (cached)
        return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x83\xEC\x18\x53\x8B\x5D\x08\x56\x57\x8B\x43\x04\x48\x83\xF8\x58",
        "xxxxxxxxxxxxxxxxxxx", 0);
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveUiCtlSliderProc — pattern not found");
        return 0;
    }
    cached = addr;
    return cached;
}

// Hover-target setter FUN_00630cd0(frame, subitem) — the SOLE writer of the hovered-frame global
// DAT_00c0ad54 (verified by xref). Call FUN_00630cd0(0,-1) to CLEAR the hover BEFORE destroying a
// window: otherwise a control that is the current hover target is freed while DAT_00c0ad54 still points
// at it, and the next mouse move dereferences freed memory — THE "crash on closing the window" use-
// after-free (foundational RE 2026-07-01). __cdecl(int,int). Prologue cmp [hover-enable],0 (addr masked)
// + MOV EAX,[EBP+8]; unique in Gw.exe 06-14.
inline uintptr_t ResolveSetHoverTarget()
{
    static uintptr_t cached = 0;
    if (cached) return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x83\xEC\x08\x83\x3D\x00\x00\x00\x00\x00\x74\x68\x8B\x45\x08\x56\x85\xC0\x74\x05\x8B\x75\x0C",
        "xxxxxxxx????xxxxxxxxxxxxxx", 0);
    if (!addr) { GWCA_ERR("[SCAN] ResolveSetHoverTarget — pattern not found"); return 0; }
    cached = addr; return cached;
}

// Focus/tooltip-frame clear FUN_0064e920(frame). Call FUN_0064e920(0) to drop the keyboard-focus /
// tooltip frame global DAT_00c0ba10 before destroying a window (edits hold focus). __cdecl(int); unique.
inline uintptr_t ResolveClearFocusFrame()
{
    static uintptr_t cached = 0;
    if (cached) return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x57\x8B\x7D\x08\x3B\x3D\x00\x00\x00\x00\x0F\x84\xA0\x00\x00\x00\x56",
        "xxxxxxxxx????xxxxxxx", 0);
    if (!addr) { GWCA_ERR("[SCAN] ResolveClearFocusFrame — pattern not found"); return 0; }
    cached = addr; return cached;
}

// Plain content-container proc FUN_0051d8e0 (GmChat's control host). Pure pass-through: on child-notify
// (msg 0x31) forwards notify 7 to its parent, ignores everything else — no instance, no paint, no
// teardown to go wrong. This is the "owned content frame" the game hosts interactive controls under
// (NEVER on the CtlDlg chrome root). Anchor: prologue MOV EAX,[EBP+8]; CMP [EAX+4],0x31; JNE; push
// 0/param/7/frame — addressless up to the trailing relative call; unique in Gw.exe 06-14. Match IS entry.
inline uintptr_t ResolvePlainContainerProc()
{
    static uintptr_t cached = 0;
    if (cached) return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x8B\x45\x08\x83\x78\x04\x31\x75\x11\x6A\x00\xFF\x75\x0C\x6A\x07\xFF\x30",
        "xxxxxxxxxxxxxxxxxxxxx", 0);
    if (!addr) { GWCA_ERR("[SCAN] ResolvePlainContainerProc — pattern not found"); return 0; }
    cached = addr; return cached;
}

// CCtlFrameListSelectable::FrameProc @ EXE 0x00613850 — the SELECTABLE frame-list proc.
// When a child item notifies the list with notifyId 8 (a button-item click) it calls
// SetSelection → highlights the row (msg 0x57) AND stores the selection; the selection is
// readable via msg 0x67 (see ResolveCtlFrameListSelectableGetSelection). Register this as
// the inner-list proc (page-context field_4) to make list items act as clickable rows.
// Prologue jump-table on max msg 0x66; the match IS the entry. Verified unique 06-14-2026.
inline uintptr_t ResolveCtlFrameListSelectableProc()
{
    static uintptr_t cached = 0;
    if (cached)
        return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x56\x8B\x75\x08\x57\x8B\x7D\x0C\x8B\x46\x04\x83\xC0\xFC\x83\xF8\x66\x0F\x87",
        "xxxxxxxxxxxxxxxxxxxxxx", 0);
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveCtlFrameListSelectableProc — pattern not found");
        return 0;
    }
    cached = addr;
    return cached;
}

// CtlFrameListSelectableGetSelection @ EXE 0x00612b40 —
//   uint32_t __cdecl(uint32_t frameListId, uint32_t* out_item_code)
//   → nonzero if a row is selected; out_item_code = the selected item's child code (msg 0x67).
// The match IS the entry. Verified unique 06-14-2026.
inline uintptr_t ResolveCtlFrameListSelectableGetSelection()
{
    static uintptr_t cached = 0;
    if (cached)
        return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x83\xEC\x08\x56\x8B\x75\x0C\x85\xF6\x75\x14\x68",
        "xxxxxxxxxxxxxxx", 0);
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveCtlFrameListSelectableGetSelection — pattern not found");
        return 0;
    }
    cached = addr;
    return cached;
}

// CtlBtnProc @ EXE 0x0060f4f0 (WASM ram:80dbe9be) — the flat ENGINE button. Paints a
// solid rectangle (msg 0x01 -> GrBuildSolidMaterial), NON-hyperlink look, single
// self-contained proc (no multi-layer base delegation), and NO s_btnCheckImageList
// dependency (that global is UiCtlBtnProc-only). Its pushed/checked state is readable
// via msg 0x59 (UIManager::IsButtonPushedByFrameId). Give item_flags bit 0x10000 for a
// persistent toggle or 0x80000 for a momentary button.
//
// The FULL 25-byte prologue below is the function ENTRY (verified unique @ 0x0060f4f0 in
// Gw.exe 06-14) — so match offset 0, NO ToFunctionStart needed. (The existing UIMgr.cpp
// CtlBtnProc_Callback uses a shorter MID-function pattern without ToFunctionStart and is
// therefore unsafe to pass as an item proc; this resolver avoids that.)
inline uintptr_t ResolveCtlBtnProc()
{
    static uintptr_t cached = 0;
    if (cached)
        return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x83\xEC\x30\x53\x8B\x5D\x08\x56\x57\x8B\x7D\x0C\x8B\x43\x04\x8B\x53\x08\x48\x83\xF8\x5E",
        "xxxxxxxxxxxxxxxxxxxxxxxxx", 0);
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveCtlBtnProc — pattern not found");
        return 0;
    }
    cached = addr;
    return cached;
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// INSTANCED-DIALOG DISPATCHER (2026-07-01) — Py4GW-authored native FrameProc, the "owner" every native
// control-hosting window has and Py4GW never did (7-window comparison, docs/RE/native_dialog_layout_process.md).
// The engine calls a FrameProc as `void __cdecl(InteractionMessage* msg, void* w, void* l)` where
// msg[0]=frameId, msg[1]=msgId, msg[5]=proc-chain index. IUi_FrameMsgCallBase (EXE 0x00647170) walks the
// chain and invokes the next proc — we forward the SAME msg pointer to it. PHASE 1: forward-only (validate
// we can chain an authored C++ proc without crashing). Later phases add: own a CCtlLayout instance, run the
// size pass on msg 0x38/0x37, handle child notifies, and free on msg 0xb.
using FrameMsgCallBase_pt = void(__cdecl*)(uint32_t*, void*, void*);
inline FrameMsgCallBase_pt g_frameMsgCallBase = nullptr;

// IUi_FrameMsgCallBase @ EXE 0x00647170. Prologue is unique (push ebp; mov ebp,esp; sub esp,0x1c; push esi;
// mov esi,[ebp+8]; mov ecx,[esi]; test ecx,ecx; je +0x191). 20-byte match, offset 0 = entry.
inline uintptr_t ResolveFrameMsgCallBase()
{
    static uintptr_t cached = 0;
    if (cached) return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x83\xEC\x1C\x56\x8B\x75\x08\x8B\x0E\x85\xC9\x0F\x84\x91\x01\x00\x00",
        "xxxxxxxxxxxxxxxxxxxx", 0);
    if (!addr) { GWCA_ERR("[SCAN] ResolveFrameMsgCallBase — pattern not found"); return 0; }
    cached = addr;
    return cached;
}

// The authored dispatcher. PHASE 1: pass every message straight to the base chain (a no-op owner that
// proves the calling convention works). Must be __cdecl and file-scope so its address is a plain proc ptr.
inline void __cdecl Py4GW_InstancedDialogProc(uint32_t* msg, void* wparam, void* lparam)
{
    if (g_frameMsgCallBase)
        g_frameMsgCallBase(msg, wparam, lparam);
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

// ============================================================================
// CtlFrameList "no-stretch" — install native size + size-query handler callbacks so the
// list stops stretching item widths to the list width. (Ghidra-verified on Gw.exe 06-14.)
//
// Dispatcher FUN_00612c80. 06-14 slot/message layout (DIFFERS from the WASM build!):
//   msg 0x62 -> ctx+0x04 size handler        (OnFrameMsgSize,      case 0x37)
//   msg 0x64 -> ctx+0x10 size-query handler  (OnFrameMsgSizeQuery, case 0x38)
// Both invoked __cdecl as handler(TArray<uint>* items, msg*); wparam IS the fn ptr.
// (msg 0x63/0x65 are fn+context variants — NOT what we want.)
// ============================================================================

// GWCA GW::Array<uint32_t>: buffer @0x00, count(m_size) @0x08 (NOT 0x04), capacity @0x0C.
struct CtlFrameListItemArray { uint32_t* buffer; uint32_t unk_04; uint32_t count; uint32_t capacity; };
struct FrameMsgSize { float width; float height; };                  // list width; height = top-anchor start
struct FrameMsgSizeQuery { float width; float unk_04; Coord2f* out_size; }; // handler writes out_size->{x,y}

// FrameSetPosition(frameId, Coord2f const& pos, Coord2f const& size) @ EXE 0x0062F770 (anchor forced 6).
using FrameSetPositionPosSize_pt = void(__cdecl*)(uint32_t, const Coord2f*, const Coord2f*);
// FrameGetNativeSize @ EXE 0x0062D2A0. sret: returns 'out' in EAX.
using FrameGetNativeSize_pt = Coord2f*(__cdecl*)(Coord2f* out, uint32_t frame_id, const Coord2f* constraint);

inline FrameSetPositionPosSize_pt g_noStretch_FrameSetPositionPosSize = nullptr;
inline FrameGetNativeSize_pt      g_noStretch_FrameGetNativeSize       = nullptr;

// FrameSetPosition(pos,size) — prologue incl. PUSH 0x840 (assert line). Unique @ 0x0062F770.
inline uintptr_t ResolveFrameSetPositionPosSize()
{
    static uintptr_t cached = 0;
    if (cached) return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x8B\x45\x08\x85\xC0\x75\x16\x68\x40\x08\x00\x00", "xxxxxxxxxxxxxxx", 0);
    if (!addr) { GWCA_ERR("[SCAN] ResolveFrameSetPositionPosSize — pattern not found"); return 0; }
    cached = addr; return cached;
}

// FrameGetNativeSize — prologue incl. PUSH 0x7FD (assert line). Unique @ 0x0062D2A0.
inline uintptr_t ResolveFrameGetNativeSize()
{
    static uintptr_t cached = 0;
    if (cached) return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x8B\x45\x0C\x85\xC0\x75\x20\x68\xFD\x07\x00\x00", "xxxxxxxxxxxxxxx", 0);
    if (!addr) { GWCA_ERR("[SCAN] ResolveFrameGetNativeSize — pattern not found"); return 0; }
    cached = addr; return cached;
}

// Size handler (msg 0x62). Reproduces the engine's OnFrameMsgSize layout (top-anchored,
// running-Y decreasing from list height, per-item native height) — ONLY change: size.x =
// item's OWN native width instead of the list width.
inline void __cdecl NoStretchSizeHandler(void* arr_ptr, void* msg_ptr)
{
    if (!g_noStretch_FrameGetNativeSize || !g_noStretch_FrameSetPositionPosSize) return;
    const CtlFrameListItemArray* arr = reinterpret_cast<const CtlFrameListItemArray*>(arr_ptr);
    const FrameMsgSize* msg = reinterpret_cast<const FrameMsgSize*>(msg_ptr);
    const uint32_t* items = arr->buffer; const uint32_t count = arr->count;
    if (!items) return;
    const Coord2f constraint = { msg->width, 0.0f };
    float running_y = msg->height;
    for (uint32_t i = 0; i < count; ++i) {
        Coord2f native = { 0.0f, 0.0f };
        g_noStretch_FrameGetNativeSize(&native, items[i], &constraint);
        running_y -= native.y;
        const Coord2f pos = { 0.0f, running_y };
        const Coord2f size = { native.x, native.y };
        g_noStretch_FrameSetPositionPosSize(items[i], &pos, &size);
    }
}

// Size-query handler (msg 0x64). Reports {max native width, sum native height}, omitting the
// engine's "if !style 0x4000: out.x = list width" override.
inline void __cdecl NoStretchSizeQueryHandler(void* arr_ptr, void* msg_ptr)
{
    if (!g_noStretch_FrameGetNativeSize) return;
    const CtlFrameListItemArray* arr = reinterpret_cast<const CtlFrameListItemArray*>(arr_ptr);
    FrameMsgSizeQuery* msg = reinterpret_cast<FrameMsgSizeQuery*>(msg_ptr);
    Coord2f* out = msg->out_size;
    if (!out) return;
    out->x = 0.0f; out->y = 0.0f;
    const uint32_t* items = arr->buffer; const uint32_t count = arr->count;
    const Coord2f constraint = { msg->width, 0.0f };
    for (uint32_t i = 0; i < count && items; ++i) {
        Coord2f native = { 0.0f, 0.0f };
        g_noStretch_FrameGetNativeSize(&native, items[i], &constraint);
        if (native.x > out->x) out->x = native.x;
        out->y += native.y;
    }
}


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
    // IUi::UiCtlDlgMsgProc (EXE 0x00876610, WASM 0x80e586ff) — the DIALOG BASE proc used at CREATE time
    // by IUi::CompositeDlgBuilder case 1. Its msg 4 installs the dialog frame vtable (table idx 0xae9);
    // Py4GW's normal window instead uses ResolveContainerFrameProc and never installs that vtable, so its
    // frame lacks the dialog context every higher layer (content page / controls) depends on. Resolved a
    // fixed 0x270 below the chrome proc IUi::UiCtlDlgProc (0x00876880) in the Gw.exe 06-14 build.
    static uintptr_t ResolveUiCtlDlgMsgProc() {
        static uintptr_t cached = 0;
        if (cached) return cached;
        const uintptr_t dlg = ResolveCompositeRootControlProc();  // IUi::UiCtlDlgProc 0x00876880
        if (!dlg) { GWCA_ERR("[UI] ResolveUiCtlDlgMsgProc — composite root proc unresolved"); return 0; }
        cached = dlg - 0x270;                                      // IUi::UiCtlDlgMsgProc 0x00876610
        return cached;
    }

    static uint32_t _create_container_window(
        float x, float y, float width, float height,
        const std::wstring& frame_label = L"",
        uint32_t parent_frame_id = 9,
        uint32_t child_index = 0,
        uint32_t frame_flags = 0,
        uintptr_t create_param = 0,
        uint32_t anchor_flags = 0x6,
        uintptr_t proc_override = 0)   // 0 = default container proc; nonzero = use this create-time proc
    {
        const uint32_t callback = proc_override
            ? static_cast<uint32_t>(proc_override)
            : ResolveContainerFrameProc();
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

    // Reserved child-id band for a composite window's CONTENT frame(s). (WASM-first RE, symbols mapped to
    // Gw.exe 06-14, 2026-07-01) A native composite dialog is a 3-level tree: WINDOW (chrome IUi::UiCtlDlgProc,
    // EXE 0x00876880) → CONTENT frame at a childId in 0x2710..0x2718 (10000..10008) → CONTROLS nested inside
    // it. The native title-bar [X] runs IUi::PopCloser (EXE 0x008766e0), which walks the window's band
    // children via FrameGetChild from 0x2718 down to 0x2710 and FrameDestroy()s each — recursively freeing
    // the whole control subtree in ONE native pass. Controls parented directly on the chrome frame (childIds
    // 1,2,3…, OUTSIDE the band) are NOT on PopCloser's path, so on [X] they are left dangling in the hover
    // hot-item (DAT_00c0ad54) and the next chrome paint dereferences freed backing → the "crash on closing
    // the window". Nesting controls under a band content frame is the NATIVE fix (no scrub/bypass needed).
    static constexpr uint32_t kContentFrameChildId  = 0x2710;   // 10000 — the sole content host we create
    static constexpr uint32_t kContentFrameBandEnd  = 0x2718;   // 10008 — end of the reserved band

    // Resolve (creating on first use) the window's native CONTENT frame — the band member that
    // interactive controls must parent to so the native [X] frees them with the content subtree.
    // Scans the reserved band on the window; if no member exists yet it creates the sole content host
    // SYNCHRONOUSLY (every control-create helper already runs on the game thread, so this is safe and
    // race-free — unlike creating it in a deferred enqueue, which let the first control be made before the
    // host existed and fall back to the crash-prone direct-child path). Returns window_id only if the
    // window is invalid or creation fails.
    static uint32_t EnsureContentFrameForWindow(uint32_t window_id) {
        // DISABLED (2026-07-01): nesting controls under a plain pass-through container (FUN_0051d8e0) at
        // band 0x2710 — even created synchronously — hides/breaks the controls (confirms the handover's
        // "plain container regressed everything"). A plain container is NOT a viable content host: it does
        // not run the native content-frame paint/size/show cascade (msg 8/9/0x2e) that IUi::UiCtlDlgProc's
        // real content frame (proc FUN_00876740, style 0x80) does, so children never lay out or render.
        // Returning window_id restores the working direct-child behavior (controls render + interact; the
        // native [X] still crashes — a SEPARATE problem). Re-enable only with the real content proc, and
        // only with in-client verification. Band scan/create scaffolding retained below for that work.
        return window_id;
#if 0
        GW::UI::Frame* win = GW::UI::GetFrameById(window_id);
        if (!(win && win->IsCreated()))
            return window_id;
        for (uint32_t id = kContentFrameChildId; id <= kContentFrameBandEnd; ++id) {
            GW::UI::Frame* c = GW::UI::GetChildFrame(win, id);
            if (c && c->IsCreated())
                return c->frame_id;
        }
        const uint32_t content = CreateContentPanelByFrameId(window_id, 0.0f, 0.0f, kContentFrameChildId);
        return content ? content : window_id;
#endif
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
        // The native CONTENT frame (band member 0x2710) is created lazily+synchronously by
        // EnsureContentFrameForWindow the first time a control is added, so there is no create-order race.
        return frame_id;
    }

    // ─────────────────────────────────────────────────────────────────────────────────────────────
    // ISOLATED (2026-07-01): a window that mimics IUi::CompositeDlgBuilder case 1 EXACTLY — created with
    // the DIALOG BASE proc IUi::UiCtlDlgMsgProc (so msg 4 installs the dialog frame vtable idx 0xae9),
    // style 0x254000, childId 0xd, then the interactive chrome IUi::UiCtlDlgProc chained on top (via
    // AttachCompositeRootToFrame). This is the create-time dialog context Py4GW's normal window omits.
    // Separate from CreateNativeWindow so testing it cannot regress the working controls. Returns the
    // window frame id, or 0.
    static uint32_t CreateNativeDialogWindow(
        float content_x, float content_y, float content_width, float content_height,
        const std::wstring& title = L"")
    {
        constexpr float kLeftBorder = 32.0f, kTopTitle = 20.0f, kRightBorder = 32.0f, kBottomBorder = 32.0f;
        constexpr uint32_t kAnchorFlags = 0x6;
        constexpr uint32_t kSubclassFlags = DEFAULT_SUBCLASS_FLAGS_COMPOSITE_ROOT;
        constexpr uint32_t kParentFrameId = 9;
        constexpr uint32_t kDialogChildId = 0xd;        // reference window childId
        constexpr uint32_t kDialogStyle   = 0x254000;   // reference window style
        static int next_layer = 1000;                   // separate layer band from CreateNativeWindow

        const uintptr_t base_proc = ResolveUiCtlDlgMsgProc();
        if (!base_proc) return 0;

        GW::UI::Frame* root = GW::UI::GetRootFrame();
        if (!root) { GWCA_ERR("[UI] CreateNativeDialogWindow — root unavailable"); return 0; }

        const auto viewport_scale = root->position.GetViewportScale(root);
        const float scale_x = viewport_scale.x != 0.0f ? viewport_scale.x : 1.0f;
        const float scale_y = viewport_scale.y != 0.0f ? viewport_scale.y : 1.0f;
        const float pixel_height = root->position.viewport_height * scale_y;

        const float engine_x = (content_x - kLeftBorder) / scale_x;
        const float engine_y = (pixel_height - content_y - content_height - kBottomBorder) / scale_y;
        const float engine_w = (content_width + kLeftBorder + kRightBorder) / scale_x;
        const float engine_h = (content_height + kTopTitle + kBottomBorder) / scale_y;
        const int layer = next_layer++;

        const uint32_t frame_id = _create_container_window(
            engine_x, engine_y, engine_w, engine_h, title,
            kParentFrameId, kDialogChildId, kDialogStyle, 0, kAnchorFlags, base_proc);
        if (!frame_id) { GWCA_ERR("[UI] CreateNativeDialogWindow — create failed"); return 0; }

        if (!AttachCompositeRootToFrame(frame_id, title, kSubclassFlags, engine_x, engine_y, layer)) {
            GWCA_ERR("[UI] CreateNativeDialogWindow — chrome attach failed, frame_id=%u", frame_id);
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
    // (Ghidra-verified, "/Gw.exe (06-14)") GWCA's GW::UI::DestroyUIComponent NO-OPS on this build:
    // its DestroyUIComponent_Func never resolves because GWCA's resolver scans for the file path
    // "\\Code\\Gw\\Ui\\Frame\\FrApi.cpp", but this build renamed it to "\\Code\\Engine\\Frame\\FrApi.cpp"
    // → FindAssertion returns 0 → the func pointer stays NULL → the wrapper's `&& DestroyUIComponent_Func`
    // guard makes it silently return false and the frame persists. That is the reported "Destroy All
    // never works" (silent no-op, no crash). Call the PUBLIC id-based native destroyer FUN_0062c550
    // (__cdecl(uint32_t frame_id) BY VALUE) directly: it validates the id, tears down the child tree,
    // and frees; it silently no-ops if the id isn't live.
    static bool DestroyUIComponentByFrameId(uint32_t frame_id) {
        using DestroyById_pt = void(__cdecl*)(uint32_t);
        static DestroyById_pt destroy_by_id = nullptr;
        if (!destroy_by_id) {
            // Prologue anchor: PUSH EBP; MOV EBP,ESP; PUSH ECX; PUSH ESI; MOV ESI,[EBP+8];
            // TEST ESI,ESI; JNZ +0x19; PUSH 0x413 (the id==0 → assert-line-0x413 path). All bytes are
            // addressless/stable; unique in Gw.exe 06-14. The match IS the function entry.
            const uintptr_t addr = GW::Scanner::Find(
                "\x55\x8B\xEC\x51\x56\x8B\x75\x08\x85\xF6\x75\x19\x68\x13\x04\x00\x00",
                "xxxxxxxxxxxxxxxxx", 0);
            if (!addr) {
                GWCA_ERR("[UI] DestroyUIComponentByFrameId — native destroyer (FUN_0062c550) not resolved");
                return false;
            }
            destroy_by_id = reinterpret_cast<DestroyById_pt>(addr);
        }
        if (frame_id == 0)   // FUN_0062c550 asserts (NO-RETURN) on id==0
            return false;
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);  // ensure the id is live (avoids the 0x15f assert)
        if (!frame)
            return false;
        destroy_by_id(frame_id);
        return true;
    }

    // Scrub the input-target globals so no freed frame can be left as the current hover/focus target.
    // (Foundational RE 2026-07-01) The recursive frame free (FUN_0062ab40) does NOT clear the hover
    // global DAT_00c0ad54 or the focus/tooltip global DAT_00c0ba10. If a control is the hover/focus
    // target when its window is destroyed, those globals dangle at freed memory and the next mouse move
    // / paint dereferences them → the "crash on closing the window" use-after-free. Call this BEFORE
    // destroying any window that hosts interactive controls (and it's harmless to call anytime).
    static void ClearUiInputTargets() {
        using SetHover_pt = void(__cdecl*)(int, int);
        using ClearFocus_pt = void(__cdecl*)(int);
        static SetHover_pt set_hover = nullptr;
        static ClearFocus_pt clear_focus = nullptr;
        if (!set_hover) { const uintptr_t a = ResolveSetHoverTarget();  if (a) set_hover = reinterpret_cast<SetHover_pt>(a); }
        if (!clear_focus) { const uintptr_t a = ResolveClearFocusFrame(); if (a) clear_focus = reinterpret_cast<ClearFocus_pt>(a); }
        if (set_hover)  set_hover(0, -1);   // DAT_00c0ad54 = 0, DAT_00c0ad58 = -1 (no hover)
        if (clear_focus) clear_focus(0);    // DAT_00c0ba10 = 0 (no focus/tooltip)
    }

    // Destroy a window (and its whole owned child subtree) SAFELY: scrub the hover/focus globals first,
    // then run the single native id-based teardown. Use this instead of a bare
    // destroy_ui_component_by_frame_id for any window that hosts interactive controls.
    static bool DestroyWindowSafelyByFrameId(uint32_t window_id) {
        ClearUiInputTargets();
        return DestroyUIComponentByFrameId(window_id);
    }

    // True if a frame with this id currently exists in the registry (created + live). Use to GUARD any
    // per-frame poll/get/set: if the hosting window was closed (e.g. via the native [X]), the control
    // frame is freed and dispatching a message to its stale id reads freed memory and crashes. Callers
    // should stop polling a control once its window fails this check. GW::UI::GetFrameById is the
    // standard validated registry lookup (returns null for freed/unknown ids).
    static bool FrameExistsByFrameId(uint32_t frame_id) {
        GW::UI::Frame* f = GW::UI::GetFrameById(frame_id);
        return f && f->IsCreated();
    }

    // Create an owned CONTENT PANEL (plain pass-through container, proc FUN_0051d8e0) as a child of a
    // window. (Foundational RE 2026-07-01) The game NEVER parents interactive controls to the CtlDlg
    // chrome root — they live under an owned content frame like this. Parent your controls to the
    // RETURNED panel id (not the window). The panel has no visual and nothing to free, so it tears down
    // cleanly with the window. Returns the panel frame id, or 0.
    static uint32_t CreateContentPanelByFrameId(
        uint32_t window_id, float width = 0.0f, float height = 0.0f, uint32_t child_index = 0)
    {
        const uintptr_t proc = ResolvePlainContainerProc();
        if (!proc) {
            GWCA_ERR("[UI] CreateContentPanelByFrameId — plain container proc not resolved");
            return 0;
        }
        const uint32_t id = CreateUIComponentByFrameId(
            window_id, 0, child_index, proc, std::wstring(), std::wstring());
        if (!id)
            return 0;
        if (width > 0.0f && height > 0.0f)
            FrameSetSizeByFrameId(id, width, height);
        return id;
    }

    // ─────────────────────────────────────────────────────────────────────────────────────────────
    // NATIVE-LAYOUT PATH (additive / isolated — 2026-07-01). Separate from every existing control helper
    // so iterating on it CANNOT regress the working controls (button/progress/group). Full RE spec:
    // docs/RE/native_dialog_layout_process.md. The native dialog nests controls under a real CONTENT-PAGE
    // frame (proc IUi::UiCtlContentPageProc @ EXE 0x00876740) in the reserved band 0x2710, made the
    // interactive top layer via FrameMouseEnable(page,1,0). Parenting controls to THIS page (not the
    // window chrome) is what makes the native [X]/IUi::PopCloser tear them down cleanly (no dangling
    // hot-item) and puts them on the correct paint/hit layer. Prior attempts used the plain container
    // (FUN_0051d8e0) — the wrong proc — and crashed; this uses the real content-page proc.

    // Resolve IUi::UiCtlContentPageProc (EXE 0x00876740). It sits a fixed 0x140 below the composite-root
    // chrome proc IUi::UiCtlDlgProc (0x00876880) in the Gw.exe 06-14 build; derive it from that
    // pattern-resolved address rather than hardcoding. (Refine to its own byte pattern if the build shifts.)
    static uintptr_t ResolveUiCtlContentPageProc() {
        static uintptr_t cached = 0;
        if (cached) return cached;
        const uintptr_t dlg = ResolveCompositeRootControlProc();  // IUi::UiCtlDlgProc 0x00876880
        if (!dlg) { GWCA_ERR("[UI] ResolveUiCtlContentPageProc — composite root proc unresolved"); return 0; }
        cached = dlg - 0x140;                                     // IUi::UiCtlContentPageProc 0x00876740
        return cached;
    }

    // Create (or return the existing) native CONTENT PAGE for a CreateNativeWindow window: a band-0x2710
    // child using the real content-page proc, made interactive with FrameMouseEnable(page,1,0). Parent
    // controls to the RETURNED page id. Returns 0 on failure (caller can fall back to the window id).
    static uint32_t CreateContentPageByFrameId(uint32_t window_id) {
        GW::UI::Frame* win = GW::UI::GetFrameById(window_id);
        if (!(win && win->IsCreated())) {
            GWCA_ERR("[UI] CreateContentPageByFrameId — window %u not created", window_id);
            return 0;
        }
        for (uint32_t id = 0x2710; id <= 0x2718; ++id) {
            GW::UI::Frame* c = GW::UI::GetChildFrame(win, id);
            if (c && c->IsCreated()) return c->frame_id;   // reuse existing page
        }
        const uintptr_t proc = ResolveUiCtlContentPageProc();
        if (!proc) return 0;
        // userdata = the window frame pointer (native content page routes its value/invalidate to the window).
        const uintptr_t userdata = reinterpret_cast<uintptr_t>(win);
        const uint32_t page = CreateUIComponentRawByFrameId(window_id, 0x80, 0x2710, proc, userdata);
        if (!page) {
            GWCA_ERR("[UI] CreateContentPageByFrameId — content page create failed");
            return 0;
        }
        // Make the new page the interactive top layer (native msg-9 does FrameMouseEnable(page,1,0)).
        using FrameMouseEnable_pt = void(__cdecl*)(uint32_t, uint32_t, uint32_t);
        static FrameMouseEnable_pt me_fn = nullptr;
        if (!me_fn) {
            const auto a = ResolveFrameMouseEnable();
            if (a) me_fn = reinterpret_cast<FrameMouseEnable_pt>(a);
        }
        if (me_fn) me_fn(page, 1, 0);
        return page;
    }

    // PHASE 2 (teardown fix): create an OWNED BAND-MEMBER content frame — a plain-container child at the
    // reserved band childId 0x2710 (so the native [X] closer IUi::PopCloser tears it down and re-arms the
    // hover hot-item, avoiding the dangling-pointer crash) with our authored dispatcher chained on as the
    // owner. Parent controls to the RETURNED frame id. Reuses an existing band member if present.
    // Returns 0 on failure (caller can fall back to the window id).
    static uint32_t CreateOwnedBandFrameByFrameId(uint32_t window_id, float width = 0.0f, float height = 0.0f) {
        GW::UI::Frame* win = GW::UI::GetFrameById(window_id);
        if (!(win && win->IsCreated())) {
            GWCA_ERR("[UI] CreateOwnedBandFrameByFrameId — window %u not created", window_id);
            return 0;
        }
        for (uint32_t id = 0x2710; id <= 0x2718; ++id) {
            GW::UI::Frame* c = GW::UI::GetChildFrame(win, id);
            if (c && c->IsCreated()) return c->frame_id;
        }
        const uint32_t content = CreateContentPanelByFrameId(window_id, width, height, 0x2710);
        if (!content) {
            GWCA_ERR("[UI] CreateOwnedBandFrameByFrameId — band frame create failed");
            return 0;
        }
        AttachInstancedDispatcherByFrameId(content);  // chain the owner dispatcher
        return content;
    }

    // PHASE 1 test: chain the Py4GW-authored instanced-dialog dispatcher (Py4GW_InstancedDialogProc) onto a
    // frame via FrameNewSubclass. Validates that the engine can call an authored C++ FrameProc without
    // crashing (the foundational capability for the native control host). Returns true on success.
    static bool AttachInstancedDispatcherByFrameId(uint32_t frame_id) {
        using FrameNewSubclass_pt = void*(__cdecl*)(uint32_t, void*, void*);
        static FrameNewSubclass_pt subclass_fn = nullptr;
        if (!subclass_fn) {
            const auto a = ResolveFrameNewSubclass();
            if (a) subclass_fn = reinterpret_cast<FrameNewSubclass_pt>(a);
        }
        if (!g_frameMsgCallBase) {
            const auto a = ResolveFrameMsgCallBase();
            if (a) g_frameMsgCallBase = reinterpret_cast<FrameMsgCallBase_pt>(a);
        }
        if (!subclass_fn || !g_frameMsgCallBase) {
            GWCA_ERR("[UI] AttachInstancedDispatcherByFrameId — resolvers failed (subclass=%p base=%p)",
                (void*)subclass_fn, (void*)g_frameMsgCallBase);
            return false;
        }
        GW::UI::Frame* f = GW::UI::GetFrameById(frame_id);
        if (!(f && f->IsCreated())) {
            GWCA_ERR("[UI] AttachInstancedDispatcherByFrameId — frame %u not created", frame_id);
            return false;
        }
        subclass_fn(frame_id, reinterpret_cast<void*>(&Py4GW_InstancedDialogProc), reinterpret_cast<void*>(0));
        return true;
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

    // Creates a native CtlBtn button using the engine-level CtlBtnProc.
    // Unlike CreateButtonFrameByFrameId (which uses IUi::UiCtlBtnProc and crashes),
    // CtlBtnProc has ZERO external dependencies — no s_btnCheckImageList, no image
    // templates. Paints flat-color rectangles. Text set after creation via msg 0x5E.
    // This is the same path IME candidate window uses (prev/next page buttons).
    static uint32_t CreateCtlButtonFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index = 0,
        const std::wstring& caption = L"",
        const std::wstring& component_label = L"")
    {
        printf("[GWCA-DEBUG] CreateCtlButtonFrameByFrameId: ENTRY parent=%u flags=0x%X child=%u caption='%ls' label='%ls'\n",
            parent_frame_id, component_flags, child_index,
            caption.c_str(), component_label.c_str());
        fflush(stdout);

        auto* frame = GW::UI::CreateCtlButtonFrame(
            parent_frame_id,
            component_flags,
            child_index,
            caption.empty() ? nullptr : const_cast<wchar_t*>(caption.c_str()),
            component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str()));

        uint32_t result = frame ? frame->frame_id : 0;
        printf("[GWCA-DEBUG] CreateCtlButtonFrameByFrameId: RESULT frame_id=%u\n", result);
        fflush(stdout);
        return result;
    }

    // ── Path B: Flat Engine Button with Click Support ──────────────────
    // Creates a flat button (CtlBtnProc) with proper dimensions and optional
    // click support via FrameNewSubclass on the parent window.
    //
    // Full pipeline:
    //   1. [Optional] FrameNewSubclass(parent, dialogSubclassType, 0) — OnFrameNotify
    //   2. FrameCreate(parent, flags, childIdx, CtlBtnProc, userData, label)
    //   3. FrameSetSize(buttonId, width, height)          ← fixes thin strip
    //   4. CtlBtnSetTextLiteral(buttonId, caption)        ← sets text
    //   5. FrameSetPosition(buttonId, x, y)
    //   6. FrameMouseEnable(buttonId, 1, 0)
    //
    // Click support requires DIALOG_SUBCLASS_TYPE_ADDR (currently 0 = disabled).
    // When resolved, click notifications (notifyId 7=pushed, 8=clicked) will
    // dispatch through the parent's OnFrameNotify handler.
    static uint32_t CreateFlatButtonWithClickByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags = 0x40000,
        uint32_t child_index = 0,
        const std::wstring& label_text = L"",
        float width = 100.0f,
        float height = 24.0f,
        float pos_x = 10.0f,
        float pos_y = 10.0f,
        bool enable_click = false)
    {
        printf("[GWCA-DEBUG] CreateFlatButtonWithClickByFrameId: ENTRY parent=%u flags=0x%X child=%u label='%ls' size=(%.1f,%.1f) pos=(%.1f,%.1f) enable_click=%d\n",
            parent_frame_id, component_flags, child_index,
            label_text.c_str(), width, height, pos_x, pos_y, (int)enable_click);
        fflush(stdout);

        // Step 1: Create the button via GWCA's CreateFlatButtonWithClick
        //  (which handles FrameNewSubclass on parent if enable_click is set)
        auto* frame = GW::UI::CreateFlatButtonWithClick(
            parent_frame_id,
            component_flags,
            child_index,
            label_text.empty() ? nullptr : const_cast<wchar_t*>(label_text.c_str()),
            enable_click);
        if (!frame) {
            printf("[GWCA-DEBUG] CreateFlatButtonWithClickByFrameId: FAILED — CreateFlatButtonWithClick returned null\n");
            fflush(stdout);
            return 0;
        }

        const uint32_t button_id = frame->frame_id;
        printf("[GWCA-DEBUG] CreateFlatButtonWithClickByFrameId: button created, frame_id=%u\n", button_id);
        fflush(stdout);

        // Step 2: Fix thin strip — set dimensions via FrameSetSize
        if (width > 0 && height > 0) {
            printf("[GWCA-DEBUG] CreateFlatButtonWithClickByFrameId: FrameSetSize(frame=%u, w=%.1f, h=%.1f)\n",
                button_id, width, height);
            fflush(stdout);
            FrameSetSizeByFrameId(button_id, width, height);
        }

        // Step 3: Set caption text via CtlBtnSetTextLiteral (msg 0x5E)
        if (!label_text.empty()) {
            printf("[GWCA-DEBUG] CreateFlatButtonWithClickByFrameId: CtlBtnSetTextLiteral(frame=%u, text='%ls')\n",
                button_id, label_text.c_str());
            fflush(stdout);
            GW::UI::SendFrameUIMessage(frame, static_cast<GW::UI::UIMessage>(0x5E),
                const_cast<wchar_t*>(label_text.c_str()), nullptr);
        }

        // Step 4: Set position
        {
            using FrameSetPos_pt = void(__cdecl*)(uint32_t, Coord2f*);
            static FrameSetPos_pt pos_fn = nullptr;
            if (!pos_fn) {
                const auto addr = ResolveFrameSetPositionCoord2f();
                if (addr) pos_fn = reinterpret_cast<FrameSetPos_pt>(addr);
            }
            if (pos_fn) {
                printf("[GWCA-DEBUG] CreateFlatButtonWithClickByFrameId: FrameSetPosition(frame=%u, x=%.1f, y=%.1f)\n",
                    button_id, pos_x, pos_y);
                fflush(stdout);
                Coord2f coord = { pos_x, pos_y };
                pos_fn(button_id, &coord);
            } else {
                printf("[GWCA-DEBUG] CreateFlatButtonWithClickByFrameId: FrameSetPosition SKIPPED — fn not resolved\n");
                fflush(stdout);
            }
        }

        // Step 5: Enable mouse input
        {
            using FrameMouseEn_pt = void(__cdecl*)(uint32_t, uint32_t, uint32_t);
            static FrameMouseEn_pt me_fn = nullptr;
            if (!me_fn) {
                const auto addr = ResolveFrameMouseEnable();
                if (addr) me_fn = reinterpret_cast<FrameMouseEn_pt>(addr);
            }
            if (me_fn) {
                printf("[GWCA-DEBUG] CreateFlatButtonWithClickByFrameId: FrameMouseEnable(frame=%u, enable=1)\n", button_id);
                fflush(stdout);
                me_fn(button_id, 1, 0);
            } else {
                printf("[GWCA-DEBUG] CreateFlatButtonWithClickByFrameId: FrameMouseEnable SKIPPED — fn not resolved\n");
                fflush(stdout);
            }
        }

        printf("[GWCA-DEBUG] CreateFlatButtonWithClickByFrameId: COMPLETE frame_id=%u\n", button_id);
        fflush(stdout);
        return button_id;
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
        // NOTE (2026-07-01): reading the id from frame+sizeof(void*) (the swarm's "-4 offset" theory)
        // caused an IMMEDIATE checkbox crash in-client — reverted. The real checkbox model needs
        // foundational RE (how the game builds/tears down a real dialog checkbox); see the reset RE.
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
        printf("[GWCA-DEBUG] CreateScrollableFrameByFrameId: ENTRY parent=%u flags=0x%X child=%u page_context=0x%p label='%ls'\n",
            parent_frame_id, component_flags, child_index,
            (void*)page_context, component_label.c_str());
        fflush(stdout);

        auto* frame = GW::UI::CreateScrollableFrame(
            parent_frame_id,
            component_flags,
            child_index,
            reinterpret_cast<void*>(page_context),
            component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str()));

        uint32_t result = frame ? frame->frame_id : 0;
        printf("[GWCA-DEBUG] CreateScrollableFrameByFrameId: RESULT frame_id=%u\n", result);
        fflush(stdout);
        return result;
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
        // (Ghidra-verified, "/Gw.exe (06-14)") Create with the STYLED page proc UiCtlPageProc
        // (0x00885590) as the PRIMARY FrameProc — mirroring the game's own textured-tabs constructor
        // FUN_00889950: `FUN_0062bfc0(parent, 0x40000, 1, FUN_00885590, 0, 0)`. This is essential:
        // the engine sends msg 4 (install base/vtable) ONLY at CREATE time; the styled proc's case 4
        // installs base CtlPageProc (0x0061a950) as its sub-handler AND its case 0x5e returns the STYLED
        // config table so OnCtlAddItem subclasses each tab button with the textured proc (0x00885340).
        // A post-create FrameNewSubclass never receives msg 4 → the vtable stays wired for the plain
        // config → the styled 0x5e feeds mismatched state → crash on display (the 2026-07-01 regression).
        // Keep flags 0x40000 (paint gate). Container renders 0×0 until ≥1 tab is added — do NOT size it.
        const uintptr_t styled_proc = ResolveUiCtlPageProc();
        if (!styled_proc) {
            GWCA_ERR("[UI] CreateTabsFrameByFrameId — styled page proc (0x00885590) not resolved");
            return 0;
        }
        return CreateUIComponentByFrameId(
            parent_frame_id, component_flags, child_index,
            styled_proc, std::wstring(), component_label);
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

    // Show/hide a frame-list item by its CHILD CODE (= the insert_index it was created with). msg 0x67.
    // (Ghidra-verified, "/Gw.exe (06-14)") The engine has NO native group/collapse: a group header only
    // toggles its own checkbox glyph and never touches sibling rows. Collapsible sections are done
    // app-side — track which item codes belong to a header and call this for each when the header
    // toggles. Base CtlFrameList proc (FUN_00612c80) case 0x67 resolves the item via FrameGetChild
    // (so item_code is the CHILD CODE, not a frame id), sets its shown flag, and reflows the list —
    // but ONLY if the list style bit 0x2000 is CLEAR (else no reflow). Send to the LIST frame, NOT the
    // item. IMPORTANT: the EXE 06-14 ShowItem msg is 0x67 (the WASM 0x65 is a different fn on this
    // build). Payload = {item_code, show?1:0}. Returns false if the list frame is invalid.
    static bool CtlFrameListShowItemByFrameId(uint32_t frame_list_id, uint32_t item_code, bool show)
    {
        GW::UI::Frame* f = GW::UI::GetFrameById(frame_list_id);
        if (!f) {
            GWCA_ERR("[UI] CtlFrameListShowItemByFrameId — invalid frame_list_id %u", frame_list_id);
            return false;
        }
        uint32_t payload[2] = { item_code, show ? 1u : 0u };
        GW::UI::SendFrameUIMessage(f, static_cast<GW::UI::UIMessage>(0x67), payload, nullptr);
        return true;
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
        // Nest the list under the window's native CONTENT frame (band 0x2710) so IUi::PopCloser frees
        // the whole list (and its item rows) on the native [X].
        return CreateScrollableFrameByFrameId(EnsureContentFrameForWindow(window_id), component_flags,
                                             child_index, 0, component_label);
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

    // Convenience: add a clickable TEXT BUTTON item to a frame list.
    //
    // This is byte-for-byte the proven AddTextItemToFrameListByFrameId path — same
    // CtlFrameListCreateItem machinery (the frame list internally FrameCreates the
    // item with flags|0x300 in its managed parent context) and the same ENCODED
    // text payload — with ONE change: the item proc is CtlTextBtnProc instead of the
    // text-label proc. That is the only difference between a label and a button here.
    //
    // Rationale (RE 2026-06-30): the engine never creates button procs as bare
    // standalone children — they always live inside a managed parent context, and
    // their caption must be an encoded payload (raw text crashes). The frame-list
    // item path satisfies both, which is why it renders where bare FrameCreate does not.
    //
    // Returns the new button item's frame ID, or 0 on failure.
    static uint32_t AddButtonItemToFrameListByFrameId(
        uint32_t frame_list_id,
        const std::wstring& plain_text,
        uint32_t insert_index = 0,
        uint32_t item_flags = 0)
    {
        if (plain_text.empty()) {
            GWCA_ERR("[UI] AddButtonItemToFrameListByFrameId — empty text");
            return 0;
        }
        const uint32_t proc = static_cast<uint32_t>(ResolveCtlTextButtonProc());
        if (!proc) {
            GWCA_ERR("[UI] AddButtonItemToFrameListByFrameId — CtlTextBtnProc not resolved");
            return 0;
        }
        std::wstring encoded = BuildStandaloneLiteralEncodedTextPayload(plain_text);
        return CtlFrameListCreateItemByFrameId(frame_list_id, item_flags,
                                               insert_index, proc, encoded);
    }

    // Add a real FLAT BUTTON item (CtlBtnProc — solid rectangle, NON-hyperlink look) to a
    // frame list. Same proven CtlFrameListCreateItem path as add_text/button_item, with the
    // flat engine button proc. After creation the caption is set via msg 0x5E (literal), and
    // an explicit size is applied (CtlBtnProc has no min-size, so an unsized button lays out
    // thin). Pass item_flags 0x10000 for a persistent toggle (checked state), 0x80000 for a
    // momentary button; read the pushed/checked state each frame via IsButtonPushedByFrameId.
    //
    // Returns the new button item's frame ID, or 0 on failure.
    static uint32_t AddFlatButtonItemToFrameListByFrameId(
        uint32_t frame_list_id,
        const std::wstring& caption,
        uint32_t insert_index = 0,
        uint32_t item_flags = 0,
        float width = 120.0f,
        float height = 22.0f)
    {
        if (caption.empty()) {
            GWCA_ERR("[UI] AddFlatButtonItemToFrameListByFrameId — empty caption");
            return 0;
        }
        const uint32_t proc = static_cast<uint32_t>(ResolveCtlBtnProc());
        if (!proc) {
            GWCA_ERR("[UI] AddFlatButtonItemToFrameListByFrameId — CtlBtnProc not resolved");
            return 0;
        }
        std::wstring encoded = BuildStandaloneLiteralEncodedTextPayload(caption);
        const uint32_t item_id = CtlFrameListCreateItemByFrameId(frame_list_id, item_flags,
                                                                 insert_index, proc, encoded);
        if (!item_id)
            return 0;

        // Caption via msg 0x5E (CtlBtnSetTextLiteral takes a raw wchar_t* safely).
        GW::UI::Frame* f = GW::UI::GetFrameById(item_id);
        if (f) {
            GW::UI::SendFrameUIMessage(f, static_cast<GW::UI::UIMessage>(0x5E),
                const_cast<wchar_t*>(caption.c_str()), nullptr);
        }
        // Explicit size — CtlBtnProc has no min-size enforcement.
        if (width > 0.0f && height > 0.0f) {
            FrameSetSizeByFrameId(item_id, width, height);
        }
        return item_id;
    }

    // Install native no-stretch size + size-query handlers on a CtlFrameList so its items keep
    // their OWN width instead of being stretched to the list width. Call ONCE on the game thread
    // right after creating the scrollable content and BEFORE adding items. Returns frame_list_id, or 0.
    static uint32_t SetFrameListNoStretchByFrameId(uint32_t frame_list_id)
    {
        if (!g_noStretch_FrameSetPositionPosSize) {
            const uintptr_t a = ResolveFrameSetPositionPosSize();
            if (a) g_noStretch_FrameSetPositionPosSize = reinterpret_cast<FrameSetPositionPosSize_pt>(a);
        }
        if (!g_noStretch_FrameGetNativeSize) {
            const uintptr_t a = ResolveFrameGetNativeSize();
            if (a) g_noStretch_FrameGetNativeSize = reinterpret_cast<FrameGetNativeSize_pt>(a);
        }
        if (!g_noStretch_FrameSetPositionPosSize || !g_noStretch_FrameGetNativeSize) {
            GWCA_ERR("[UI] SetFrameListNoStretchByFrameId — geometry resolvers failed");
            return 0;
        }
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_list_id);
        if (!frame) {
            GWCA_ERR("[UI] SetFrameListNoStretchByFrameId — invalid frame_list_id %u", frame_list_id);
            return 0;
        }
        // Query handler first (0x64 -> ctx+0x10), then size handler (0x62 -> ctx+0x04). wparam IS the fn ptr.
        GW::UI::SendFrameUIMessage(frame, static_cast<GW::UI::UIMessage>(0x64),
            reinterpret_cast<void*>(&NoStretchSizeQueryHandler), nullptr);
        GW::UI::SendFrameUIMessage(frame, static_cast<GW::UI::UIMessage>(0x62),
            reinterpret_cast<void*>(&NoStretchSizeHandler), nullptr);
        return frame_list_id;
    }

    // ================= checkbox =================
// ── Real GW checkbox as a DIRECT CHILD of a managed window ──────────────────────────────
// (Ghidra-verified, Gw.exe 06-14) The working checkbox is IUi::UiCtlBtnProc (FUN_00877e60)
// created via CreateUIComponent with the checkbox style bit 0x8000 — i.e. GWCA's
// GW::UI::CreateCheckboxFrame (= CreateButtonFrame(parent, flags|0x8000, ...), whose
// ButtonFrame_Callback is already resolved at UIMgr.cpp:1421 via
// FindAssertion("UiCtlBtn.cpp","!s_btnCheckImageList") -> ToFunctionStart; NO new resolver
// needed). Creating it this way (NOT as a CtlFrameListCreateItem row) runs the proc's
// class-init cascade:
//   * msg 0x04 (case 4) wires the base = flat CtlBtnProc FUN_0060f4f0, which owns the
//     persistent checked bit0 and the auto-toggle-on-click -> create/destroy/input delegation
//     is crash-safe (unlike the single-proc frame-list path).
//   * the box/check face is painted in msg 0x01 pass 1 from the shared image-list global
//     s_btnCheckImageList (DAT_010819cc); the proc is its SOLE owner — it builds the list on its
//     class-init msg 0x05 (asserting !s_btnCheckImageList to forbid a double-build) and frees it
//     on 0x06. The list is NULL until a Store/merchant fires msg 0x05, so on a cold session we
//     trigger the proc's own build via EnsureBtnCheckImageList (sends msg 0x05, guarded on ==0).
// The box SELF-SIZES (base msg 0x38 = FUN_00610580 -> ~21x21 native), so it is NOT stretched
// full-width. Direct-child positioning is by a UNIQUE child_index (child_offset_id): the
// window's composite-root CRProc sorts/lays out children by this id, so give each checkbox a
// distinct child_index (1,2,3,...). Never share child_index=0 (that overlaps).
// Read state with IsCheckboxCheckedByFrameId (msg 0x58); set with SetCheckboxCheckedByFrameId
// (msg 0x57) — both already exist. Do NOT use IsButtonPushedByFrameId (msg 0x59 = pushed bit).
// parent_window_id MUST be a CreateNativeWindow() window (it carries the CRProc composite root).
// Returns the checkbox frame id, or 0.
static uint32_t CreateCheckboxChildByFrameId(
    uint32_t parent_window_id,
    const std::wstring& label,
    uint32_t child_index = 1,
    bool initial_checked = false)
{
    GW::UI::Frame* parent = GW::UI::GetFrameById(parent_window_id);
    if (!(parent && parent->IsCreated())) {
        GWCA_ERR("[UI] CreateCheckboxChildByFrameId — parent window %u not created", parent_window_id);
        return 0;
    }
    // Parent to the window's native CONTENT frame (band 0x2710), NOT the chrome window itself, so the
    // native [X] closer (IUi::PopCloser) frees this checkbox with the content subtree instead of leaving
    // it dangling in the hover hot-item. Falls back to the window id if creation fails.
    const uint32_t host_id = EnsureContentFrameForWindow(parent_window_id);
    // Encoded caption payload (raw wide strings crash the encoded-text path; reuse the same
    // literal encoder the other control helpers use).
    std::wstring name_enc = BuildStandaloneLiteralEncodedTextPayload(
        label.empty() ? std::wstring(L" ") : label);
    // (Ghidra-verified, "/Gw.exe (06-14)" + Gw.wasm, 2026-07-01) Match how GW builds its OWN
    // checkboxes (Options→General, IUi::NDlgOptionGroupGeneral::CDlgOptGeneral::OnFrameCreate @
    // WASM 80fcf105): every real checkbox — CheckShowKoreanRatings, CheckMouseDisableWalk,
    // CheckCompassLock, … — is created with:
    //     FrameCreate(parent, 0x10000, childId, UiCtlBtnProc, textId, name)
    // i.e. style word 0x10000 ONLY. 0x10000/0x20000 are LAYOUT auto-size-width/height flags
    // (confirmed in IFrame::Layout::CCmdAdd::Execute), NOT control-type styles.
    //
    // Do NOT set 0x40000. In ButtonFrame_Callback (FUN_00877e60) case 1, the raised BUTTON FACE
    // is drawn by sub-pass 0 (*param2==0) ONLY when FrameTestStyles(frame,0x40000)!=0. Setting
    // 0x40000 on a checkbox paints a button face — the "wrong glyph / cancel-button look". The
    // real CHECK GLYPH is drawn by sub-pass 1 (*param2==1), which is NOT gated by 0x40000; it
    // always pulls from the shared image list. So a correct checkbox needs NO face flag.
    //
    // CreateCheckboxFrame ORs 0x8000 (the explicit checkbox-glyph variant) → effective 0x18000.
    // Do NOT add 0x4000/0x800000 (0x804000 selects the RADIO glyph instead).
    GW::UI::Frame* frame = GW::UI::CreateCheckboxFrame(
        host_id,
        0x10000,
        child_index,
        const_cast<wchar_t*>(name_enc.c_str()),
        nullptr);
    if (!frame)
        return 0;
    // NOTE (2026-07-01): the frame+sizeof(void*) "-4 offset" id read caused an IMMEDIATE checkbox crash
    // in-client — reverted to frame->frame_id. The correct checkbox model is pending foundational RE.
    const uint32_t id = frame->frame_id;
    // Ensure the shared checkbox image list exists before the glyph paints. UiCtlBtnProc is its
    // SOLE owner (DAT_010819cc): it builds the list on its class-init msg 0x05 and paints the
    // glyph from it in msg 0x01 pass 1 (→ FUN_0062b0e0, which asserts 0xa28 on a NULL list). The
    // list stays NULL until a Store/merchant broadcasts msg 0x05, so on a cold session we trigger
    // the proc's OWN build by sending it msg 0x05 — but ONLY when the global is still null
    // (EnsureBtnCheckImageList guards on ==0). That respects the proc's `!s_btnCheckImageList`
    // single-build assert (case 5 asserts 0x45 if already built): we never send when it's set.
    EnsureBtnCheckImageList(id);
    // NOTE: the caption is already delivered ENCODED via name_enc at create (param 4) — exactly like
    // the native checkbox builder, which never sends msg 0x5E. Do NOT send a RAW caption to base
    // SetText (0x5E) afterwards: it routes a raw wide string into the encoded-text path and is
    // needless crash surface. (Removed the old post-create 0x5E send.)
    // Optional initial tick via the base checked bit (msg 0x57).
    if (initial_checked)
        SetCheckboxCheckedByFrameId(id, true);
    TriggerFrameRedrawByFrameId(id);
    return id;
}

    // ================= radio =================
    // Programmatically set the current selection of a SELECTABLE frame list to a specific
    // item (e.g. to apply a radio-group DEFAULT, or restore a saved choice). This is the
    // write side of GetFrameListSelectionByFrameId.
    //
    // (Ghidra-verified, "/Gw.exe (06-14)") The correct message is 0x6A, NOT 0x66.
    //   * Selectable proc CCtlFrameListSelectable::FrameProc @ 0x00613850 case 0x6A does:
    //       prop = FUN_00613b30(param_1);   FUN_00613b60(prop, /*wparam*/ code);
    //     FUN_00613b60 stores prop+8 = code (the selected item) and highlights the row via
    //     child msg 0x57. wparam is the item CHILD CODE passed BY VALUE — the same integer
    //     returned by AddTextItemToFrameListByFrameId and read back by
    //     GetFrameListSelectionByFrameId (msg 0x69, prop+8).
    //   * Msg 0x66 is a RED HERRING: the selectable proc has no case 0x66; it falls through
    //     to the BASE CtlFrameList proc (0x00612c80) case 0x66, which stores a selection-
    //     CHANGED CALLBACK pointer at base-prop+0x1C (FUN_00613bf0). It does NOT set the row.
    //
    // WARNING: FUN_00613b60 asserts NO-RETURN (FUN_00487a80(0x428)) when the code does not
    // resolve to a live child (FrameGetChild == 0). child_code MUST be a value previously
    // returned by AddTextItemToFrameListByFrameId on THIS list. A zero code is rejected here;
    // a stale/foreign nonzero code will crash, so only feed codes you created on this list.
    //
    // Returns frame_list_id on success, or 0 on failure.
    static uint32_t SetFrameListSelectionByFrameId(uint32_t frame_list_id, uint32_t child_code)
    {
        if (!frame_list_id || !child_code) {
            GWCA_ERR("[UI] SetFrameListSelectionByFrameId — invalid frame_list_id/child_code");
            return 0;
        }
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_list_id);
        if (!frame) {
            GWCA_ERR("[UI] SetFrameListSelectionByFrameId — frame %u not found", frame_list_id);
            return 0;
        }
        // msg 0x6A: wparam = child code BY VALUE (proc case 0x6A reads param_2 as the item
        // code and passes it straight to CtlFrameListSelectableSetSel / FUN_00613b60);
        // lparam unused.
        GW::UI::SendFrameUIMessage(frame, static_cast<GW::UI::UIMessage>(0x6A),
            reinterpret_cast<void*>(static_cast<uintptr_t>(child_code)), nullptr);
        return frame_list_id;
    }

    // ================= text_button_click =================
// ── CtlTextBtnProc (text button) — click-via-selectable-list helper + cosmetic setters ──
// (Ghidra-verified vs Gw.exe 06-14, FUN_00616c00 @ 0x00616c00; resolver already exists as
//  UIManager::ResolveCtlTextButtonProc — no NEW resolver required):
//   case 0x25 (mouse-up): FrameMsgNotifyParent(notifyId=8) UNCONDITIONALLY  → the SELECTABLE
//     list (0x00613850) turns this into SetSelection, readable via msg 0x67.
//   case 0x5b: *(instance+0x18) = wParam  → NORMAL text color (baked 0xff64beeb), ABGR
//   case 0x5d: *(instance+0x1c) = wParam  → HOVER  text color (baked 0xff78d2ff), ABGR
//   case 0x5f: rewrites caption buffer from a RAW wchar_t* wParam (create-time payload must be
//              ENCODED, but this SETTER takes a raw wide string)
//   Msg payload is {wParam, lParam}; the proc reads *param_2 == wParam (case 0x24 reads param_2[1]).
//   DO NOT poll msg 0x59 here (that is GetText, NOT IsPushed) and DO NOT parent to a PLAIN list.

    // Add a CLICKABLE text button whose click is READABLE. selectable_frame_list_id MUST come from
    // CreateSelectableScrollableContentByFrameId (selectable inner proc), NOT the plain scrollable
    // list — CtlTextBtnProc surfaces a click ONLY as parent-notify 8, and only the selectable list
    // consumes it (→ SetSelection). Poll the click with GetFrameListSelectionByFrameId(list): the
    // returned nonzero code == this row. Returns the item's frame id, or 0.
    static uint32_t AddClickableTextButtonToSelectableList(
        uint32_t selectable_frame_list_id,
        const std::wstring& caption,
        uint32_t insert_index = 0,
        uint32_t item_flags = 0)
    {
        if (!selectable_frame_list_id) {
            GWCA_ERR("[UI] AddClickableTextButtonToSelectableList — invalid selectable list id");
            return 0;
        }
        if (caption.empty()) {
            GWCA_ERR("[UI] AddClickableTextButtonToSelectableList — empty caption");
            return 0;
        }
        // (Ghidra-verified, "/Gw.exe (06-14)") Build the row with the SELECTABLE text proc
        // CtlTextSelectable (FUN_00617df0), NOT CtlTextBtnProc. When the selectable list highlights a
        // row on hover/select it sends child msg 0x57 with a NULL out-ptr; CtlTextBtnProc case 0x57
        // writes *param_2 = colour through that null → hover crash. CtlTextSelectable case 0x57 only
        // flips a bit (no wparam deref) and its case 0x24 still notifies parent id 8 (the click the
        // list turns into the pollable selection). Item flags 0xe001 mirror the native selectable-row
        // constructor FUN_00619b70. Click stays pollable via GetFrameListSelectionByFrameId (msg 0x69).
        const uintptr_t proc = ResolveCtlTextSelectableProc();
        if (!proc) {
            GWCA_ERR("[UI] AddClickableTextButtonToSelectableList — CtlTextSelectable proc not resolved");
            return 0;
        }
        std::wstring encoded = BuildStandaloneLiteralEncodedTextPayload(caption);
        // Use the caller's item_flags (default 0) so the list AUTO-STACKS the row exactly like a text
        // item. The native constructor's 0xe001 includes bit 0x2000 (manual-positioning / no
        // auto-layout), which in this frame-list-item path renders the row at zero size (invisible) and
        // corrupts teardown (crash on window close) — that was the 2026-07-01 regression. The proc swap
        // to CtlTextSelectable is what fixes the hover crash; the flags must stay list-managed here.
        return CtlFrameListCreateItemByFrameId(
            selectable_frame_list_id, item_flags, insert_index,
            static_cast<uint32_t>(proc), encoded);
    }

    // Override the baked cyan "hyperlink" NORMAL color. color_abgr is 0xAABBGGRR. msg 0x5b.
    static bool SetTextButtonColorByFrameId(uint32_t text_button_id, uint32_t color_abgr)
    {
        GW::UI::Frame* f = GW::UI::GetFrameById(text_button_id);
        if (!f) {
            GWCA_ERR("[UI] SetTextButtonColorByFrameId — invalid frame %u", text_button_id);
            return false;
        }
        GW::UI::SendFrameUIMessage(f, static_cast<GW::UI::UIMessage>(0x5B),
            reinterpret_cast<void*>(static_cast<uintptr_t>(color_abgr)), nullptr);
        return true;
    }

    // Override the baked HOVER color (shown while the mouse is over the button). ABGR. msg 0x5d.
    static bool SetTextButtonHoverColorByFrameId(uint32_t text_button_id, uint32_t color_abgr)
    {
        GW::UI::Frame* f = GW::UI::GetFrameById(text_button_id);
        if (!f) {
            GWCA_ERR("[UI] SetTextButtonHoverColorByFrameId — invalid frame %u", text_button_id);
            return false;
        }
        GW::UI::SendFrameUIMessage(f, static_cast<GW::UI::UIMessage>(0x5D),
            reinterpret_cast<void*>(static_cast<uintptr_t>(color_abgr)), nullptr);
        return true;
    }

    // Replace the caption. msg 0x5f takes a RAW wide string (it wcslen+copies it) — this is the
    // text-button caption setter. Do NOT use 0x5e here: on this proc 0x5e propagates the frame
    // NAME to children, it is not the caption setter.
    static bool SetTextButtonTextByFrameId(uint32_t text_button_id, const std::wstring& caption)
    {
        GW::UI::Frame* f = GW::UI::GetFrameById(text_button_id);
        if (!f) {
            GWCA_ERR("[UI] SetTextButtonTextByFrameId — invalid frame %u", text_button_id);
            return false;
        }
        GW::UI::SendFrameUIMessage(f, static_cast<GW::UI::UIMessage>(0x5F),
            const_cast<wchar_t*>(caption.c_str()), nullptr);
        return true;
    }

    // ================= edit =================
// ============================================================================
// Editable text (edit box) — CORRECT build path. (Ghidra-verified, Gw.exe 06-14.)
//
// CtlEditProc (0x00888aa0) is ONLY the render/caret/input SUBCLASS: its switch has NO
// case 9 (OnFrameCreate) and NO case 4 (class vtable), so registering it as the primary
// component callback (or as a bare frame-list item via CtlFrameListCreateItem) attaches no
// CCtlEdit value context and every value msg (0x5E/0x57/0x5A) is dead / faults. The game
// builds edit boxes (decompiled GmChat builder FUN_0051b580) by registering the OUTER CCtlEdit
// proc FUN_008852e0 through CreateUIComponent:
//   msg 4   -> installs C++ class vtable   *(ctx+0xc)=0x00619c50
//   msg 9   -> FUN_0062f150(frame, CtlEditProc 0x00888aa0, 0)  attaches the render subclass
//   msg 100 -> copies the 7-ptr value-interface table from 0x00b96960 (REP MOVSD)
// That outer proc is what makes SetText(0x5E)/GetText(0x57)/SetMaxLength(0x5A) functional.
// => edit MUST be a DIRECT CHILD via CreateUIComponent, NOT a (no-stretch) frame-list item.
//
// Resolver: the msg-100 REP MOVSD from 0x00b96960 anchors the outer proc. The 41-byte
// prologue is verified UNIQUE @ 0x008852e0 (search_byte_patterns, Gw.exe 06-14).
static uintptr_t ResolveOuterCCtlEditProc()
{
    static uintptr_t cached = 0;
    if (cached)
        return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x8B\x4D\x08\x8B\x41\x04\x83\xF8\x04\x74\x3B\x83\xF8\x09\x74\x23\x83\xF8\x64\x75\x15\x56\x57\x8B\x7D\x10\xB9\x07\x00\x00\x00\xBE\x60\x69\xB9\x00\xF3\xA5",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0);
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveOuterCCtlEditProc — pattern not found");
        return 0;
    }
    cached = addr;
    return cached;
}

// (Ghidra-verified) s_editCaretMaterial cluster (Gw.exe 06-14 = 0x01081d78 caret /
// 0x01081d7c selection / 0x01081d80 glyph). Built ONCE in CtlEditProc (0x00888aa0) msg 5 under
// `if(s_editCaretMaterial==0){...build...}else{FUN_00487a80(0x3c) FATAL}`. So msg 5 is SAFE
// only while the global is NULL — identical guard to EnsureBtnCheckImageList. In a live in-game
// session a chat box already ran msg 5, so it is warm and we send nothing. NOTE: 0x01081d78 is
// build-specific (resolve from CtlEditProc's msg-5 store for build independence later).
static bool IsEditCaretMaterialReady()
{
    volatile uint32_t* g = reinterpret_cast<uint32_t*>(0x01081d78u);
    return *g != 0;
}

static void EnsureEditCaretMaterial(uint32_t edit_frame_id)
{
    volatile uint32_t* g = reinterpret_cast<uint32_t*>(0x01081d78u);
    if (*g == 0) {
        GW::UI::Frame* f = GW::UI::GetFrameById(edit_frame_id);
        if (f) GW::UI::SendFrameUIMessage(f, static_cast<GW::UI::UIMessage>(0x05), nullptr, nullptr);
    }
}

// Build a REAL editable text box as a DIRECT CHILD of a window (NOT a frame-list item / no-stretch
// row — CtlEditProc has NO msg 0x38 self-size and the CCtlEdit value context only exists on the
// CreateUIComponent child path). Registers the OUTER CCtlEdit proc (0x008852e0), applies an
// explicit size (else it renders 0x0), and warms the caret material iff cold. component_flags
// default 0x892e000 = the game's own full "EditName" edit flags (decompiled from FUN_0051b580;
// simple single-line variant = proc 0x0087d9a0 + flags 0x2020000). parent_frame_id MUST be a real
// CreateNativeWindow child. Returns the edit frame id, or 0.
static uint32_t CreateEditBoxChildByFrameId(
    uint32_t parent_frame_id,
    const std::wstring& label = L"EditBox",
    uint32_t child_index = 0,
    uint32_t component_flags = 0x892e000,
    float width = 200.0f, float height = 20.0f)
{
    const uintptr_t proc = ResolveOuterCCtlEditProc();
    if (!proc) {
        GWCA_ERR("[UI] CreateEditBoxChildByFrameId — outer CCtlEdit proc not resolved");
        return 0xFFFFFFFFu;
    }
    // (Ghidra-verified) GW frame ids are 0-BASED and slot 0 is recycled — id 0 is a VALID handle, not a
    // failure. The old `if(!id) return 0` discarded a valid id-0 edit and skipped its size+seed → blank
    // box (log showed "edit created ok id=0"). Guard the PARENT up front and use 0xFFFFFFFF as the only
    // failure sentinel; let id 0 fall through to sizing + the msg-0x5A/0x5E seed.
    GW::UI::Frame* parent = GW::UI::GetFrameById(parent_frame_id);
    if (!(parent && parent->IsCreated())) {
        GWCA_ERR("[UI] CreateEditBoxChildByFrameId — invalid parent %u", parent_frame_id);
        return 0xFFFFFFFFu;
    }
    // Nest under the window's native CONTENT frame (band 0x2710) so IUi::PopCloser frees it on [X].
    const uint32_t id = CreateUIComponentByFrameId(
        EnsureContentFrameForWindow(parent_frame_id), component_flags, child_index,
        static_cast<uintptr_t>(proc), std::wstring(), label);
    // Size with ANCHOR 6 (0x0062F770), NOT FrameSetSize: CtlEditProc has no msg 0x38 self-size, and
    // as a direct window child a plain FrameSetSize is overwritten by the window layout each pass →
    // the frame collapses to 0x0 and paints nothing ("empty window"). The anchor-6 pos/size setter
    // pins it (same fix as the slider). (2026-07-01)
    if (width > 0.0f && height > 0.0f) {
        if (!g_noStretch_FrameSetPositionPosSize) {
            const uintptr_t a = ResolveFrameSetPositionPosSize();
            if (a) g_noStretch_FrameSetPositionPosSize = reinterpret_cast<FrameSetPositionPosSize_pt>(a);
        }
        if (g_noStretch_FrameSetPositionPosSize) {
            const Coord2f pos  = { 10.0f, 10.0f };
            const Coord2f size = { width, height };
            g_noStretch_FrameSetPositionPosSize(id, &pos, &size);
        } else {
            FrameSetSizeByFrameId(id, width, height);
        }
    }
    EnsureEditCaretMaterial(id);                     // warm caret/sel/glyph iff NULL (safe)
    // (Ghidra-verified, "/Gw.exe (06-14)") The edit only paints once its value store is seeded. The
    // game's EditName builder FUN_0051b580 sends SetMaxLength (msg 0x5A = FUN_00604aa0) then SetText
    // (msg 0x5E = FUN_00604b00) post-create; without them the value frame reports ZERO content size and
    // CtlEditProc's paint enumerates no glyph/caret/background sub-passes → nothing draws ("empty
    // window"), even with a valid frame rect. Seed both: max length first (allocates/sizes the store),
    // then a single ENCODED space so it lays out and issues its first paint. (Raw wide strings trip the
    // SetText assert — must be encoded.)
    if (GW::UI::Frame* ef = GW::UI::GetFrameById(id)) {
        GW::UI::SendFrameUIMessage(ef, static_cast<GW::UI::UIMessage>(0x5A),
            reinterpret_cast<void*>(static_cast<uintptr_t>(0x100)), nullptr);
        std::wstring seed = BuildStandaloneLiteralEncodedTextPayload(L" ");
        GW::UI::SendFrameUIMessage(ef, static_cast<GW::UI::UIMessage>(0x5E),
            const_cast<wchar_t*>(seed.c_str()), nullptr);
    }
    return id;
}

// Set edit text (msg 0x5E; game accessor FUN_00604b00). Text MUST be an encoded payload — a raw
// wide string trips the SetText null/encoding assert. Returns false if the frame is invalid.
static bool SetEditBoxTextByFrameId(uint32_t frame_id, const std::wstring& plain_text)
{
    GW::UI::Frame* f = GW::UI::GetFrameById(frame_id);
    if (!(f && f->IsCreated()))
        return false;
    std::wstring encoded = BuildStandaloneLiteralEncodedTextPayload(plain_text);
    GW::UI::SendFrameUIMessage(f, static_cast<GW::UI::UIMessage>(0x5E),
        const_cast<wchar_t*>(encoded.c_str()), nullptr);
    return true;
}

// Set edit max input length (msg 0x5A; game accessor FUN_00604aa0). max_length is the raw wparam
// value (not a pointer), exactly as the engine passes it.
static bool SetEditBoxMaxLengthByFrameId(uint32_t frame_id, uint32_t max_length)
{
    GW::UI::Frame* f = GW::UI::GetFrameById(frame_id);
    if (!(f && f->IsCreated()))
        return false;
    GW::UI::SendFrameUIMessage(f, static_cast<GW::UI::UIMessage>(0x5A),
        reinterpret_cast<void*>(static_cast<uintptr_t>(max_length)), nullptr);
    return true;
}

// GetText is msg 0x57 (game accessor FUN_00603c40). Because the frame is now built via the outer
// proc, the EXISTING GetEditableTextValueByFrameId (GWCA GW::EditableTextFrame::GetValue) reads
// the live CCtlEdit context correctly — reuse it rather than a hand-rolled 0x57 buffer call.

    // ================= progress =================
// ============================================================================
// PLACEMENT 1 — free-function resolver: add next to ResolveCtlBtnProc (~py_ui.h:305).
// ============================================================================

// CtlProgress::CProgressFrame::FrameProc @ EXE 0x008812e0 (WASM ram:80f6cf82) — the REAL
// progress-bar FrameProc. Single-layer, self-driving: msg 4 declares base &LAB_00618b70
// (=FrameWithValue FUN_00618d00), msg 5 builds the 3 class-static image lists incl.
// sm_rateArrowImageList (via the LOADER FUN_00882530), msg 9 allocs the 0x28 instance ctx
// and self-arms the 0x45 anim tick; value/max/percent fall through to the FrameWithValue
// base (thunk_FUN_00647170). It carries NO assertion-message string of its own, so it
// CANNOT be anchored by FindAssertion — the GWCA anchor
// FindAssertion("UiCtlProgress.cpp","!sm_rateArrowImageList")->ToFunctionStart wrongly
// lands on the rate-arrow image-list LOADER FUN_00882530 (a void(void), the string's ONLY
// referrer — verified via get_xrefs_to: single caller 0x0088159b inside 0x008812e0), which
// crashes when invoked as a FrameProc. Anchor instead on the proc's own unique jump-table
// prologue (26 bytes = the function ENTRY):
//   push ebp; mov ebp,esp; sub esp,0x18; push ebx; mov ebx,[ebp+0xC]; push esi;
//   mov esi,[ebp+8]; push edi; mov eax,[esi+4]; add eax,-4; cmp eax,0x5E; ja ...
// Verified UNIQUE @ 0x008812e0 in Gw.exe build 06-14 (match IS the entry; no ToFunctionStart).
static uintptr_t ResolveCtlProgressProc()
{
    static uintptr_t cached = 0;
    if (cached)
        return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x83\xEC\x18\x53\x8B\x5D\x0C\x56\x8B\x75\x08\x57\x8B\x46\x04\x83\xC0\xFC\x83\xF8\x5E\x0F\x87",
        "xxxxxxxxxxxxxxxxxxxxxxxxxx", 0);
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveCtlProgressProc — pattern not found");
        return 0;
    }
    cached = addr;
    return cached;
}

// ============================================================================
// PLACEMENT 2 — UIManager members: add next to CreateProgressBarByFrameId (~py_ui.h:3492).
// ============================================================================

    // Creates a WORKING native progress bar as a POSITIONED DIRECT CHILD of parent_frame_id,
    // using the CORRECT CtlProgress proc (0x008812e0). This bypasses GWCA's
    // GW::ProgressBar::Create, whose callback anchor resolves to the rate-arrow LOADER and
    // crashes. Registering the real proc lets the engine self-run msg 4/5/9 (base wiring,
    // class-static image lists incl. sm_rateArrowImageList, ctx alloc). The proc self-sizes
    // HEIGHT only (msg 0x38 echoes the proposed WIDTH), so width/x/y are supplied explicitly
    // or it renders 0-wide. To make the fill actually PAINT, the paint-style must include bit
    // 0x10 (primary bar); this helper sets it via msg 0x64. Drive the fill afterwards with
    // SetProgressBar(Value|Percent|Max)ByFrameId. Returns the child frame id, or 0.
    static uint32_t CreateProgressBarChildByFrameId(
        uint32_t parent_frame_id,
        float x = 10.0f, float y = 10.0f,
        float width = 160.0f, float height = 0.0f,
        uint32_t child_index = 0,
        uint32_t component_flags = 0x300)
    {
        const uintptr_t proc = ResolveCtlProgressProc();
        if (!proc) {
            GWCA_ERR("[UI] CreateProgressBarChildByFrameId — CtlProgress proc not resolved");
            return 0;
        }
        // Same typed-component create GW::ProgressBar::Create uses, but with the correct proc.
        // msg 9 ignores userData/name, so both are empty. Nest under the band 0x2710 content frame so
        // IUi::PopCloser frees it on the native [X].
        const uint32_t id = CreateUIComponentByFrameId(
            EnsureContentFrameForWindow(parent_frame_id), component_flags, child_index,
            static_cast<uintptr_t>(proc), std::wstring(), std::wstring());
        if (!id)
            return 0;
        // Force width (self-size only covers height). Height 0 -> a sane default; a caller
        // height overrides the proc's font-height self-size.
        if (width > 0.0f)
            FrameSetSizeByFrameId(id, width, height > 0.0f ? height : 16.0f);
        {
            using FrameSetPos_pt = void(__cdecl*)(uint32_t, Coord2f*);
            static FrameSetPos_pt pos_fn = nullptr;
            if (!pos_fn) { const auto a = ResolveFrameSetPositionCoord2f(); if (a) pos_fn = reinterpret_cast<FrameSetPos_pt>(a); }
            if (pos_fn) { Coord2f c = { x, y }; pos_fn(id, &c); }
        }
        // Paint-style: OR in the primary-bar bit (0x10) so the fill renders (msg 0x64 SetStyle).
        SendFrameUIMessage(id, 0x64, 0x10, 0);
        return id;
    }

    // Sets the progress fill as a percentage 0..100 (base FrameWithValue SetPercent, msg 0x5B;
    // wparam is the raw percent — the base computes value = max*percent/100).
    static bool SetProgressBarPercentByFrameId(uint32_t frame_id, uint32_t percent) {
        return SendFrameUIMessage(frame_id, 0x5B, static_cast<uintptr_t>(percent), 0);
    }

    // Reads the current progress max (base FrameWithValue GetMax, msg 0x57; out via lparam).
    static uint32_t GetProgressBarMaxByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return 0;
        uint32_t out = 0;
        GW::UI::SendFrameUIMessage(frame, static_cast<GW::UI::UIMessage>(0x57), nullptr, &out);
        return out;
    }

    // Sets an animated auto-advance rate in units/second (base FrameWithValue, msg 0x59;
    // wparam is a POINTER to a float — the base reads *(float*)wparam, then arms msg 0x45).
    static bool SetProgressBarIncrementsPerSecondByFrameId(uint32_t frame_id, float per_second) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;
        GW::UI::SendFrameUIMessage(frame, static_cast<GW::UI::UIMessage>(0x59), &per_second, nullptr);
        return true;
    }

    // Sets the on-bar overlay text (proc-level msg 0x62 -> ctx+0x04, invalidates draw). The
    // text MUST be a GW-encoded payload (BuildStandaloneLiteralEncodedTextPayload); a raw
    // wide string is unsafe. Pass an empty string to clear.
    static bool SetProgressBarOverlayTextByFrameId(uint32_t frame_id, const std::wstring& enc_text) {
        return SendFrameUIMessage(frame_id, 0x62,
            reinterpret_cast<uintptr_t>(enc_text.empty() ? nullptr : enc_text.c_str()), 0);
    }

    // ================= tabs =================
// ============================================================================
// TABS (CtlPage / CtlPageProc @ EXE 0x0061a950, WASM ram:80e078f3)
// ----------------------------------------------------------------------------
// Self-contained container FrameProc. msg 0x09 (create) self-allocs an 8-byte
// instance {+0x00 frame handle, +0x04 active_index = 0xffffffff} and does NOT
// register a multi-layer base (unlike slider/dropdown) → renders cold as a plain
// frame-list item, no crash. It self-lays-out its tab-button strip + active body
// on 0x37/0x38 and derives its size ONLY from its children (verified FUN_0061b2d0):
// with ZERO tabs it reports {0,0} and renders 0x0 — you MUST add >=1 tab. Never
// FrameSetSize it and never position its children.
//
// Resolve via the full 30-byte function-ENTRY prologue (the match IS the entry —
// no ToFunctionStart). Verified UNIQUE @ 0x0061a950 in Gw.exe build 06-14-2026
// (search_byte_patterns → single hit).
static uintptr_t ResolveCtlPageProc()
{
    static uintptr_t cached = 0;
    if (cached)
        return cached;
    const uintptr_t addr = GW::Scanner::Find(
        "\x55\x8B\xEC\x8B\x45\x08\x83\xEC\x0C\x53\x8B\x5D\x10\x56\x8B\x75\x0C\x57\x8B\x78\x08\x8B\x40\x04\x83\xC0\xFC\x83\xF8\x5A",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0);
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveCtlPageProc — pattern not found");
        return 0;
    }
    cached = addr;
    return cached;
}

// ---- The following are static members of class UIManager (place beside the
//      other frame-list-item helpers, e.g. after AddFlatButtonItemToFrameListByFrameId) ----

    // Create a TABS container (CtlPage) as a frame-list item — the crash-safe path.
    // The managed list supplies the parent context; CtlPageProc msg 0x09 self-allocs
    // its instance and self-lays-out. RENDERS 0x0 UNTIL A TAB IS ADDED (msg 0x38
    // sizes from children). Do NOT FrameSetSize the returned container.
    // Returns the tabs container frame id, or 0.
    static uint32_t CreateTabsAsListItemByFrameId(
        uint32_t frame_list_id,
        uint32_t insert_index = 0,
        uint32_t flags = 0x300)
    {
        const uintptr_t proc = ResolveCtlPageProc();
        if (!proc) {
            GWCA_ERR("[UI] CreateTabsAsListItemByFrameId — CtlPageProc not resolved");
            return 0;
        }
        // Empty encoded userData — CtlPageProc msg 0x09 ignores it (harmless).
        std::wstring encoded = BuildStandaloneLiteralEncodedTextPayload(L"");
        return CtlFrameListCreateItemByFrameId(frame_list_id, flags, insert_index,
                                               static_cast<uint32_t>(proc), encoded);
    }

    // Add a tab to a CtlPage container (msg 0x56 → FUN_0061ad40). Builds the 5-dword
    // CtlPageItem {caption, flags, code, proc, ctx} the engine reads:
    //   caption -> tab-button userData (MUST be GW-literal-encoded; a raw wchar_t* crashes)
    //   flags   -> body create flags (engine ORs 0x200 for the page body); 0x20000 = the game value
    //   code    -> tab index, MUST be >= 0 and unique (asserted "!IsBtnCode(pageCode)")
    //   proc    -> body content FrameProc (MUST be valid & self-contained; 0 = null proc = unsafe)
    //   ctx     -> body userData (for a text proc it MUST be an encoded payload)
    // Engine builds body = FrameCreate(page, flags|0x200, code, proc, ctx) and tab button =
    // FrameCreate(page, 0x20100, ~code, flat CtlBtnProc 0x0060f4f0, caption); FIRST tab auto-selects.
    // Returns the tab BODY frame id (in msg lparam) to populate, or 0.
    static uint32_t AddTabToPageByFrameId(
        uint32_t tabs_frame_id,
        const std::wstring& tab_caption,
        uint32_t tab_code,
        uintptr_t body_proc = 0,
        const std::wstring& body_text = L"")
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(tabs_frame_id);
        if (!frame) {
            GWCA_ERR("[UI] AddTabToPageByFrameId — invalid tabs_frame_id %u", tabs_frame_id);
            return 0;
        }
        if (static_cast<int32_t>(tab_code) < 0) {
            GWCA_ERR("[UI] AddTabToPageByFrameId — tab_code must be >= 0");
            return 0;
        }
        // Never pass a null body proc: default to the self-contained text-label proc.
        if (!body_proc)
            body_proc = reinterpret_cast<uintptr_t>(ResolveTextLabelFrameCallback());
        if (!body_proc) {
            GWCA_ERR("[UI] AddTabToPageByFrameId — body proc not resolved");
            return 0;
        }
        // Encoded payloads must stay alive across the msg 0x56 send (engine copies them).
        std::wstring caption_enc = BuildStandaloneLiteralEncodedTextPayload(
            tab_caption.empty() ? std::wstring(L" ") : tab_caption);
        std::wstring body_enc = BuildStandaloneLiteralEncodedTextPayload(
            body_text.empty() ? std::wstring(L" ") : body_text);

        // 5-dword CtlPageItem {caption, flags, code, proc, ctx} (32-bit dwords).
        uintptr_t item[5];
        item[0] = reinterpret_cast<uintptr_t>(caption_enc.c_str());
        item[1] = 0x20000;
        item[2] = static_cast<uintptr_t>(tab_code);
        item[3] = body_proc;
        item[4] = reinterpret_cast<uintptr_t>(body_enc.c_str());

        uint32_t out_body_frame_id = 0;
        // wparam = &item (param_2), lparam = &out_body (param_3) — verified FUN_0061ad40.
        GW::UI::SendFrameUIMessage(frame, static_cast<GW::UI::UIMessage>(0x56),
            reinterpret_cast<void*>(item), &out_body_frame_id);
        return out_body_frame_id;
    }

    // Select the active tab by index (msg 0x5d → asserts index>=0, then SetActive(index,0)).
    static bool TabSetActiveByFrameId(uint32_t tabs_frame_id, uint32_t index)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(tabs_frame_id);
        if (!frame) return false;
        return GW::UI::SendFrameUIMessage(frame, static_cast<GW::UI::UIMessage>(0x5d),
            reinterpret_cast<void*>(static_cast<uintptr_t>(index)), nullptr);
    }

    // Current active tab index (msg 0x59 → *out = instance+4). Returns 0xffffffff until the
    // first tab exists; poll each frame and diff to detect user tab-button clicks.
    static uint32_t TabGetActiveByFrameId(uint32_t tabs_frame_id)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(tabs_frame_id);
        if (!frame) return 0xffffffffu;
        uint32_t out = 0xffffffffu;
        GW::UI::SendFrameUIMessage(frame, static_cast<GW::UI::UIMessage>(0x59), nullptr, &out);
        return out;
    }

    // Body frame id for the given tab index (msg 0x5a → FrameGetChild(page, index)).
    static uint32_t TabGetBodyFrameByFrameId(uint32_t tabs_frame_id, uint32_t index)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(tabs_frame_id);
        if (!frame) return 0;
        uint32_t out = 0;
        GW::UI::SendFrameUIMessage(frame, static_cast<GW::UI::UIMessage>(0x5a),
            reinterpret_cast<void*>(static_cast<uintptr_t>(index)), &out);
        return out;
    }

    // Enable (msg 0x58, flag 1) / disable (msg 0x57, flag 0) a tab by index — verified FUN_0061a950.
    static bool SetTabEnabledByFrameId(uint32_t tabs_frame_id, uint32_t index, bool enabled)
    {
        GW::UI::Frame* frame = GW::UI::GetFrameById(tabs_frame_id);
        if (!frame) return false;
        return GW::UI::SendFrameUIMessage(frame,
            static_cast<GW::UI::UIMessage>(enabled ? 0x58 : 0x57),
            reinterpret_cast<void*>(static_cast<uintptr_t>(index)), nullptr);
    }

    // ================= slider =================
// ── Slider (MULTI-LAYER: CtlSliderProc base @ EXE 0x00615fe0 + UiCtlSliderProc wrapper @ EXE 0x0087f440) ──
// Ghidra-verified (Gw.exe 06-14): the slider is a TWO-LAYER typed control and CANNOT be created
// single-proc. The frame-list-item path (AddControlItemByFrameId -> CtlFrameListCreateItem 0x00612900)
// registers only ONE proc and never runs the msg-4 base-declaration cascade, so frame+0xB0 (subclass
// depth) is too shallow and the FIRST click (msg 0x24 -> FrameMsgCallBase thunk_FUN_00647170) fires the
// depth assertion FUN_00487a80(0x223) => CRASH. (Create msg 9 / destroy msg 0xB are exempt in
// FUN_00647170 — that is why creation itself did not crash, only the first click did.)
// Confirmed handshake: wrapper 0x0087f440 case 4 => *(code**)param_2[3]=CtlSliderProc(0x00615fe0);
// base 0x00615fe0 case 4 => *(code**)param_2[3]=FUN_006123a0. Only a typed FrameCreate runs that
// cascade — which is exactly what GWCA's UI::CreateSliderFrame does (CreateUIComponent(base) +
// FrameNewSubclass(wrapper)), already wrapped by CreateSliderFrameByFrameId() below.
//
// Crash-safe one-shot creator (fills the orphaned py_ui.h:3901 comment that was never implemented).
// PARENT MUST be a real managed window (CreateNativeWindow), NOT a frame list.
// Order is load-bearing:
//   1) two-layer typed create (runs msg-4 cascade => click-safe)
//   2) SetRange (msg 0x56) FIRST — SetValue asserts min<=v<=max, so range must exist
//   3) SetValue (msg 0x57)
//   4) explicit FrameSetSize — base msg-0x38 self-size is proportional to (max-min) => "extremely wide"
// Read the live value each frame via GetSliderValueByFrameId (msg 0x58); the base updates its internal
// position on drag (case 0x2c/0x2e) even if the parent ignores notify codes 7/9, so polling reflects
// user drags with no ImGui hit-test.
static uint32_t CreateSliderControlByFrameId(
    uint32_t parent_window_id,
    uint32_t min_value,
    uint32_t max_value,
    uint32_t initial_value,
    float    width  = 150.0f,
    float    height = 18.0f,
    uint32_t child_index = 0)
{
    // Guard: base msg-0x38 / msg-0x57 divide & compare against (max-min); zero/inverted range is unsafe.
    if (max_value <= min_value)
        return 0;
    if (initial_value < min_value) initial_value = min_value;
    if (initial_value > max_value) initial_value = max_value;

    // (1) SINGLE create with the WRAPPER UiCtlSliderProc (0x0087f440) as the PRIMARY proc — mirrors the
    //     game (FUN_00507210) and the tabs fix. Its case 4 installs base CtlSliderProc as sub-handler.
    //     Do NOT use CreateSliderFrameByFrameId (base-primary + FrameNewSubclass(wrapper)) — that re-fires
    //     msg 9/0xb and crashes on create. flags 0x40000 (paint gate), NOT 0x300; empty userdata so msg 9
    //     seeds min=max=0 with no deref (range is set next).
    const uintptr_t slider_proc = ResolveUiCtlSliderProc();
    if (!slider_proc) {
        GWCA_ERR("[UI] CreateSliderControlByFrameId — UiCtlSliderProc (0x0087f440) not resolved");
        return 0;
    }
    // Nest under the window's native CONTENT frame (band 0x2710) so IUi::PopCloser frees it on [X].
    const uint32_t slider_id = CreateUIComponentByFrameId(
        EnsureContentFrameForWindow(parent_window_id), 0x40000, child_index, slider_proc, std::wstring(), std::wstring());
    if (!slider_id)
        return 0;

    // (2) SetRange (msg 0x56) MUST precede SetValue — msg 0x57 asserts min<=v<=max (fatal otherwise).
    SetSliderRangeByFrameId(slider_id, min_value, max_value);
    // (3) SetValue (msg 0x57) — safe now that range is set.
    SetSliderValueByFrameId(slider_id, initial_value);
    // (4) Explicit size AFTER range (so base measure msg 0x38 uses a real max-min). Plain FrameSetSize
    //     only — the anchor-6 setter (0x0062F770) asserts (inverted-rect 0x238) on the self-measuring
    //     slider container. Slider still renders range-wide; true width control = range-span (open item).
    FrameSetSizeByFrameId(slider_id, width, height);

    return slider_id;
}

// Tear down a slider created by CreateSliderControlByFrameId — use this INSTEAD of a bare destroy.
// (Ghidra-verified, "/Gw.exe (06-14)") CtlSliderProc (0x00615fe0) registers an auto-scroll CTimer
// node on a groove/track click (case 0x24 → FUN_00630080 → alloc) and only UNREGISTERS it on mouse-up
// (case 0x2e → FUN_00630040). The destroy path (case 0xb) frees the instance but NEVER unregisters the
// timer, so destroying a slider while a groove timer is live orphans the node AND leaves a repeating
// timer firing on freed memory → the "crashes after fiddling / seems like a leak" symptom. Send a
// synthetic mouse-up (msg 0x2e, button==0) FIRST to release any pending timer, THEN destroy. Safe even
// when no timer is live (the unregister just walks the list and finds nothing).
static bool DestroySliderControlByFrameId(uint32_t slider_id)
{
    if (!slider_id)
        return false;
    GW::UI::Frame* f = GW::UI::GetFrameById(slider_id);
    if (!f)
        return false;
    // msg 0x2e reads *wparam == 0 (button up) to enter the release branch; zeroed 0x20-byte payload.
    uint32_t up[8] = { 0 };
    GW::UI::SendFrameUIMessage(f, static_cast<GW::UI::UIMessage>(0x2E), up, nullptr);
    return DestroyUIComponentByFrameId(slider_id);
}

    // ================= groupheader =================
// ─────────────────────────────────────────────────────────────────────────────
// CGroupHeaderFrame::FrameProc @ EXE 0x0087ddc0 (WASM ram:81192c89) — GROUP HEADER
// composite. On msg 9 it inline-allocates its 0x60-byte TCtlInstance (vtable
// 0x00b96070) and its base OnFrameCreate self-builds TWO children and lays them out:
//   child0 = CheckOpen checkbox (proc FUN_008867f0; self-builds image list
//            DAT_01081d70 on msg 5, frees on msg 6 → NO cold-art crash)
//   child1 = CtlTextBtnProc caption (proc FUN_00616c00, TxtName)
// positioned via named template L"UiCtlGroupHeader" (FramePlaceChildren FUN_0062f1a0).
// It renders + self-lays-out cold; do NOT create children or FrameSetSize/Position.
//
// Message protocol (verified from the switch in FUN_0087ddc0, 06-14 — the catalog had
// 0x57/0x58 SWAPPED):
//   0x56 GetIsOpen(lparam=&u32 out)  reads child0 checked bit (FUN_0060f4b0)
//   0x57 GetText(lparam=TArray<wchar_t>* out)
//   0x58 SetIsOpen(wparam=bool value) sets child0 checked bit (FUN_0060f490)
//   0x59 SetText(wparam=wchar_t* text) forwards to child1 CtlTextBtnSetText (FUN_006177b0)
//
// Resolver: the OLD anchor {file="UiCtlGroupHeader.cpp", msg="UiCtlGroupHeader.cpp"}
// passed the FILENAME as the assertion EXPRESSION → GW::Scanner::FindAssertion returned 0
// (this was the in-client failure). The real assert expression inside the msg-0x56 path
// is "isOpen" (EXE string 0x009495b4, pushed at 0x0087ddfe right after the file string
// at 0x0087ddf9 — both xref'd only inside this proc). Fallback expr "decodedName" (0x57).
static uintptr_t ResolveGroupHeaderProc()
{
    static uintptr_t cached = 0;
    if (cached)
        return cached;

    uintptr_t addr = GW::Scanner::FindAssertion(
        "UiCtlGroupHeader.cpp", "isOpen", 0, 0);
    if (!addr) {
        addr = GW::Scanner::FindAssertion(
            "UiCtlGroupHeader.cpp", "decodedName", 0, 0);
    }
    if (!addr) {
        GWCA_ERR("[SCAN] ResolveGroupHeaderProc — assertion not found");
        return 0;
    }
    const uintptr_t func_start = GW::Scanner::ToFunctionStart(addr, 0xFFF);
    if (!func_start) {
        GWCA_ERR("[SCAN] ResolveGroupHeaderProc — ToFunctionStart failed");
        return 0;
    }
    cached = func_start;   // → 0x0087ddc0
    return cached;
}

// ── The following are static members of UIManager (place beside
//    AddButtonItemToFrameListByFrameId / IsButtonPushedByFrameId). ──────────────

// Add a native GROUP HEADER (collapsible section) row to a frame list. Same proven
// CtlFrameListCreateItem path as add_text/button items, but with the group-header proc;
// the engine self-builds the checkbox+caption children and positions them. Do NOT send
// 0x5E and do NOT FrameSetSize/FrameSetPosition. item_flags: 0x1000 = non-collapsible
// (hides the CheckOpen box), 0x2000 = disable caption mouse. Returns the item's frame ID.
static uint32_t AddGroupHeaderItemToFrameListByFrameId(
    uint32_t frame_list_id,
    const std::wstring& header_text,
    uint32_t insert_index = 0,
    uint32_t item_flags = 0)
{
    if (header_text.empty()) {
        GWCA_ERR("[UI] AddGroupHeaderItemToFrameListByFrameId — empty header text");
        return 0;
    }
    const uint32_t proc = static_cast<uint32_t>(ResolveGroupHeaderProc());
    if (!proc) {
        GWCA_ERR("[UI] AddGroupHeaderItemToFrameListByFrameId — GroupHeader proc not resolved");
        return 0;
    }
    std::wstring encoded = BuildStandaloneLiteralEncodedTextPayload(header_text);
    return CtlFrameListCreateItemByFrameId(frame_list_id, item_flags,
                                           insert_index, proc, encoded);
}

// Group header expanded/collapsed state (msg 0x56 — reads child0 checkbox). Poll each frame.
static bool GroupHeaderGetIsOpenByFrameId(uint32_t frame_id)
{
    auto* frame = GW::UI::GetFrameById(frame_id);
    if (!(frame && frame->IsCreated()))
        return false;
    uint32_t out = 0;
    GW::UI::SendFrameUIMessage(
        frame, static_cast<GW::UI::UIMessage>(0x56), nullptr, &out);
    return out != 0;
}

// Set group header expanded/collapsed (msg 0x58 — wparam = value). Returns true on send.
static bool GroupHeaderSetIsOpenByFrameId(uint32_t frame_id, bool is_open)
{
    return SendFrameUIMessage(frame_id, 0x58,
        static_cast<uintptr_t>(is_open ? 1u : 0u), 0);
}

// Set group header caption (msg 0x59 — wparam = GW-literal-encoded text, forwarded to
// child1 CtlTextBtnSetText). The message is handled synchronously, so the temporary
// encoded payload is safe for the duration of the send.
static bool GroupHeaderSetTextByFrameId(uint32_t frame_id, const std::wstring& header_text)
{
    auto* frame = GW::UI::GetFrameById(frame_id);
    if (!(frame && frame->IsCreated()))
        return false;
    std::wstring encoded = BuildStandaloneLiteralEncodedTextPayload(header_text);
    return GW::UI::SendFrameUIMessage(
        frame, static_cast<GW::UI::UIMessage>(0x59),
        encoded.empty() ? nullptr : const_cast<wchar_t*>(encoded.c_str()),
        nullptr);
}

    // Generic: add ANY researched control as a frame-list item, resolving its FrameProc by
    // name via the same proven CtlFrameListCreateItem path. This is the test surface for the
    // whole UI-element swarm — one call per control, so a risky control can be tried in
    // isolation. Returns the item frame id, or 0 (0 = proc unresolved / not creatable — this
    // function itself never crashes; a crash, if any, is inside the native item creation for
    // controls still marked "needs_workaround").
    //
    // control names: "flat_button","checkbox","radio","styled_button","text_button",
    //   "text_label","dropdown","slider","edit","progress","tabs","groupheader".
    // Button-family controls also get their caption set (msg 0x5E) and an explicit size.
    static uint32_t AddControlItemByFrameId(
        uint32_t frame_list_id,
        const std::string& control,
        const std::wstring& caption,
        uint32_t insert_index = 0,
        uint32_t item_flags = 0)
    {
        uintptr_t proc = 0;
        bool button_family = false;
        if (control == "flat_button" || control == "checkbox" || control == "radio") {
            proc = ResolveCtlBtnProc(); button_family = true;
        } else if (control == "text_button" || control == "hyperlink") {
            proc = ResolveCtlTextButtonProc();
        } else if (control == "text_label") {
            proc = reinterpret_cast<uintptr_t>(ResolveTextLabelFrameCallback());
        } else {
            // Assertion-anchored procs (build-stable), mirroring UIMgr.cpp resolvers.
            const char* file = nullptr; const char* msg = nullptr;
            if (control == "styled_button") { file = "UiCtlBtn.cpp"; msg = "!s_btnCheckImageList"; button_family = true; }
            else if (control == "dropdown")  { file = "UiCtlDropMenu.cpp"; msg = "!FrameGetChild(thisFrame, CTL_LIST_ENTRIES)"; }
            else if (control == "slider")    { file = "CtlSlider.cpp"; msg = "value >= m_range.min"; }
            else if (control == "edit")      { file = "UiCtlEditBox.cpp"; msg = "!s_editCaretMaterial"; }
            else if (control == "progress")  { file = "UiCtlProgress.cpp"; msg = "!sm_rateArrowImageList"; }
            else if (control == "tabs")      { file = "CtlPage.cpp"; msg = "!IsBtnCode(pageCode)"; }
            else if (control == "groupheader"){ file = "UiCtlGroupHeader.cpp"; msg = "UiCtlGroupHeader.cpp"; }
            if (file) {
                const uintptr_t a = GW::Scanner::FindAssertion(file, msg, 0, 0);
                if (a) proc = GW::Scanner::ToFunctionStart(a, 0xFFF);
            }
        }
        if (!proc) {
            GWCA_ERR("[UI] AddControlItemByFrameId — proc not resolved for '%s'", control.c_str());
            return 0;
        }
        // (Ghidra-verified) The styled 9-slice button (UiCtlBtnProc) msg-0x01 paint sub-0 does
        // `FrameTestStyles(frame,0x40000); if(==0) return;` — without 0x40000 it draws NOTHING.
        if (control == "styled_button") item_flags |= 0x40000;
        std::wstring encoded = BuildStandaloneLiteralEncodedTextPayload(caption.empty() ? std::wstring(L" ") : caption);
        const uint32_t item_id = CtlFrameListCreateItemByFrameId(
            frame_list_id, item_flags, insert_index, static_cast<uint32_t>(proc), encoded);
        if (!item_id)
            return 0;
        if (button_family && !caption.empty()) {
            GW::UI::Frame* f = GW::UI::GetFrameById(item_id);
            if (f) {
                GW::UI::SendFrameUIMessage(f, static_cast<GW::UI::UIMessage>(0x5E),
                    const_cast<wchar_t*>(caption.c_str()), nullptr);
            }
            FrameSetSizeByFrameId(item_id, 120.0f, 22.0f);
        }
        // Styled button's other gate: the shared s_btnCheckImageList must exist (else sub-1's
        // FrameContentAddImage gets a null handle and paints nothing).
        if (control == "styled_button") EnsureBtnCheckImageList(item_id);
        return item_id;
    }

    // (Ghidra-verified) s_btnCheckImageList global — Gw.exe 06-14 build = 0x010819cc. It is NULL
    // until a Store/merchant opens (that broadcasts msg 0x05 to UiCtlBtnProc, which runs
    // FrameImageListCreate). If still null, send the styled-button item msg 0x05 ourselves to
    // populate it. Guarded on ==0 only: the proc asserts !s_btnCheckImageList, so a second
    // create would assert. NOTE: 0x010819cc is build-specific; resolve from UiCtlBtnProc's
    // msg-0x05 `mov [global],eax` store for build-independence later.
    static void EnsureBtnCheckImageList(uint32_t styled_btn_item_id)
    {
        volatile uint32_t* g = reinterpret_cast<uint32_t*>(0x010819ccu);
        if (*g == 0) {
            GW::UI::Frame* f = GW::UI::GetFrameById(styled_btn_item_id);
            if (f) GW::UI::SendFrameUIMessage(f, static_cast<GW::UI::UIMessage>(0x05), nullptr, nullptr);
        }
    }

    // Resolve a named control's FrameProc (shared by the list-item and direct-child paths).
    // Sets *is_button_family when the control uses CtlBtnProc/UiCtlBtnProc (caption via 0x5E).
    static uintptr_t ResolveNamedControlProc(const std::string& control, bool* is_button_family)
    {
        bool bf = false; uintptr_t proc = 0;
        if (control == "flat_button" || control == "checkbox" || control == "radio") { proc = ResolveCtlBtnProc(); bf = true; }
        else if (control == "text_button" || control == "hyperlink") { proc = ResolveCtlTextButtonProc(); }
        else if (control == "text_label") { proc = reinterpret_cast<uintptr_t>(ResolveTextLabelFrameCallback()); }
        else {
            const char* file = nullptr; const char* msg = nullptr;
            if (control == "styled_button") { file = "UiCtlBtn.cpp"; msg = "!s_btnCheckImageList"; bf = true; }
            else if (control == "dropdown")  { file = "UiCtlDropMenu.cpp"; msg = "!FrameGetChild(thisFrame, CTL_LIST_ENTRIES)"; }
            else if (control == "slider")    { file = "CtlSlider.cpp"; msg = "value >= m_range.min"; }
            else if (control == "edit")      { file = "UiCtlEditBox.cpp"; msg = "!s_editCaretMaterial"; }
            else if (control == "progress")  { file = "UiCtlProgress.cpp"; msg = "!sm_rateArrowImageList"; }
            else if (control == "tabs")      { file = "CtlPage.cpp"; msg = "!IsBtnCode(pageCode)"; }
            else if (control == "groupheader"){ file = "UiCtlGroupHeader.cpp"; msg = "UiCtlGroupHeader.cpp"; }
            if (file) { const uintptr_t a = GW::Scanner::FindAssertion(file, msg, 0, 0); if (a) proc = GW::Scanner::ToFunctionStart(a, 0xFFF); }
        }
        if (is_button_family) *is_button_family = bf;
        return proc;
    }

    // EXPERIMENT: create a control as a DIRECT CHILD of a parent frame (like the game's own
    // dialog buttons) rather than a full-width frame-list row — this is the path to a
    // CORRECTLY-SIZED, free-standing control. Uses ENCODED text as userData (the frame-list
    // path proved raw text crashes; the earlier bare-FrameCreate crash was likely the raw
    // text, not the parent context). Applies explicit size + position so it does not lay out
    // as a strip. Returns the child frame id, or 0.
    static uint32_t CreateControlChildByFrameId(
        uint32_t parent_frame_id,
        const std::string& control,
        const std::wstring& caption,
        uint32_t child_index = 0,
        float x = 10.0f, float y = 10.0f,
        float width = 120.0f, float height = 22.0f)
    {
        bool bf = false;
        const uintptr_t proc = ResolveNamedControlProc(control, &bf);
        if (!proc) {
            GWCA_ERR("[UI] CreateControlChildByFrameId — proc not resolved for '%s'", control.c_str());
            return 0;
        }
        std::wstring encoded = BuildStandaloneLiteralEncodedTextPayload(caption.empty() ? std::wstring(L" ") : caption);
        // Direct child: FrameCreate(parent, 0x300, child, proc, /*userData*/encoded, /*name*/L"").
        const uint32_t id = CreateUIComponentByFrameId(parent_frame_id, 0x300, child_index,
            static_cast<uintptr_t>(proc), encoded, std::wstring());
        if (!id)
            return 0;
        if (width > 0.0f && height > 0.0f)
            FrameSetSizeByFrameId(id, width, height);
        {
            using FrameSetPos_pt = void(__cdecl*)(uint32_t, Coord2f*);
            static FrameSetPos_pt pos_fn = nullptr;
            if (!pos_fn) { const auto a = ResolveFrameSetPositionCoord2f(); if (a) pos_fn = reinterpret_cast<FrameSetPos_pt>(a); }
            if (pos_fn) { Coord2f c = { x, y }; pos_fn(id, &c); }
        }
        if (bf && !caption.empty()) {
            GW::UI::Frame* f = GW::UI::GetFrameById(id);
            if (f) GW::UI::SendFrameUIMessage(f, static_cast<GW::UI::UIMessage>(0x5E),
                const_cast<wchar_t*>(caption.c_str()), nullptr);
        }
        return id;
    }

    // Create a SELECTABLE scrollable frame list as a child of the window. Identical to
    // CreateScrollableContentByFrameId, except the inner frame list uses the selectable
    // proc (CCtlFrameListSelectable::FrameProc) instead of the plain one. Clicking any item
    // then highlights it (visual cue) AND stores it as the list's selection, which you read
    // each frame with GetFrameListSelectionByFrameId. This is the native click-recognition
    // path: item click → notify 8 → SetSelection → highlight + pollable selection.
    //
    // Returns the frame list's ID, or 0 on failure.
    static uint32_t CreateSelectableScrollableContentByFrameId(
        uint32_t window_id,
        uint32_t child_index = 0,
        uint32_t component_flags = 0x20000)
    {
        const uintptr_t sel_proc = ResolveCtlFrameListSelectableProc();
        if (!sel_proc) {
            GWCA_ERR("[UI] CreateSelectableScrollableContentByFrameId — selectable proc not resolved");
            return 0;
        }
        // Mirrors the anon-namespace TypedScrollablePageContext {void*; void*; uint32_t}:
        // field_4 = the inner frame-list proc. It is consumed synchronously during creation
        // (exactly like the working plain-list default_page_context), so a stack local is safe.
        struct PageCtx { void* field_0; void* field_4; uint32_t field_8; } ctx = {};
        ctx.field_4 = reinterpret_cast<void*>(sel_proc);
        // (Ghidra-verified, "/Gw.exe (06-14)") The native selectable-list constructor
        // CCtlFrameListSelectable create @ FUN_00619b70 builds this SAME {0, sel_proc, 0}
        // page context, but calls the frame-create primitive with flags 0x20128, NOT 0x20000.
        // The extra bits 0x128 (0x100|0x20|0x08) are what make the create dispatch INIT (proc
        // msg 9) to the inner selectable proc, which allocates the per-instance selection state
        // at *(prop). Without them the state stays NULL and the FIRST selection read/write
        // (GetFrameListSelectionByFrameId msg 0x69 / SetFrameListSelectionByFrameId msg 0x6A)
        // hits the null-deref assert in FUN_00613b30 (P:\Code\...\CtlFrameList.cpp:0x3f8) →
        // hard client crash. This is the confirmed radio (crash on default-select at create)
        // and hyperlink (crash on the per-frame selection poll) failure. Force the bits on.
        const uint32_t selectable_flags = component_flags | 0x128;
        // Nest under the band 0x2710 content frame so IUi::PopCloser frees it on the native [X].
        return CreateScrollableFrameByFrameId(EnsureContentFrameForWindow(window_id), selectable_flags,
                                              child_index, reinterpret_cast<uintptr_t>(&ctx), L"");
    }

    // Read the currently-selected item of a SELECTABLE frame list (msg 0x67).
    // Returns the selected item's child code, or 0 if nothing is selected. Poll each frame;
    // a nonzero code appearing after a click identifies the clicked row.
    static uint32_t GetFrameListSelectionByFrameId(uint32_t frame_list_id)
    {
        using GetSel_pt = uint32_t(__cdecl*)(uint32_t, uint32_t*);
        static GetSel_pt fn = nullptr;
        if (!fn) {
            const uintptr_t addr = ResolveCtlFrameListSelectableGetSelection();
            if (!addr) {
                GWCA_ERR("[UI] GetFrameListSelectionByFrameId — getter not resolved");
                return 0;
            }
            fn = reinterpret_cast<GetSel_pt>(addr);
        }
        if (!frame_list_id) return 0;
        uint32_t code = 0;
        const uint32_t has_sel = fn(frame_list_id, &code);
        return has_sel ? code : 0;
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

