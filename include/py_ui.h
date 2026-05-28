#pragma once
#include "Headers.h"
#include <MinHook.h>
#include <intrin.h>

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

// Clone-time title overrides need to intercept the same native title path that
// DevText uses when Guild Wars builds a composite window. The game can attach:
// 1. a dynamic encoded text payload via Ui_SetFrameText, and
// 2. a resource-backed caption via Ui_SetFrameEncodedTextResource.
// Suppressing only the text path leaves the original resource caption visible.
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

        if (!UiSetFrameEncodedTextResource_Func) {
            const uintptr_t addr = GW::Scanner::Find(
                "\x55\x8B\xEC\x53\x56\x57\x8B\x7D\x08\x8B\xF7\xF7\xDE\x1B\xF6\x85\xFF\x75\x14\x68\xD2\x0B\x00\x00",
                "xxxxxxxxxxxxxxxxxxxxxxxx");
            if (!addr)
                return false;
            UiSetFrameEncodedTextResource_Func = reinterpret_cast<UiSetFrameEncodedTextResource_pt>(addr);
        }

        if (!g_devtext_title_create_return || !g_devtext_title_set_return || !g_devtext_title_resource_set_return) {
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

                if (!g_devtext_title_resource_set_return && target == reinterpret_cast<uintptr_t>(UiSetFrameEncodedTextResource_Func)) {
                    g_devtext_title_resource_set_return = addr + 5;
                    continue;
                }
            }
        }

        return UiSetFrameText_Func &&
            UiSetFrameEncodedTextResource_Func &&
            UiCreateEncodedText_Func &&
            g_devtext_title_create_return != 0 &&
            g_devtext_title_set_return != 0 &&
            g_devtext_title_resource_set_return != 0;
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

        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()))
            return false;

        const auto clear_fn = fn;
        GW::GameThread::Enqueue([frame_id, clear_fn]() {
            if (clear_fn)
                clear_fn(frame_id);
        });
        return true;
    }

    // Clears the content host of a cloned window and redraws the root.
    static bool ClearWindowContentsByFrameId(uint32_t root_frame_id)
    {
        const uint32_t host_frame_id = ResolveObservedContentHostByFrameId(root_frame_id);
        if (!host_frame_id)
            return false;
        if (!ClearFrameChildrenRecursiveByFrameId(host_frame_id))
            return false;
        TriggerFrameRedrawByFrameId(root_frame_id);
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

        ClearWindowContentsByFrameId(frame_id);
        TriggerFrameRedrawByFrameId(frame_id);
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
        using CreateEncodedText_pt = uintptr_t(__cdecl*)(uint32_t, uint32_t, const wchar_t*, uint32_t);
        using SetFrameText_pt = void(__cdecl*)(uint32_t, uintptr_t);

        // This setter only replaces the dynamic text-caption channel. Composite
        // windows can also keep a resource-caption channel, which is why clone
        // creation uses the hook path above when a full title override is needed.
        static CreateEncodedText_pt create_text_fn = nullptr;
        static SetFrameText_pt set_frame_text_fn = nullptr;
        if (!create_text_fn) {
            const auto addr = GW::Scanner::Find(
                "\x55\x8B\xEC\x8B\x45\x10\x53\x56\x57\x8B\x7D\x0C\x85\xFF\x74\x00",
                "xxxxxxxxxxxxxxx?");
            if (!addr)
                return false;
            create_text_fn = reinterpret_cast<CreateEncodedText_pt>(addr);
        }
        if (!set_frame_text_fn) {
            const auto addr = GW::Scanner::Find(
                "\x55\x8B\xEC\x56\x8B\x75\x08\x85\xF6\x74\x00\xFF\x76\x24",
                "xxxxxxxxxx?xxx");
            if (!addr)
                return false;
            set_frame_text_fn = reinterpret_cast<SetFrameText_pt>(addr);
        }

        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated()) || title.empty())
            return false;

        const uint32_t target_frame_id = frame_id;
        const std::wstring requested_title = title;
        const auto create_fn = create_text_fn;
        const auto set_fn = set_frame_text_fn;
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
        auto* frame = reinterpret_cast<GW::SliderFrame*>(GW::UI::GetFrameById(frame_id));
        return frame ? frame->GetValue() : 0;
    }

    static bool SetSliderValueByFrameId(uint32_t frame_id, uint32_t value) {
        auto* frame = reinterpret_cast<GW::SliderFrame*>(GW::UI::GetFrameById(frame_id));
        return frame && frame->SetValue(value);
    }

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

};





