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
    uint32_t field25_0x64;
    uint32_t field26_0x68;
    uint32_t field27_0x6c;
    uint32_t field28_0x70;
    uint32_t field29_0x74;
    uint32_t field30_0x78;
    std::vector<uintptr_t> field31_0x7c;
    uint32_t field32_0x8c;
    uint32_t field33_0x90;
    uint32_t field34_0x94;
    uint32_t field35_0x98;
    uint32_t field36_0x9c;
    std::vector<UIInteractionCallbackWrapper> frame_callbacks;
    uint32_t child_offset_id;
    uint32_t field40_0xb8;
    uint32_t field41_0xbc;
    uint32_t field42_0xc0;
    uint32_t field43_0xc4;
    uint32_t field44_0xc8;
    uint32_t field45_0xcc;
	FramePositionWrapper position;
    uint32_t field63_0x114;
    uint32_t field64_0x118;
    uint32_t field65_0x11c;
    FrameRelationWrapper relation;
    uint32_t field73_0x13c;
    uint32_t field74_0x140;
    uint32_t field75_0x144;
    uint32_t field76_0x148;
    uint32_t field77_0x14c;
    uint32_t field78_0x150;
    uint32_t field79_0x154;
    uint32_t field80_0x158;
    uint32_t field81_0x15c;
    uint32_t field82_0x160;
    uint32_t field83_0x164;
    uint32_t field84_0x168;
    uint32_t field85_0x16c;
    uint32_t field86_0x170;
    uint32_t field87_0x174;
    uint32_t field88_0x178;
    uint32_t field89_0x17c;
    uint32_t field90_0x180;
    uint32_t field91_0x184;
    uint32_t field92_0x188;
    uint32_t field93_0x18c;
    uint32_t field94_0x190;
    uint32_t field95_0x194;
    uint32_t field96_0x198;
    uint32_t field97_0x19c;
    uint32_t field98_0x1a0;
    //TooltipInfo* tooltip_info;
	uint32_t field100_0x1a8;

    UIFrame(int pframe_id) {
		frame_id = pframe_id;
        GetContext();
    }
	void GetContext();
};

class UIManager {
public:


    static uint32_t GetFrameIDByLabel(const std::string& label) {
        std::wstring wlabel(label.begin(), label.end()); // Convert to wide string
        return GW::UI::GetFrameIDByLabel(wlabel.c_str());
    }

