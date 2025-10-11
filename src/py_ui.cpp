#pragma once
#include "py_ui.h"

std::vector<UIInteractionCallbackWrapper> ConvertUIInteractionCallbacks(const GW::Array<GW::UI::UIInteractionCallback>& arr) {
	std::vector<UIInteractionCallbackWrapper> result;
	result.reserve(arr.size());

	for (GW::UI::UIInteractionCallback callback : arr) {
		result.emplace_back(callback);
	}

	return result;
}

std::vector<uintptr_t> FillVectorFromPointerArray(const GW::Array<void*>& arr) {
	std::vector<uintptr_t> vec;
	vec.reserve(arr.size());  // Optimize memory allocation

	for (void* ptr : arr) {
		vec.push_back(reinterpret_cast<uintptr_t>(ptr));
	}

	return vec;  // Returning by value (RVO will optimize this)
}


std::vector<uint32_t> GetSiblingFrameIDs(uint32_t frame_id) {
	std::vector<uint32_t> sibling_ids;

	GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);

	if (!frame) {
		return sibling_ids;
	}

	const GW::UI::FrameRelation* relation = &frame->relation;

	if (!relation)
		return sibling_ids;

	auto it = relation->siblings.begin();

	for (; it != relation->siblings.end(); ++it) {
		GW::UI::FrameRelation& sibling = *it;
		GW::UI::Frame* frame = sibling.GetFrame();
		if (frame) {
			sibling_ids.push_back(frame->frame_id);
		}
	}

	return sibling_ids;
}



