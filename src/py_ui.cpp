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

std::vector<UIInteractionCallbackWrapper> ConvertFrameInteractionCallbacks(const GW::Array<GW::UI::UIInteractionCallback>& arr) {
    std::vector<UIInteractionCallbackWrapper> result;
    auto* callbacks = reinterpret_cast<const GW::Array<GW::UI::FrameInteractionCallback>*>(&arr);
    if (!(callbacks && callbacks->valid()))
        return result;
    result.reserve(callbacks->size());
    for (const auto& callback : *callbacks) {
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
	frame_callbacks = ConvertFrameInteractionCallbacks(frame->frame_callbacks);
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
	field24a_0x64 = frame->field24a_0x64;
	field24b_0x68 = frame->field24b_0x68;
	field25_0x6c = frame->field25_0x6c;
	field26_0x70 = frame->field26_0x70;
	field27_0x74 = frame->field27_0x74;
	field28_0x78 = frame->field28_0x78;
	field29_0x7c = frame->field29_0x7c;
	field30_0x80 = frame->field30_0x80;
	field31_0x84 = FillVectorFromPointerArray(frame->field31_0x84);
	field32_0x94 = frame->field32_0x94;
	field33_0x98 = frame->field33_0x98;
	field34_0x9c = frame->field34_0x9c;
	field35_0xa0 = frame->field35_0xa0;
	field36_0xa4 = frame->field36_0xa4;

	field40_0xc0 = frame->field40_0xc0;
	field41_0xc4 = frame->field41_0xc4;
	field42_0xc8 = frame->field42_0xc8;
	field43_0xcc = frame->field43_0xcc;
	field44_0xd0 = frame->field44_0xd0;
	field45_0xd4 = frame->field45_0xd4;
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

	field63_0x11c = frame->field63_0x11c;
	field64_0x120 = frame->field64_0x120;
	field65_0x124 = frame->field65_0x124;

	relation.parent_id = parent ? parent->frame_id : 0;
	relation.field67_0x124 = frame->relation.field67_0x124;
	relation.field68_0x128 = frame->relation.field68_0x128;
	relation.frame_hash_id = frame->relation.frame_hash_id;
	relation.siblings = GetSiblingFrameIDs(frame->frame_id);

	field73_0x144 = frame->field73_0x144;
	field74_0x148 = frame->field74_0x148;
	field75_0x14c = frame->field75_0x14c;
	field76_0x150 = frame->field76_0x150;
	field77_0x154 = frame->field77_0x154;
	field78_0x158 = frame->field78_0x158;
	field79_0x15c = frame->field79_0x15c;
	field80_0x160 = frame->field80_0x160;
	field81_0x164 = frame->field81_0x164;
	field82_0x168 = frame->field82_0x168;
	field83_0x16c = frame->field83_0x16c;
	field84_0x170 = frame->field84_0x170;
	field85_0x174 = frame->field85_0x174;
	field86_0x178 = frame->field86_0x178;
	field87_0x17c = frame->field87_0x17c;
	field88_0x180 = frame->field88_0x180;
	field89_0x184 = frame->field89_0x184;
	field90_0x188 = frame->field90_0x188;
	frame_state = frame->frame_state;
	field92_0x190 = frame->field92_0x190;
	field93_0x194 = frame->field93_0x194;
	field94_0x198 = frame->field94_0x198;
	field95_0x19c = frame->field95_0x19c;
	field96_0x1a0 = frame->field96_0x1a0;
	field97_0x1a4 = frame->field97_0x1a4;
	field98_0x1a8 = frame->field98_0x1a8;
	//TooltipInfo* tooltip_info;
	field100_0x1b0 = frame->field100_0x1b0;
	field101_0x1b4 = frame->field101_0x1b4;
	field102_0x1b8 = frame->field102_0x1b8;
	field103_0x1bc = frame->field103_0x1bc;
	field104_0x1c0 = frame->field104_0x1c0;
	field105_0x1c4 = frame->field105_0x1c4;

	is_visible = frame->IsVisible();
	is_created = frame->IsCreated();

}



PYBIND11_EMBEDDED_MODULE(PyUIManager, m) {
	py::class_<UIInteractionCallbackWrapper>(m, "UIInteractionCallback")
		.def(py::init<GW::UI::UIInteractionCallback>())
		.def_readwrite("callback_address", &UIInteractionCallbackWrapper::callback_address)
		.def_readwrite("uictl_context", &UIInteractionCallbackWrapper::uictl_context)
		.def_readwrite("h0008", &UIInteractionCallbackWrapper::h0008)
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
		.def_readwrite("field24a_0x64", &UIFrame::field24a_0x64)
		.def_readwrite("field24b_0x68", &UIFrame::field24b_0x68)
		.def_readwrite("field25_0x6c", &UIFrame::field25_0x6c)
		.def_readwrite("field26_0x70", &UIFrame::field26_0x70)
		.def_readwrite("field27_0x74", &UIFrame::field27_0x74)
		.def_readwrite("field28_0x78", &UIFrame::field28_0x78)
		.def_readwrite("field29_0x7c", &UIFrame::field29_0x7c)
		.def_readwrite("field30_0x80", &UIFrame::field30_0x80)
		.def_readwrite("field31_0x84", &UIFrame::field31_0x84)
		.def_readwrite("field32_0x94", &UIFrame::field32_0x94)
		.def_readwrite("field33_0x98", &UIFrame::field33_0x98)
		.def_readwrite("field34_0x9c", &UIFrame::field34_0x9c)
		.def_readwrite("field35_0xa0", &UIFrame::field35_0xa0)
		.def_readwrite("field36_0xa4", &UIFrame::field36_0xa4)
		.def_readwrite("frame_callbacks", &UIFrame::frame_callbacks)
		.def_readwrite("child_offset_id", &UIFrame::child_offset_id)
		.def_readwrite("field40_0xc0", &UIFrame::field40_0xc0)
		.def_readwrite("field41_0xc4", &UIFrame::field41_0xc4)
		.def_readwrite("field42_0xc8", &UIFrame::field42_0xc8)
		.def_readwrite("field43_0xcc", &UIFrame::field43_0xcc)
		.def_readwrite("field44_0xd0", &UIFrame::field44_0xd0)
		.def_readwrite("field45_0xd4", &UIFrame::field45_0xd4)
		.def_readonly("position", &UIFrame::position)
		.def_readwrite("field63_0x11c", &UIFrame::field63_0x11c)
		.def_readwrite("field64_0x120", &UIFrame::field64_0x120)
		.def_readwrite("field65_0x124", &UIFrame::field65_0x124)
		.def_readwrite("relation", &UIFrame::relation)
		.def_readwrite("field73_0x144", &UIFrame::field73_0x144)
		.def_readwrite("field74_0x148", &UIFrame::field74_0x148)
		.def_readwrite("field75_0x14c", &UIFrame::field75_0x14c)
		.def_readwrite("field76_0x150", &UIFrame::field76_0x150)
		.def_readwrite("field77_0x154", &UIFrame::field77_0x154)
		.def_readwrite("field78_0x158", &UIFrame::field78_0x158)
		.def_readwrite("field79_0x15c", &UIFrame::field79_0x15c)
		.def_readwrite("field80_0x160", &UIFrame::field80_0x160)
		.def_readwrite("field81_0x164", &UIFrame::field81_0x164)
		.def_readwrite("field82_0x168", &UIFrame::field82_0x168)
		.def_readwrite("field83_0x16c", &UIFrame::field83_0x16c)
		.def_readwrite("field84_0x170", &UIFrame::field84_0x170)
		.def_readwrite("field85_0x174", &UIFrame::field85_0x174)
		.def_readwrite("field86_0x178", &UIFrame::field86_0x178)
		.def_readwrite("field87_0x17c", &UIFrame::field87_0x17c)
		.def_readwrite("field88_0x180", &UIFrame::field88_0x180)
		.def_readwrite("field89_0x184", &UIFrame::field89_0x184)
		.def_readwrite("field90_0x188", &UIFrame::field90_0x188)
		.def_readwrite("frame_state", &UIFrame::frame_state)
		.def_readwrite("field92_0x190", &UIFrame::field92_0x190)
		.def_readwrite("field93_0x194", &UIFrame::field93_0x194)
		.def_readwrite("field94_0x198", &UIFrame::field94_0x198)
		.def_readwrite("field95_0x19c", &UIFrame::field95_0x19c)
		.def_readwrite("field96_0x1a0", &UIFrame::field96_0x1a0)
		.def_readwrite("field97_0x1a4", &UIFrame::field97_0x1a4)
		.def_readwrite("field98_0x1a8", &UIFrame::field98_0x1a8)
		.def_readwrite("field100_0x1b0", &UIFrame::field100_0x1b0)
		.def_readwrite("field101_0x1b4", &UIFrame::field101_0x1b4)
		.def_readwrite("field102_0x1b8", &UIFrame::field102_0x1b8)
		.def_readwrite("field103_0x1bc", &UIFrame::field103_0x1bc)
		.def_readwrite("field104_0x1c0", &UIFrame::field104_0x1c0)
		.def_readwrite("field105_0x1c4", &UIFrame::field105_0x1c4)

		// Methods
		.def("get_context", &UIFrame::GetContext);


	py::class_<UIManager>(m, "UIManager")
		.def_static("get_text_language", &UIManager::GetTextLanguage, "Gets the current text language.")
		.def_static("get_frame_logs", &UIManager::GetFrameLogs, "Retrieves the logs related to UI frames.")
		.def_static("clear_frame_logs", &UIManager::ClearFrameLogs, "Clears the UI frame logs.")
		.def_static("get_ui_message_logs", &UIManager::GetUIPayloads, "Retrieves the UI payloads.")
		.def_static("clear_ui_message_logs", &UIManager::ClearUIPayloads, "Clears the UI payload logs.")
		.def_static("get_frame_id_by_label", &UIManager::GetFrameIDByLabel, py::arg("label"), "Gets the frame ID associated with a given label.")
		.def_static("get_frame_id_by_hash", &UIManager::GetFrameIDByHash, py::arg("hash"), "Gets the frame ID using its hash.")
		.def_static("get_child_frame_by_frame_id", &UIManager::GetChildFrameByFrameId, py::arg("parent_frame_id"), py::arg("child_offset"), "Gets a direct child frame ID from a parent frame ID and child offset.")
		.def_static("get_child_frame_path_by_frame_id", &UIManager::GetChildFramePathByFrameId, py::arg("parent_frame_id"), py::arg("child_offsets"), "Gets a descendant frame ID by walking child offsets from a parent frame ID.")
		.def_static("get_parent_frame_id", &UIManager::GetParentFrameID, py::arg("frame_id"), "Gets the parent frame ID for a frame.")
		.def_static("get_frame_context", &UIManager::GetFrameContext, py::arg("frame_id"), "Gets the last non-null UI control context pointer for a frame.")
		.def_static("get_first_child_frame_id", &UIManager::GetFirstChildFrameID, py::arg("parent_frame_id"), "Gets the first child frame ID for a parent frame.")
		.def_static("get_last_child_frame_id", &UIManager::GetLastChildFrameID, py::arg("parent_frame_id"), "Gets the last child frame ID for a parent frame.")
		.def_static("get_next_child_frame_id", &UIManager::GetNextChildFrameID, py::arg("frame_id"), "Gets the next sibling child frame ID for a frame.")
		.def_static("get_prev_child_frame_id", &UIManager::GetPrevChildFrameID, py::arg("frame_id"), "Gets the previous sibling child frame ID for a frame.")
		.def_static("get_item_frame_id", &UIManager::GetItemFrameID, py::arg("parent_frame_id"), py::arg("index"), "Gets the child frame ID at an ordered index under a parent frame.")
		.def_static("get_tab_frame_id", &UIManager::GetTabFrameID, py::arg("parent_frame_id"), py::arg("index"), "Gets the tab child frame ID at an ordered index under a parent frame.")
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
		.def_static("get_key_mappings", &UIManager::GetKeyMappings, "Gets the current key mappings.")
		.def_static("set_key_mappings", &UIManager::SetKeyMappings, py::arg("mappings"), "Sets new key mappings.")


		.def_static(
			"SendUIMessage",
			&UIManager::SendUIMessage,
			py::arg("msgid"),
			py::arg("values"),
			py::arg("skip_hooks") = false
		)

		.def_static(
			"SendUIMessageRaw",
			&UIManager::SendUIMessageRaw,
			py::arg("msgid"),
			py::arg("wparam"),
			py::arg("lparam") = 0,
			py::arg("skip_hooks") = false
		)

		.def_static("SendFrameUIMessage",
			&UIManager::SendFrameUIMessage,
			py::arg("frame_id"),
			py::arg("message_id"),
			py::arg("wparam"),
			py::arg("lparam") = 0
		)		.def_static("SendFrameUIMessageWString",
			&UIManager::SendFrameUIMessageWString,
			py::arg("frame_id"),
			py::arg("message_id"),
			py::arg("text")
		)
		.def_static("create_ui_component_by_frame_id",
			&UIManager::CreateUIComponentByFrameId,
			py::arg("parent_frame_id"),
			py::arg("component_flags"),
			py::arg("child_index"),
			py::arg("event_callback"),
			py::arg("name_enc") = std::wstring(),
			py::arg("component_label") = std::wstring()
		)
		.def_static("create_ui_component_raw_by_frame_id",
			&UIManager::CreateUIComponentRawByFrameId,
			py::arg("parent_frame_id"),
			py::arg("component_flags"),
			py::arg("child_index"),
			py::arg("event_callback"),
			py::arg("wparam") = static_cast<uintptr_t>(0),
			py::arg("component_label") = std::wstring()
		)
		.def_static("create_labeled_frame_by_frame_id",
			&UIManager::CreateLabeledFrameByFrameId,
			py::arg("parent_frame_id"),
			py::arg("frame_flags"),
			py::arg("child_index"),
			py::arg("frame_callback"),
			py::arg("create_param"),
			py::arg("frame_label") = std::wstring()
		)
		.def_static("create_window_by_frame_id",
			&UIManager::CreateWindowByFrameId,
			py::arg("parent_frame_id"),
			py::arg("child_index"),
			py::arg("frame_callback"),
			py::arg("x"),
			py::arg("y"),
			py::arg("width"),
			py::arg("height"),
			py::arg("frame_flags") = 0,
			py::arg("create_param") = 0,
			py::arg("frame_label") = std::wstring(),
			py::arg("anchor_flags") = 0x6
		)
		.def_static("find_available_child_slot",
			&UIManager::FindAvailableChildSlot,
			py::arg("parent_frame_id"),
			py::arg("start_index") = 0x20,
			py::arg("end_index") = 0xFE
		)
		.def_static("resolve_devtext_dialog_proc",
			&UIManager::ResolveDevTextDialogProc
		)
		.def_static("ensure_devtext_source",
			&UIManager::EnsureDevTextSource
		)
		.def_static("open_devtext_window",
			&UIManager::OpenDevTextWindow
		)
		.def_static("get_devtext_frame_id",
			&UIManager::GetDevTextFrameID
		)
		.def_static("restore_devtext_source",
			&UIManager::RestoreDevTextSource,
			py::arg("opened_temporarily")
		)
		.def_static("resolve_observed_content_host_by_frame_id",
			&UIManager::ResolveObservedContentHostByFrameId,
			py::arg("root_frame_id")
		)
		.def_static("clear_frame_children_recursive_by_frame_id",
			&UIManager::ClearFrameChildrenRecursiveByFrameId,
			py::arg("frame_id")
		)
		.def_static("clear_window_contents_by_frame_id",
			&UIManager::ClearWindowContentsByFrameId,
			py::arg("root_frame_id")
		)
		.def_static("create_window",
			&UIManager::CreateWindowClone,
			py::arg("x"),
			py::arg("y"),
			py::arg("width"),
			py::arg("height"),
			py::arg("frame_label") = std::wstring(),
			py::arg("parent_frame_id") = 9,
			py::arg("child_index") = 0,
			py::arg("frame_flags") = 0,
			py::arg("create_param") = 0,
			py::arg("frame_callback") = 0,
			py::arg("anchor_flags") = 0x6,
			py::arg("ensure_devtext_source") = true
		)
		.def_static("create_empty_window",
			&UIManager::CreateEmptyWindowClone,
			py::arg("x"),
			py::arg("y"),
			py::arg("width"),
			py::arg("height"),
			py::arg("frame_label") = std::wstring(),
			py::arg("parent_frame_id") = 9,
			py::arg("child_index") = 0,
			py::arg("frame_flags") = 0,
			py::arg("create_param") = 0,
			py::arg("frame_callback") = 0,
			py::arg("anchor_flags") = 0x6,
			py::arg("ensure_devtext_source") = true
		)
		.def_static("set_frame_controller_anchor_margins_by_frame_id_ex",
			&UIManager::SetFrameControllerAnchorMarginsByFrameIdEx,
			py::arg("frame_id"),
			py::arg("x"),
			py::arg("y"),
			py::arg("width"),
			py::arg("height"),
			py::arg("flags") = 0x6
		)
		.def_static("choose_anchor_flags_for_desired_rect",
			&UIManager::ChooseAnchorFlagsForDesiredRect,
			py::arg("x"),
			py::arg("y"),
			py::arg("width"),
			py::arg("height"),
			py::arg("parent_width"),
			py::arg("parent_height"),
			py::arg("disable_center") = false
		)
		.def_static("collapse_window_by_frame_id",
			&UIManager::CollapseWindowByFrameId,
			py::arg("frame_id")
		)
		.def_static("set_frame_visible_by_frame_id",
			&UIManager::SetFrameVisibleByFrameId,
			py::arg("frame_id"),
			py::arg("is_visible")
		)
		.def_static("set_frame_disabled_by_frame_id",
			&UIManager::SetFrameDisabledByFrameId,
			py::arg("frame_id"),
			py::arg("is_disabled")
		)
		.def_static("set_frame_title_by_frame_id",
			&UIManager::SetFrameTitleByFrameId,
			py::arg("frame_id"),
			py::arg("title")
		)
		.def_static("get_frame_label_by_frame_id",
			&UIManager::GetFrameLabelByFrameId,
			py::arg("frame_id")
		)
		.def_static("get_text_label_encoded_by_frame_id",
			&UIManager::GetTextLabelEncodedByFrameId,
			py::arg("frame_id")
		)
		.def_static("get_text_label_encoded_bytes_by_frame_id",
			&UIManager::GetTextLabelEncodedBytesByFrameId,
			py::arg("frame_id")
		)
		.def_static("get_text_label_decoded_by_frame_id",
			&UIManager::GetTextLabelDecodedByFrameId,
			py::arg("frame_id")
		)
		.def_static("set_label_by_frame_id",
			&UIManager::SetLabelByFrameId,
			py::arg("frame_id"),
			py::arg("label")
		)
		.def_static("set_text_label_by_frame_id",
			&UIManager::SetTextLabelByFrameId,
			py::arg("frame_id"),
			py::arg("label")
		)
		.def_static("set_text_label_bytes_by_frame_id",
			&UIManager::SetTextLabelBytesByFrameId,
			py::arg("frame_id"),
			py::arg("label_bytes")
		)
		.def_static("append_text_label_encoded_suffix_by_frame_id",
			&UIManager::AppendTextLabelEncodedSuffixByFrameId,
			py::arg("frame_id"),
			py::arg("encoded_suffix")
		)
		.def_static("append_text_label_plain_suffix_by_frame_id",
			&UIManager::AppendTextLabelPlainSuffixByFrameId,
			py::arg("frame_id"),
			py::arg("plain_text")
		)
		.def_static("set_multiline_label_by_frame_id",
			&UIManager::SetMultilineLabelByFrameId,
			py::arg("frame_id"),
			py::arg("label")
		)
		.def_static("set_text_label_font_by_frame_id",
			&UIManager::SetTextLabelFontByFrameId,
			py::arg("frame_id"),
			py::arg("font_id")
		)
		.def_static("set_read_only_by_frame_id",
			&UIManager::SetReadOnlyByFrameId,
			py::arg("frame_id"),
			py::arg("is_read_only")
		)
		.def_static("is_read_only_by_frame_id",
			&UIManager::IsReadOnlyByFrameId,
			py::arg("frame_id")
		)
		.def_static("restore_window_rect_by_frame_id",
			&UIManager::RestoreWindowRectByFrameId,
			py::arg("frame_id"),
			py::arg("x"),
			py::arg("y"),
			py::arg("width"),
			py::arg("height"),
			py::arg("flags") = 0,
			py::arg("use_auto_flags") = true,
			py::arg("disable_center") = true
		)
		.def_static("set_frame_margins_by_frame_id",
			&UIManager::SetFrameMarginsByFrameId,
			py::arg("frame_id"),
			py::arg("flags"),
			py::arg("x"),
			py::arg("y"),
			py::arg("width"),
			py::arg("height")
		)
		.def_static("set_next_created_window_title",
			&UIManager::SetNextCreatedWindowTitle,
			py::arg("title")
		)
		.def_static("clear_next_created_window_title",
			&UIManager::ClearNextCreatedWindowTitle
		)
		.def_static("has_next_created_window_title",
			&UIManager::HasNextCreatedWindowTitle
		)
		.def_static("is_window_title_hook_installed",
			&UIManager::IsWindowTitleHookInstalled
		)
		.def_static("get_last_applied_window_title_frame_id",
			&UIManager::GetLastAppliedWindowTitleFrameId
		)
		.def_static("get_last_applied_window_title",
			&UIManager::GetLastAppliedWindowTitle
		)
		.def_static("destroy_ui_component_by_frame_id",
			&UIManager::DestroyUIComponentByFrameId,
			py::arg("frame_id")
		)
		.def_static("add_frame_ui_interaction_callback_by_frame_id",
			&UIManager::AddFrameUIInteractionCallbackByFrameId,
			py::arg("frame_id"),
			py::arg("event_callback"),
			py::arg("wparam") = 0
		)
		.def_static("trigger_frame_redraw_by_frame_id",
			&UIManager::TriggerFrameRedrawByFrameId,
			py::arg("frame_id")
		)
		.def_static("create_button_frame_by_frame_id",
			&UIManager::CreateButtonFrameByFrameId,
			py::arg("parent_frame_id"),
			py::arg("component_flags"),
			py::arg("child_index") = 0,
			py::arg("name_enc") = std::wstring(),
			py::arg("component_label") = std::wstring()
		)
		.def_static("create_checkbox_frame_by_frame_id",
			&UIManager::CreateCheckboxFrameByFrameId,
			py::arg("parent_frame_id"),
			py::arg("component_flags"),
			py::arg("child_index") = 0,
			py::arg("name_enc") = std::wstring(),
			py::arg("component_label") = std::wstring()
		)
		.def_static("create_scrollable_frame_by_frame_id",
			&UIManager::CreateScrollableFrameByFrameId,
			py::arg("parent_frame_id"),
			py::arg("component_flags"),
			py::arg("child_index") = 0,
			py::arg("page_context") = 0,
			py::arg("component_label") = std::wstring()
		)
		.def_static("create_text_label_frame_by_frame_id",
			&UIManager::CreateTextLabelFrameByFrameId,
			py::arg("parent_frame_id"),
			py::arg("component_flags"),
			py::arg("child_index") = 0,
			py::arg("name_enc") = std::wstring(),
			py::arg("component_label") = std::wstring()
		)
		.def_static("create_text_label_frame_with_plain_text_by_frame_id",
			&UIManager::CreateTextLabelFrameWithPlainTextByFrameId,
			py::arg("parent_frame_id"),
			py::arg("component_flags"),
			py::arg("child_index") = 0,
			py::arg("plain_text") = std::wstring(),
			py::arg("component_label") = std::wstring()
		)
		.def_static("create_text_label_frame_from_template_by_frame_id",
			&UIManager::CreateTextLabelFrameFromTemplateByFrameId,
			py::arg("parent_frame_id"),
			py::arg("component_flags"),
			py::arg("child_index"),
			py::arg("template_frame_id"),
			py::arg("plain_text") = std::wstring(),
			py::arg("component_label") = std::wstring()
		)
		.def_static("get_text_label_create_payload_diagnostics_by_template_frame_id",
			&UIManager::GetTextLabelCreatePayloadDiagnosticsByTemplateFrameId,
			py::arg("template_frame_id"),
			py::arg("plain_text") = std::wstring()
		)
		.def_static("get_text_label_literal_create_payload_diagnostics",
			&UIManager::GetTextLabelLiteralCreatePayloadDiagnostics,
			py::arg("plain_text") = std::wstring()
		)
		/*
		.def_static("register_create_ui_component_callback",
			&UIManager::RegisterCreateUIComponentCallback,
			py::arg("callback"),
			py::arg("altitude") = -0x8000
		)
		.def_static("remove_create_ui_component_callback",
			&UIManager::RemoveCreateUIComponentCallback,
			py::arg("handle")
		)
		*/

		.def_static("button_click", &UIManager::ButtonClick, py::arg("frame_id"), "Simulates a button click on a frame.")
		.def_static("button_double_click", &UIManager::ButtonDoubleClick, py::arg("frame_id"), "Simulates a button double click on a frame.")
		.def_static("test_mouse_action", &UIManager::TestMouseAction, py::arg("frame_id"), py::arg("current_state"), py::arg("wparam_value") = 0, py::arg("lparam") = 0, "Simulates a mouse action on a frame.")
		.def_static("test_mouse_click_action", &UIManager::TestMouseClickAction, py::arg("frame_id"), py::arg("current_state"), py::arg("wparam_value") = 0, py::arg("lparam") = 0, "Simulates a mouse action on a frame.")
		.def_static("get_root_frame_id", &UIManager::GetRootFrameID, "Gets the ID of the root frame.")
		.def_static("is_world_map_showing", &UIManager::IsWorldMapShowing, "Checks if the world map is currently showing.")
		.def_static("is_ui_drawn", &UIManager::IsUIDrawn, "Checks if the UI is currently drawn.")
		.def_static("async_decode_str", &UIManager::AsyncDecodeStr, py::arg("enc_str"), "Decodes an encoded GW string.")
		.def_static("is_valid_enc_str", &UIManager::IsValidEncStr, py::arg("enc_str"), "Checks if an encoded string is valid.")
		.def_static("is_valid_enc_bytes", &UIManager::IsValidEncBytes, py::arg("enc_bytes"), "Checks if encoded bytes are valid.")
		.def_static("uint32_to_enc_str", &UIManager::UInt32ToEncStr, py::arg("value"), "Encodes a uint32 into a GW encoded string.")
		.def_static("enc_str_to_uint32", &UIManager::EncStrToUInt32, py::arg("enc_str"), "Decodes a GW encoded string into a uint32.")
		.def_static("set_open_links", &UIManager::SetOpenLinks, py::arg("toggle"), "Enables or disables GW open-links behavior.")
		.def_static("draw_on_compass", &UIManager::DrawOnCompass, py::arg("session_id"), py::arg("points"), "Draws a polyline on the compass.")
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