    static uint32_t GetFrameIDByHash(uint32_t hash) {
        return GW::UI::GetFrameIDByHash(hash);
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

	static void ButtonClick(uint32_t frame_id) {
        GW::GameThread::Enqueue([frame_id]() {
            GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
            GW::UI::ButtonClick(frame);
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

    static void SetPreference(uint32_t pref, uint32_t value) {
        GW::GameThread::Enqueue([pref, value]() {
            GW::UI::EnumPreference pref_enum = static_cast<GW::UI::EnumPreference>(pref);
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

	static std::vector <uint32_t> GetFrameArray() {
		return GW::UI::GetFrameArray();
	}
};


PYBIND11_EMBEDDED_MODULE(PyUIManager, m) {
	py::class_<UIInteractionCallbackWrapper>(m, "UIInteractionCallback")
		.def(py::init<GW::UI::UIInteractionCallback>())
		.def("get_address", &UIInteractionCallbackWrapper::get_address);

    py::class_<FramePositionWrapper>(m, "FramePosition")
        .def(py::init<>())
        .def_readwrite("top", &FramePositionWrapper::top)
        .def_readwrite("left", &FramePositionWrapper::left)
        .def_readwrite("bottom", &FramePositionWrapper::bottom)
        .def_readwrite("right", &FramePositionWrapper::right)
        .def_readwrite("content_top", &FramePositionWrapper::content_top)
        .def_readwrite("content_left", &FramePositionWrapper::content_left)
        .def_readwrite("content_bottom", &FramePositionWrapper::content_bottom)
        .def_readwrite("content_right", &FramePositionWrapper::content_right)
        .def_readwrite("unknown", &FramePositionWrapper::unknown)
        .def_readwrite("scale_factor", &FramePositionWrapper::scale_factor)
        .def_readwrite("viewport_width", &FramePositionWrapper::viewport_width)
        .def_readwrite("viewport_height", &FramePositionWrapper::viewport_height)
        .def_readwrite("screen_top", &FramePositionWrapper::screen_top)
        .def_readwrite("screen_left", &FramePositionWrapper::screen_left)
        .def_readwrite("screen_bottom", &FramePositionWrapper::screen_bottom)
        .def_readwrite("screen_right", &FramePositionWrapper::screen_right)
        .def_readwrite("top_on_screen", &FramePositionWrapper::top_on_screen)
        .def_readwrite("left_on_screen", &FramePositionWrapper::left_on_screen)
        .def_readwrite("bottom_on_screen", &FramePositionWrapper::bottom_on_screen)
        .def_readwrite("right_on_screen", &FramePositionWrapper::right_on_screen)
        .def_readwrite("width_on_screen", &FramePositionWrapper::width_on_screen)
        .def_readwrite("height_on_screen", &FramePositionWrapper::height_on_screen)
        .def_readwrite("viewport_scale_x", &FramePositionWrapper::viewport_scale_x)
        .def_readwrite("viewport_scale_y", &FramePositionWrapper::viewport_scale_y);

    py::class_<FrameRelationWrapper>(m, "FrameRelation")
        .def(py::init<>())
        .def_readwrite("parent_id", &FrameRelationWrapper::parent_id)
        .def_readwrite("field67_0x124", &FrameRelationWrapper::field67_0x124)
        .def_readwrite("field68_0x128", &FrameRelationWrapper::field68_0x128)
        .def_readwrite("frame_hash_id", &FrameRelationWrapper::frame_hash_id)
        .def_readwrite("siblings", &FrameRelationWrapper::siblings);

    py::class_<UIFrame>(m, "UIFrame")
        .def(py::init<int>())
        .def_readwrite("frame_id", &UIFrame::frame_id)
        .def_readwrite("parent_id", &UIFrame::parent_id)
        .def_readwrite("frame_hash", &UIFrame::frame_hash)
        .def_readwrite("visibility_flags", &UIFrame::visibility_flags)
        .def_readwrite("type", &UIFrame::type)
        .def_readwrite("template_type", &UIFrame::template_type)
        .def_readwrite("position", &UIFrame::position)
        .def_readwrite("relation", &UIFrame::relation)
		.def_readwrite("is_created", &UIFrame::is_created)
		.def_readwrite("is_visible", &UIFrame::is_visible)

        // Binding all fields explicitly
        .def_readwrite("field1_0x0", &UIFrame::field1_0x0)
        .def_readwrite("field2_0x4", &UIFrame::field2_0x4)
        .def_readwrite("frame_layout", &UIFrame::frame_layout)
        .def_readwrite("field3_0xc", &UIFrame::field3_0xc)
        .def_readwrite("field4_0x10", &UIFrame::field4_0x10)
        .def_readwrite("field5_0x14", &UIFrame::field5_0x14)
        .def_readwrite("field7_0x1c", &UIFrame::field7_0x1c)
        .def_readwrite("field10_0x28", &UIFrame::field10_0x28)
        .def_readwrite("field11_0x2c", &UIFrame::field11_0x2c)
        .def_readwrite("field12_0x30", &UIFrame::field12_0x30)
        .def_readwrite("field13_0x34", &UIFrame::field13_0x34)
        .def_readwrite("field14_0x38", &UIFrame::field14_0x38)
        .def_readwrite("field15_0x3c", &UIFrame::field15_0x3c)
        .def_readwrite("field16_0x40", &UIFrame::field16_0x40)
        .def_readwrite("field17_0x44", &UIFrame::field17_0x44)
        .def_readwrite("field18_0x48", &UIFrame::field18_0x48)
        .def_readwrite("field19_0x4c", &UIFrame::field19_0x4c)
        .def_readwrite("field20_0x50", &UIFrame::field20_0x50)
        .def_readwrite("field21_0x54", &UIFrame::field21_0x54)
        .def_readwrite("field22_0x58", &UIFrame::field22_0x58)
        .def_readwrite("field23_0x5c", &UIFrame::field23_0x5c)
        .def_readwrite("field24_0x60", &UIFrame::field24_0x60)
        .def_readwrite("field25_0x64", &UIFrame::field25_0x64)
        .def_readwrite("field26_0x68", &UIFrame::field26_0x68)
        .def_readwrite("field27_0x6c", &UIFrame::field27_0x6c)
        .def_readwrite("field28_0x70", &UIFrame::field28_0x70)
        .def_readwrite("field29_0x74", &UIFrame::field29_0x74)
        .def_readwrite("field30_0x78", &UIFrame::field30_0x78)
        .def_readwrite("field31_0x7c", &UIFrame::field31_0x7c)
        .def_readwrite("field32_0x8c", &UIFrame::field32_0x8c)
        .def_readwrite("field33_0x90", &UIFrame::field33_0x90)
        .def_readwrite("field34_0x94", &UIFrame::field34_0x94)
        .def_readwrite("field35_0x98", &UIFrame::field35_0x98)
        .def_readwrite("field36_0x9c", &UIFrame::field36_0x9c)
		.def_readwrite("frame_callbacks", &UIFrame::frame_callbacks)
		.def_readwrite("child_offset_id", &UIFrame::child_offset_id)
        .def_readwrite("field40_0xb8", &UIFrame::field40_0xb8)
        .def_readwrite("field41_0xbc", &UIFrame::field41_0xbc)
        .def_readwrite("field42_0xc0", &UIFrame::field42_0xc0)
        .def_readwrite("field43_0xc4", &UIFrame::field43_0xc4)
        .def_readwrite("field44_0xc8", &UIFrame::field44_0xc8)
        .def_readwrite("field45_0xcc", &UIFrame::field45_0xcc)
		.def_readonly("position", &UIFrame::position)
        .def_readwrite("field63_0x114", &UIFrame::field63_0x114)
        .def_readwrite("field64_0x118", &UIFrame::field64_0x118)
        .def_readwrite("field65_0x11c", &UIFrame::field65_0x11c)
		.def_readwrite("relation", &UIFrame::relation)
        .def_readwrite("field73_0x13c", &UIFrame::field73_0x13c)
        .def_readwrite("field74_0x140", &UIFrame::field74_0x140)
        .def_readwrite("field75_0x144", &UIFrame::field75_0x144)
        .def_readwrite("field76_0x148", &UIFrame::field76_0x148)
        .def_readwrite("field77_0x14c", &UIFrame::field77_0x14c)
        .def_readwrite("field78_0x150", &UIFrame::field78_0x150)
        .def_readwrite("field79_0x154", &UIFrame::field79_0x154)
        .def_readwrite("field80_0x158", &UIFrame::field80_0x158)
        .def_readwrite("field81_0x15c", &UIFrame::field81_0x15c)
        .def_readwrite("field82_0x160", &UIFrame::field82_0x160)
        .def_readwrite("field83_0x164", &UIFrame::field83_0x164)
        .def_readwrite("field84_0x168", &UIFrame::field84_0x168)
        .def_readwrite("field85_0x16c", &UIFrame::field85_0x16c)
        .def_readwrite("field86_0x170", &UIFrame::field86_0x170)
        .def_readwrite("field87_0x174", &UIFrame::field87_0x174)
        .def_readwrite("field88_0x178", &UIFrame::field88_0x178)
        .def_readwrite("field89_0x17c", &UIFrame::field89_0x17c)
        .def_readwrite("field90_0x180", &UIFrame::field90_0x180)
        .def_readwrite("field91_0x184", &UIFrame::field91_0x184)
        .def_readwrite("field92_0x188", &UIFrame::field92_0x188)
        .def_readwrite("field93_0x18c", &UIFrame::field93_0x18c)
        .def_readwrite("field94_0x190", &UIFrame::field94_0x190)
        .def_readwrite("field95_0x194", &UIFrame::field95_0x194)
        .def_readwrite("field96_0x198", &UIFrame::field96_0x198)
        .def_readwrite("field97_0x19c", &UIFrame::field97_0x19c)
        .def_readwrite("field98_0x1a0", &UIFrame::field98_0x1a0)
        .def_readwrite("field100_0x1a8", &UIFrame::field100_0x1a8)

        // Methods
        .def("get_context", &UIFrame::GetContext);


    py::class_<UIManager>(m, "UIManager")
        .def_static("get_frame_id_by_label", &UIManager::GetFrameIDByLabel, py::arg("label"), "Gets the frame ID associated with a given label.")
        .def_static("get_frame_id_by_hash", &UIManager::GetFrameIDByHash, py::arg("hash"), "Gets the frame ID using its hash.")
        .def_static("get_hash_by_label", &UIManager::GetHashByLabel, py::arg("label"), "Gets the hash of a frame label.")
        .def_static("get_frame_hierarchy", &UIManager::GetFrameHierarchy, "Retrieves the hierarchy of frames as a list of tuples (parent, child, etc.).")
        .def_static("get_frame_coords_by_hash", &UIManager::GetFrameCoordsByHash, py::arg("frame_hash"), "Gets the coordinates of a frame using its hash.")
		.def_static("button_click", &UIManager::ButtonClick, py::arg("frame_id"), "Simulates a button click on a frame.")
		.def_static("get_root_frame_id", &UIManager::GetRootFrameID, "Gets the ID of the root frame.")
		.def_static("is_world_map_showing", &UIManager::IsWorldMapShowing, "Checks if the world map is currently showing.")
		.def_static("set_preference", &UIManager::SetPreference, py::arg("pref"), py::arg("value"), "Sets a UI preference.")
		.def_static("get_frame_limit", &UIManager::GetFrameLimit, "Gets the frame limit.")
		.def_static("set_frame_limit", &UIManager::SetFrameLimit, py::arg("value"), "Sets the frame limit.")
		.def_static("get_frame_array", &UIManager::GetFrameArray, "Gets the frame array.")
		.def_static("get_child_frame_id", &UIManager::GetChildFrameID, py::arg("parent_hash"), py::arg("child_offsets"), "Gets the ID of a child frame using its parent hash and child offsets.");

}
