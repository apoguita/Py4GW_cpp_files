#pragma once
#include "Headers.h"

namespace py = pybind11;

template <typename T>
std::vector<T> ConvertArrayToVector(const GW::Array<T>& arr) {
    return std::vector<T>(arr.begin(), arr.end());
}


// Wrapper for function pointers
struct UIInteractionCallbackWrapper {
    uintptr_t callback_address;  // Store function pointer as an integer

    UIInteractionCallbackWrapper(GW::UI::UIInteractionCallback callback)
        : callback_address(reinterpret_cast<uintptr_t>(callback)) {
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
	void GetContext();
};

class UIManager {
public:
    static uint32_t GetTextLanguage() {
        return static_cast<uint32_t>(GW::UI::GetTextLanguage());
    }

	static std::vector<std::tuple<uint64_t, uint32_t, std::string>> GetFrameLogs() {
		return GW::UI::GetFrameLogs();
	}

	static void ClearFrameLogs() {
		GW::UI::ClearFrameLogs();
	}

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

	static void ClearUIPayloads() {
		GW::UI::ClearUIPayloads();
	}
	

    static uint32_t GetFrameIDByLabel(const std::string& label) {
        std::wstring wlabel(label.begin(), label.end()); // Convert to wide string
        return GW::UI::GetFrameIDByLabel(wlabel.c_str());
    }

    static uint32_t GetFrameIDByHash(uint32_t hash) {
        return GW::UI::GetFrameIDByHash(hash);
    }

    static uint32_t GetChildFrameByFrameId(uint32_t parent_frame_id, uint32_t child_offset) {
        GW::UI::Frame* parent = GW::UI::GetFrameById(parent_frame_id);
        if (!parent)
            return 0;
        GW::UI::Frame* child = GW::UI::GetChildFrame(parent, child_offset);
        return child ? child->frame_id : 0;
    }

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

    static uint32_t GetParentFrameID(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame)
            return 0;
        GW::UI::Frame* parent = GW::UI::GetParentFrame(frame);
        return parent ? parent->frame_id : 0;
    }

    static uint32_t GetHashByLabel(const std::string& label) {
        return GW::UI::GetHashByLabel(label);
    }

    static std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> GetFrameHierarchy() {
        return GW::UI::GetFrameHierarchy();
    }

    static std::vector<std::pair<uint32_t, uint32_t>> GetFrameCoordsByHash(uint32_t frame_hash) {
        return GW::UI::GetFrameCoordsByHash(frame_hash);
    }

	static uint32_t GetChildFrameID(uint32_t parent_hash, std::vector<uint32_t> child_offsets) {
		return GW::UI::GetChildFrameID(parent_hash, child_offsets);
	}

    // pybind11 signature: SendUIMessagePacked(msgid, layout, values, skip_hooks=false)
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

