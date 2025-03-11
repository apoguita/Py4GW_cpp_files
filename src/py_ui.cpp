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