void UIFrame::GetContext() {

	GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);

	if (!frame) {
		return;
	}
	
    GW::UI::Frame* parent = frame->relation.GetParent();
	parent_id = parent ? parent->frame_id : 0;
	frame_hash = frame->relation.frame_hash_id;
	

	frame_layout = frame->frame_layout;
	visibility_flags = frame->visibility_flags;
	type = frame->type;
	template_type = frame->template_type;
	frame_callbacks = ConvertUIInteractionCallbacks(frame->frame_callbacks);
	child_offset_id = frame->child_offset_id;

	field1_0x0 = frame->field1_0x0;
	field2_0x4 = frame->field2_0x4;	
	field3_0xc = frame->field3_0xc;
	field4_0x10 = frame->field4_0x10;
	field5_0x14 = frame->field5_0x14;
	field7_0x1c = frame->field7_0x1c;
	field10_0x28 = frame->field10_0x28;
	field11_0x2c = frame->field11_0x2c;
	field12_0x30 = frame->field12_0x30;
	field13_0x34 = frame->field13_0x34;
	field14_0x38 = frame->field14_0x38;
	field15_0x3c = frame->field15_0x3c;
	field16_0x40 = frame->field16_0x40;
	field17_0x44 = frame->field17_0x44;
	field18_0x48 = frame->field18_0x48;
	field19_0x4c = frame->field19_0x4c;
	field20_0x50 = frame->field20_0x50;
	field21_0x54 = frame->field21_0x54;
	field22_0x58 = frame->field22_0x58;
	field23_0x5c = frame->field23_0x5c;
	field24_0x60 = frame->field24_0x60;
	field25_0x64 = frame->field25_0x64;
	field26_0x68 = frame->field26_0x68;
	field27_0x6c = frame->field27_0x6c;
	field28_0x70 = frame->field28_0x70;
	field29_0x74 = frame->field29_0x74;
	field30_0x78 = frame->field30_0x78;
	field31_0x7c = FillVectorFromPointerArray(frame->field31_0x7c);
	field32_0x8c = frame->field32_0x8c;
	field33_0x90 = frame->field33_0x90;
	field34_0x94 = frame->field34_0x94;
	field35_0x98 = frame->field35_0x98;
	field36_0x9c = frame->field36_0x9c;
	field40_0xb8 = frame->field40_0xb8;
	field41_0xbc = frame->field41_0xbc;
	field42_0xc0 = frame->field42_0xc0;
	field43_0xc4 = frame->field43_0xc4;
	field44_0xc8 = frame->field44_0xc8;
	field45_0xcc = frame->field45_0xcc;
	position.top = frame->position.top;
	position.bottom = frame->position.bottom;
	position.left = frame->position.left;
	position.right = frame->position.right;

	position.content_top = frame->position.content_top;
	position.content_bottom = frame->position.content_bottom;
	position.content_left = frame->position.content_left;
	position.content_right = frame->position.content_right;

	position.unknown = frame->position.unk;
	position.scale_factor = frame->position.scale_factor;
	position.viewport_width = frame->position.viewport_width;
	position.viewport_height = frame->position.viewport_height;

	position.screen_top = frame->position.screen_top;
	position.screen_bottom = frame->position.screen_bottom;
	position.screen_left = frame->position.screen_left;
	position.screen_right = frame->position.screen_right;

	const auto root = GW::UI::GetRootFrame();
	if (!root) {
		return;
	}

	const auto top_left = frame->position.GetTopLeftOnScreen(root);
	const auto bottom_right = frame->position.GetBottomRightOnScreen(root);

	position.top_on_screen = top_left.y;
	position.left_on_screen = top_left.x;
	position.bottom_on_screen = bottom_right.y;
	position.right_on_screen = bottom_right.x;

	position.width_on_screen = frame->position.GetSizeOnScreen(root).x;
	position.height_on_screen = frame->position.GetSizeOnScreen(root).y;

	position.viewport_scale_x = frame->position.GetViewportScale(root).x;
	position.viewport_scale_y = frame->position.GetViewportScale(root).y;

	field63_0x114 = frame->field63_0x114;
	field64_0x118 = frame->field64_0x118;
	field65_0x11c = frame->field65_0x11c;

	relation.parent_id = parent ? parent->frame_id : 0;
	relation.field67_0x124 = frame->relation.field67_0x124;
	relation.field68_0x128 = frame->relation.field68_0x128;
	relation.frame_hash_id = frame->relation.frame_hash_id;
	relation.siblings = GetSiblingFrameIDs(frame->frame_id);

	field73_0x13c = frame->field73_0x13c;
	field74_0x140 = frame->field74_0x140;
	field75_0x144 = frame->field75_0x144;
	field76_0x148 = frame->field76_0x148;
	field77_0x14c = frame->field77_0x14c;
	field78_0x150 = frame->field78_0x150;
	field79_0x154 = frame->field79_0x154;
	field80_0x158 = frame->field80_0x158;
	field81_0x15c = frame->field81_0x15c;
	field82_0x160 = frame->field82_0x160;
	field83_0x164 = frame->field83_0x164;
	field84_0x168 = frame->field84_0x168;
	field85_0x16c = frame->field85_0x16c;
	field86_0x170 = frame->field86_0x170;
	field87_0x174 = frame->field87_0x174;
	field88_0x178 = frame->field88_0x178;
	field89_0x17c = frame->field89_0x17c;
	field90_0x180 = frame->field90_0x180;
	field91_0x184 = frame->field91_0x184;
	field92_0x188 = frame->field92_0x188;
	field93_0x18c = frame->field93_0x18c;
	field94_0x190 = frame->field94_0x190;
	field95_0x194 = frame->field95_0x194;
	field96_0x198 = frame->field96_0x198;
	field97_0x19c = frame->field97_0x19c;
	field98_0x1a0 = frame->field98_0x1a0;
	//TooltipInfo* tooltip_info;
	field100_0x1a8 = frame->field100_0x1a8;

	is_visible = frame->IsVisible();
	is_created = frame->IsCreated();

}



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
		.def_static("get_preference_options", &UIManager::GetPreferenceOptions, py::arg("pref"), "Gets the available options for a given preference.")
		.def_static("get_enum_preference", &UIManager::GetEnumPreference, py::arg("pref"), "Gets the value of an enum preference.")
		.def_static("get_int_preference", &UIManager::GetIntPreference, py::arg("pref"), "Gets the value of an integer preference.")
		.def_static("get_string_preference", &UIManager::GetStringPreference, py::arg("pref"), "Gets the value of a string preference.")
		.def_static("get_bool_preference", &UIManager::GetBoolPreference, py::arg("pref"), "Gets the value of a boolean preference.")
		.def_static("set_enum_preference", &UIManager::SetEnumPreference, py::arg("pref"), py::arg("value"), "Sets the value of an enum preference.")
		.def_static("set_int_preference", &UIManager::SetIntPreference, py::arg("pref"), py::arg("value"), "Sets the value of an integer preference.")
		.def_static("set_string_preference", &UIManager::SetStringPreference, py::arg("pref"), py::arg("value"), "Sets the value of a string preference.")
		.def_static("set_bool_preference", &UIManager::SetBoolPreference, py::arg("pref"), py::arg("value"), "Sets the value of a boolean preference.")

		.def_static("button_click", &UIManager::ButtonClick, py::arg("frame_id"), "Simulates a button click on a frame.")
		.def_static("test_mouse_action", &UIManager::TestMouseAction, py::arg("frame_id"), py::arg("current_state"), py::arg("wparam_value") = 0, py::arg("lparam") = 0, "Simulates a mouse action on a frame.")
		.def_static("test_mouse_click_action", &UIManager::TestMouseClickAction, py::arg("frame_id"), py::arg("current_state"), py::arg("wparam_value") = 0, py::arg("lparam") = 0, "Simulates a mouse action on a frame.")
		.def_static("get_root_frame_id", &UIManager::GetRootFrameID, "Gets the ID of the root frame.")
		.def_static("is_world_map_showing", &UIManager::IsWorldMapShowing, "Checks if the world map is currently showing.")
		.def_static("get_frame_limit", &UIManager::GetFrameLimit, "Gets the frame limit.")
		.def_static("set_frame_limit", &UIManager::SetFrameLimit, py::arg("value"), "Sets the frame limit.")
		.def_static("get_frame_array", &UIManager::GetFrameArray, "Gets the frame array.")
		.def_static("get_child_frame_id", &UIManager::GetChildFrameID, py::arg("parent_hash"), py::arg("child_offsets"), "Gets the ID of a child frame using its parent hash and child offsets.")
		.def_static("key_down", &UIManager::KeyDown, py::arg("key"), py::arg("frame_id"), "Simulates a key down event on a frame.")
		.def_static("key_up", &UIManager::KeyUp, py::arg("key"), py::arg("frame_id"), "Simulates a key up event on a frame.")
		.def_static("key_press", &UIManager::KeyPress, py::arg("key"), py::arg("frame_id"), "Simulates a key press event on a frame.")
		.def_static("get_window_position", &UIManager::GetWindowPosition, py::arg("window_id"), "Gets the position of a window.")
		.def_static("is_window_visible", &UIManager::IsWindowVisible, py::arg("window_id"), "Checks if a window is visible.")
		.def_static("set_window_visible", &UIManager::SetWindowVisible, py::arg("window_id"), py::arg("is_visible"), "Sets the visibility of a window.")
		.def_static("set_window_position", &UIManager::SetWindowPosition, py::arg("window_id"), py::arg("position"), "Sets the position of a window.")
		.def_static("is_shift_screenshot", &UIManager::IsShiftScreenShot, "Checks if the Shift key is used for screenshots.");

}
