#include "window_title_hook.h"

#include <MinHook.h>
#include <intrin.h>
#include <mutex>
#include <vector>

namespace {
    using UiSetFrameText_pt = void(__cdecl*)(uint32_t frame, uint32_t text_resource_or_string);
    using UiCreateEncodedText_pt = uint32_t(__cdecl*)(uint32_t style_id, uint32_t layout_profile, const wchar_t* wide_text, uint32_t reserved);

    struct PendingTitleOverride {
        uint32_t parent_frame_id = 0;
        uint32_t child_index = 0;
        std::wstring title;
    };

    std::mutex g_window_title_hook_mutex;
    std::vector<PendingTitleOverride> g_pending_title_overrides;
    std::wstring g_next_created_window_title;
    std::wstring g_last_applied_title;
    uint32_t g_last_applied_frame_id = 0;
    bool g_hook_installed = false;
    bool g_expect_next_title_set = false;

    UiSetFrameText_pt UiSetFrameText_Func = nullptr;
    UiSetFrameText_pt UiSetFrameText_Ret = nullptr;
    UiCreateEncodedText_pt UiCreateEncodedText_Func = nullptr;
    UiCreateEncodedText_pt UiCreateEncodedText_Ret = nullptr;

    uintptr_t g_devtext_title_create_return = 0;
    uintptr_t g_devtext_title_set_return = 0;

    uintptr_t ResolveDevTextStringUse()
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

    uintptr_t ResolveRelativeCallTarget(uintptr_t call_addr)
    {
        const auto opcode = *reinterpret_cast<const uint8_t*>(call_addr);
        if (opcode != 0xE8)
            return 0;
        const int32_t rel = *reinterpret_cast<const int32_t*>(call_addr + 1);
        return call_addr + 5 + rel;
    }

    bool ResolveSupportFunctions()
    {
        if (!UiSetFrameText_Func) {
            const uintptr_t addr = GW::Scanner::Find(
                "\x55\x8B\xEC\x53\x56\x57\x8B\x7D\x08\x8B\xF7\xF7\xDE\x1B\xF6\x85",
                "xxxxxxxxxxxxxxxx");
            if (!addr)
                return false;
            UiSetFrameText_Func = reinterpret_cast<UiSetFrameText_pt>(addr);
        }

        if (!UiCreateEncodedText_Func) {
            const uintptr_t addr = GW::Scanner::Find(
                "\x55\x8B\xEC\x51\x56\x57\xE8\x00\x00\x00\x00\x8B\x48\x18\xE8\x00\x00\x00\x00\x8B\xF8",
                "xxxxxxx????xxxx????xx");
            if (!addr)
                return false;
            UiCreateEncodedText_Func = reinterpret_cast<UiCreateEncodedText_pt>(addr);
        }

        if (!g_devtext_title_create_return || !g_devtext_title_set_return) {
            const uintptr_t use_addr = ResolveDevTextStringUse();
            if (!use_addr)
                return false;

            for (uintptr_t addr = use_addr; addr < use_addr + 0x40; ++addr) {
                const uintptr_t target = ResolveRelativeCallTarget(addr);
                if (!target)
                    continue;

                if (!g_devtext_title_create_return && target == reinterpret_cast<uintptr_t>(UiCreateEncodedText_Func)) {
                    g_devtext_title_create_return = addr + 5;
                    continue;
                }

                if (!g_devtext_title_set_return && target == reinterpret_cast<uintptr_t>(UiSetFrameText_Func)) {
                    g_devtext_title_set_return = addr + 5;
                    continue;
                }
            }
        }

        return UiSetFrameText_Func &&
               UiCreateEncodedText_Func &&
               g_devtext_title_create_return != 0 &&
               g_devtext_title_set_return != 0;
    }

    int FindPendingOverrideIndex(uint32_t parent_frame_id, uint32_t child_index)
    {
        for (size_t i = 0; i < g_pending_title_overrides.size(); ++i) {
            const auto& pending = g_pending_title_overrides[i];
            if (pending.parent_frame_id == parent_frame_id && pending.child_index == child_index)
                return static_cast<int>(i);
        }
        return -1;
    }

    uint32_t __cdecl OnUiCreateEncodedText(uint32_t style_id, uint32_t layout_profile, const wchar_t* wide_text, uint32_t reserved)
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
            }
        }

        if (!replacement_title.empty() && UiCreateEncodedText_Ret)
            return UiCreateEncodedText_Ret(style_id, layout_profile, replacement_title.c_str(), reserved);

        if (UiCreateEncodedText_Ret)
            return UiCreateEncodedText_Ret(style_id, layout_profile, wide_text, reserved);
        return 0;
    }

    void __cdecl OnUiSetFrameText(uint32_t frame, uint32_t text_resource_or_string)
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

    bool EnsureInstalled()
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

        GW::HookBase::EnableHooks(UiCreateEncodedText_Func);
        GW::HookBase::EnableHooks(UiSetFrameText_Func);
        g_hook_installed = true;
        return true;
    }
}

namespace WindowTitleHook {
    bool SetNextCreatedWindowTitle(const std::wstring& title)
    {
        if (title.empty())
            return false;
        if (!EnsureInstalled())
            return false;

        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        g_next_created_window_title = title;
        return true;
    }

    void ClearNextCreatedWindowTitle()
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        g_next_created_window_title.clear();
        g_pending_title_overrides.clear();
        g_expect_next_title_set = false;
    }

    bool HasNextCreatedWindowTitle()
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        return !g_next_created_window_title.empty();
    }

    void ArmNextCreatedWindowTitle(uint32_t parent_frame_id, uint32_t child_index)
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        if (g_next_created_window_title.empty())
            return;
        g_pending_title_overrides.push_back({ parent_frame_id, child_index, g_next_created_window_title });
        g_next_created_window_title.clear();
    }

    void CancelArmedWindowTitle(uint32_t parent_frame_id, uint32_t child_index)
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        const int pending_index = FindPendingOverrideIndex(parent_frame_id, child_index);
        if (pending_index >= 0)
            g_pending_title_overrides.erase(g_pending_title_overrides.begin() + pending_index);
    }

    uint32_t GetLastAppliedFrameId()
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        return g_last_applied_frame_id;
    }

    std::wstring GetLastAppliedTitle()
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        return g_last_applied_title;
    }

    bool IsInstalled()
    {
        std::lock_guard<std::mutex> lock(g_window_title_hook_mutex);
        return g_hook_installed;
    }
}