    static bool SendFrameUIMessage(uint32_t frame_id, uint32_t message_id,
                                   uintptr_t wparam, uintptr_t lparam = 0) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame) return false;
        return GW::UI::SendFrameUIMessage(
            frame, static_cast<GW::UI::UIMessage>(message_id),
            reinterpret_cast<void*>(wparam), reinterpret_cast<void*>(lparam));
    }

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

    static void RestoreDevTextSource(bool opened_temporarily)
    {
        if (!opened_temporarily)
            return;

        const uint32_t frame_id = GetFrameIDByLabel("DevText");
        auto* frame = frame_id ? GW::UI::GetFrameById(frame_id) : nullptr;
        if (frame && frame->IsCreated())
            KeyPress(0x25, 0);
    }

    static uint32_t ResolveObservedContentHostByFrameId(uint32_t root_frame_id)
    {
        if (!root_frame_id)
            return 0;
        return GetChildFramePathByFrameId(root_frame_id, { 0, 0, 0 });
    }

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
        RestoreDevTextSource(opened_temporarily);
        return frame_id;
    }

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


    static bool DestroyUIComponentByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame)
            return false;
        return GW::UI::DestroyUIComponent(frame);
    }

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

    static bool TriggerFrameRedrawByFrameId(uint32_t frame_id) {
        GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame)
            return false;
        return GW::UI::TriggerFrameRedraw(frame);
    }

    static uint32_t CreateButtonFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index = 0,
        const std::wstring& name_enc = L"",
        const std::wstring& component_label = L"")
    {
        GW::UI::Frame* parent = GW::UI::GetFrameById(parent_frame_id);
        if (!(parent && parent->IsCreated()))
            return 0;
        auto* frame = GW::UI::CreateButtonFrame(
            parent,
            component_flags,
            child_index,
            name_enc.empty() ? nullptr : const_cast<wchar_t*>(name_enc.c_str()),
            component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str()));
        return frame ? frame->frame_id : 0;
    }

    static uint32_t CreateCheckboxFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index = 0,
        const std::wstring& name_enc = L"",
        const std::wstring& component_label = L"")
    {
        GW::UI::Frame* parent = GW::UI::GetFrameById(parent_frame_id);
        if (!(parent && parent->IsCreated()))
            return 0;
        auto* frame = GW::UI::CreateCheckboxFrame(
            parent,
            component_flags,
            child_index,
            name_enc.empty() ? nullptr : const_cast<wchar_t*>(name_enc.c_str()),
            component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str()));
        return frame ? frame->frame_id : 0;
    }

    static uint32_t CreateScrollableFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index = 0,
        uintptr_t page_context = 0,
        const std::wstring& component_label = L"")
    {
        GW::UI::Frame* parent = GW::UI::GetFrameById(parent_frame_id);
        if (!(parent && parent->IsCreated()))
            return 0;
        auto* frame = GW::UI::CreateScrollableFrame(
            parent,
            component_flags,
            child_index,
            reinterpret_cast<void*>(page_context),
            component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str()));
        return frame ? frame->frame_id : 0;
    }

    static uint32_t CreateTextLabelFrameByFrameId(
        uint32_t parent_frame_id,
        uint32_t component_flags,
        uint32_t child_index = 0,
        const std::wstring& name_enc = L"",
        const std::wstring& component_label = L"")
    {
        GW::UI::Frame* parent = GW::UI::GetFrameById(parent_frame_id);
        if (!(parent && parent->IsCreated()))
            return 0;
        auto* frame = GW::UI::CreateTextLabelFrame(
            parent,
            component_flags,
            child_index,
            name_enc.empty() ? nullptr : const_cast<wchar_t*>(name_enc.c_str()),
            component_label.empty() ? nullptr : const_cast<wchar_t*>(component_label.c_str()));
        return frame ? frame->frame_id : 0;
    }




	static void ButtonClick(uint32_t frame_id) {
        GW::GameThread::Enqueue([frame_id]() {
            GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
            GW::UI::ButtonClick(frame);
            });
        
	}

    static void TestMouseAction(uint32_t frame_id, uint32_t current_state, uint32_t wparam_value = 0, uint32_t lparam=0) {
        GW::GameThread::Enqueue([frame_id, current_state, wparam_value, lparam]() {
			GW::UI::TestMouseAction(frame_id, current_state, wparam_value, lparam);
            });

    }

    static void TestMouseClickAction(uint32_t frame_id, uint32_t current_state, uint32_t wparam_value = 0, uint32_t lparam = 0) {
        GW::GameThread::Enqueue([frame_id, current_state, wparam_value, lparam]() {
            GW::UI::TestMouseClickAction(frame_id, current_state, wparam_value, lparam);
            });

    }

	static uint32_t GetRootFrameID() {
		GW::UI::Frame* frame = GW::UI::GetRootFrame();
		if (!frame) {
			return 0;
		}
		return frame->frame_id;
	}

	static bool IsWorldMapShowing() {
		return GW::UI::GetIsWorldMapShowing();
	}

    static bool IsUIDrawn() {
        return GW::UI::GetIsUIDrawn();
    }

    static std::string AsyncDecodeStr(const std::string& enc_str) {
        std::wstring winput(enc_str.begin(), enc_str.end());
        std::wstring output;
        GW::UI::AsyncDecodeStr(winput.c_str(), &output);
        return std::string(output.begin(), output.end());
    }

    static bool IsValidEncStr(const std::string& enc_str) {
        std::wstring winput(enc_str.begin(), enc_str.end());
        return GW::UI::IsValidEncStr(winput.c_str());
    }

    static std::string UInt32ToEncStr(uint32_t value) {
        wchar_t buffer[8] = {0};
        if (!GW::UI::UInt32ToEncStr(value, buffer, _countof(buffer))) {
            return "";
        }
        std::wstring woutput(buffer);
        return std::string(woutput.begin(), woutput.end());
    }

    static uint32_t EncStrToUInt32(const std::string& enc_str) {
        std::wstring winput(enc_str.begin(), enc_str.end());
        return GW::UI::EncStrToUInt32(winput.c_str());
    }

    static void SetOpenLinks(bool toggle) {
        GW::GameThread::Enqueue([toggle]() {
            GW::UI::SetOpenLinks(toggle);
        });
    }

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

    static uintptr_t GetCurrentTooltipAddress() {
        return reinterpret_cast<uintptr_t>(GW::UI::GetCurrentTooltip());
    }

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



	static uint32_t GetEnumPreference(uint32_t pref) {
		GW::UI::EnumPreference pref_enum = static_cast<GW::UI::EnumPreference>(pref);
		return GW::UI::GetPreference(pref_enum);
	}

	static uint32_t GetIntPreference(uint32_t pref) {
		GW::UI::NumberPreference pref_enum = static_cast<GW::UI::NumberPreference>(pref);
		return GW::UI::GetPreference(pref_enum);
	}

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

	static bool GetBoolPreference(uint32_t pref) {
		GW::UI::FlagPreference pref_enum = static_cast<GW::UI::FlagPreference>(pref);
		return GW::UI::GetPreference(pref_enum);
	}

    static void SetEnumPreference(uint32_t pref, uint32_t value) {
        GW::GameThread::Enqueue([pref, value]() {
            GW::UI::EnumPreference pref_enum = static_cast<GW::UI::EnumPreference>(pref);
            GW::UI::SetPreference(pref_enum, value);
            });	
	}

	static void SetIntPreference(uint32_t pref, uint32_t value) {
		GW::GameThread::Enqueue([pref, value]() {
			GW::UI::NumberPreference pref_enum = static_cast<GW::UI::NumberPreference>(pref);
			GW::UI::SetPreference(pref_enum, value);
			});
	}

	static void SetStringPreference(uint32_t pref, const std::string& value) {
		GW::GameThread::Enqueue([pref, value]() {
			GW::UI::StringPreference pref_enum = static_cast<GW::UI::StringPreference>(pref);
			std::wstring wstr(value.begin(), value.end());
			wchar_t* wstr_ptr = const_cast<wchar_t*>(wstr.c_str());
			GW::UI::SetPreference(pref_enum, wstr_ptr);
			});
	}

	static void SetBoolPreference(uint32_t pref, bool value) {
		GW::GameThread::Enqueue([pref, value]() {
			GW::UI::FlagPreference pref_enum = static_cast<GW::UI::FlagPreference>(pref);
			GW::UI::SetPreference(pref_enum, value);
			});
	}

	static uint32_t GetFrameLimit() {
		return GW::UI::GetFrameLimit();
	}

    static void SetFrameLimit(uint32_t value) {
        GW::GameThread::Enqueue([value]() {
            GW::UI::SetFrameLimit(value);
            });

	}

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

	static bool IsWindowVisible(uint32_t window_id) {
        GW::UI::WindowPosition* position = GW::UI::GetWindowPosition(static_cast<GW::UI::WindowID>(window_id));
		if (!position) {
			return false;
		}
        return (position->state & 0x1) != 0;
	}

	static void SetWindowVisible(uint32_t window_id, bool is_visible) {
		GW::GameThread::Enqueue([window_id, is_visible]() {
			GW::UI::SetWindowVisible(static_cast<GW::UI::WindowID>(window_id), is_visible);
			});
	}

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

	static bool IsShiftScreenShot() {
		return GW::UI::GetIsShiftScreenShot();
	}

};

