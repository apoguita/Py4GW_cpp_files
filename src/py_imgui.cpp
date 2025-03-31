#include "py_imgui.h"

// Text
void ImGui_Text(const std::string& text) {
    ImGui::Text("%s", text.c_str());
}

void ImGui_TextWrapped(const std::string& text) {
    ImGui::TextWrapped("%s", text.c_str());
}

void ImGui_TextColored(const std::string& text, const std::array<float, 4>& color) {
    ImGui::TextColored(ImVec4(color[0], color[1], color[2], color[3]), "%s", text.c_str());
}

void ImGui_TextDisabled(const std::string& text) {
    ImGui::TextDisabled("%s", text.c_str());
}

/*
void ImGui_TextEx(const std::string& text, ImGuiTextFlags flags) {
    ImGui::TextEx("%s", text.c_str(), flags);
}
*/


void ImGui_TextUnformatted(const std::string& text) {
    ImGui::TextUnformatted(text.c_str());

}

void ImGui_TextScaled(std::string& text, const std::array<float, 4>& color, float scale) {
    ImFont* currentFont = ImGui::GetFont();   // Get the current font
    float originalFontSize = currentFont->Scale;  // Save the original font scale
    currentFont->Scale = scale;  // Set the new scale for the font

    ImGui::PushFont(currentFont);  // Apply the new font scale

    // Convert std::string to const char* for ImGui::AddText
    ImGui::TextColored(ImVec4(color[0], color[1], color[2], color[3]), "%s", text.c_str());

    // Restore the original font size
    currentFont->Scale = originalFontSize;  // Restore original font scale
    ImGui::PopFont();  // Pop the font after rendering
}

//get text line height
float ImGui_GetTextLineHeight() {
    return ImGui::GetTextLineHeight();
}

//get text line height with spacing
float ImGui_GetTextLineHeightWithSpacing() {
    return ImGui::GetTextLineHeightWithSpacing();
}

//calculate text size
std::array<float, 2> ImGui_CalcTextSize(const std::string& text) {
    ImVec2 size = ImGui::CalcTextSize(text.c_str());
    return { size.x, size.y };
}

// Button
bool ImGui_Button(const std::string& label, float width = 0, float height = 0) {
    return ImGui::Button(label.c_str(), ImVec2(width, height));
}

// Checkbox
bool ImGui_Checkbox(const std::string& label, bool v) {
    bool temp = v;
    ImGui::Checkbox(label.c_str(), &temp);
    return temp;
}

// RadioButton
std::pair<bool, int> ImGui_RadioButton(const std::string& label, int v, int button_index) {
    bool selected = (v == button_index);
    if (ImGui::RadioButton(label.c_str(), selected)) {
        v = button_index;  // Update the selected button
    }
    return { selected, v };  // Return a pair: (was selected, updated value)
}

// SliderFloat
float ImGui_SliderFloat(const std::string& label, float v, float v_min, float v_max) {
    float temp = v;
    ImGui::SliderFloat(label.c_str(), &temp, v_min, v_max);
    return temp;
}


// SliderInt
int ImGui_SliderInt(const std::string& label, int v, int v_min, int v_max) {
    int temp = v;
    ImGui::SliderInt(label.c_str(), &temp, v_min, v_max);
    return temp;
}


// InputText
// Overload 1: InputText with no flags
std::string ImGui_InputText(const std::string& label, const std::string& text) {
    // Create a buffer initialized with the input text
    std::vector<char> buffer(text.begin(), text.end());
    buffer.resize(256);  // Resize buffer to a fixed size

    // Call ImGui's InputText
    ImGui::InputText(label.c_str(), buffer.data(), buffer.size());

    // Return the updated text as a string
    return std::string(buffer.data());
}

// Overload 2: InputText with flags
std::string ImGui_InputText(const std::string& label, const std::string& text, ImGuiInputTextFlags flags) {
    // Create a buffer initialized with the input text
    std::vector<char> buffer(text.begin(), text.end());
    buffer.resize(256);  // Resize buffer to a fixed size

    // Call ImGui's InputText with flags
    ImGui::InputText(label.c_str(), buffer.data(), buffer.size(), flags);

    // Return the updated text as a string
    return std::string(buffer.data());
}


// InputFloat
float ImGui_InputFloat(const std::string& label, float v) {
    float temp = v;
    ImGui::InputFloat(label.c_str(), &temp);
    return temp;
}


// InputInt
int ImGui_InputInt(const std::string& label, int v) {
    int temp = v;
    ImGui::InputInt(label.c_str(), &temp);
    return temp;
}


// Combo
int ImGui_Combo(const std::string& label, int current_item, const std::vector<std::string>& items) {
    int temp = current_item;
    std::vector<const char*> items_cstr;
    for (const std::string& item : items) {
        items_cstr.push_back(item.c_str());
    }
    ImGui::Combo(label.c_str(), &temp, items_cstr.data(), items_cstr.size());
    return temp;
}

//selectable
bool ImGui_Selectable(const std::string& label, bool selected, ImGuiSelectableFlags flags, const std::array<float, 2>& size) {
	return ImGui::Selectable(label.c_str(), selected, flags, ImVec2(size[0], size[1]));
}

// ColorEdit3
std::vector<float> ImGui_ColorEdit3(const std::string& label, const std::vector<float>& color) {
    std::vector<float> temp = color;
    if (ImGui::ColorEdit3(label.c_str(), temp.data())) {
        return temp; // Return the updated color
    }
    return color; // Return the original color if not changed
}


// ColorEdit4
std::array<float, 4> ImGui_ColorEdit4(const std::string& label, const std::array<float, 4>& color) {
    std::array<float, 4> temp = color;
    if (ImGui::ColorEdit4(label.c_str(), temp.data())) {
        return temp; // Return the updated color
    }
    return color; // Return the original color if not changed
}

//push_style_color
void ImGui_PushStyleColor(ImGuiCol idx, const std::array<float, 4>& col) {
    ImGui::PushStyleColor(idx, ImVec4(col[0], col[1], col[2], col[3]));
}

// pop style color
void ImGui_PopStyleColor(int count = 1) {
    ImGui::PopStyleColor(count);
}

// push style var
void ImGui_PushStyleVar(int idx, float val) {
	ImGuiStyleVar_ idx_ = static_cast<ImGuiStyleVar_>(idx);
    ImGui::PushStyleVar(idx_, val);
}

void ImGui_PushStyleVar2(int idx, float x, float y) {
	ImGuiStyleVar_ idx_ = static_cast<ImGuiStyleVar_>(idx);
    ImGui::PushStyleVar(idx_, ImVec2(x, y));
}


// pop style var
void ImGui_PopStyleVar(int count = 1) {
    ImGui::PopStyleVar(count);
}

// push item width
void ImGui_PushItemWidth(float item_width) {
    ImGui::PushItemWidth(item_width);
}

// pop item width
void ImGui_PopItemWidth() {
    ImGui::PopItemWidth();
}

// push text wrap pos
void ImGui_PushTextWrapPos(float wrap_pos_x = 0.0f) {
    ImGui::PushTextWrapPos(wrap_pos_x);
}

// pop text wrap pos
void ImGui_PopTextWrapPos() {
    ImGui::PopTextWrapPos();
}

// push allow keyboard focus
void ImGui_PushAllowKeyboardFocus(bool allow_keyboard_focus) {
    //ImGui::PushAllowKeyboardFocus(allow_keyboard_focus);
    (void)allow_keyboard_focus;
    return;
}

// pop allow keyboard focus
void ImGui_PopAllowKeyboardFocus() {
    //ImGui::PopAllowKeyboardFocus();
    return;
}

// push button repeat
void ImGui_PushButtonRepeat(bool repeat) {
    ImGui::PushButtonRepeat(repeat);
}

// pop button repeat
void ImGui_PopButtonRepeat() {
    ImGui::PopButtonRepeat();
}





// ProgressBar
void ImGui_ProgressBar(float fraction, float size_arg = -1.0f, const std::string& overlay = "") {
    ImGui::ProgressBar(fraction, ImVec2(size_arg, 0), overlay.empty() ? nullptr : overlay.c_str());
}

// BulletText
void ImGui_BulletText(const std::string& text) {
    ImGui::BulletText("%s", text.c_str());
}

///// WINDOWS PANELS AND GROUPS 

// Begin and End Window
bool ImGui_Begin(const std::string& name) {
    return ImGui::Begin(name.c_str());
}

bool ImGui_Begin(const std::string& name, ImGuiWindowFlags flags) {
    return ImGui::Begin(name.c_str(), nullptr, flags);
}

bool ImGui_Begin(const std::string& name, bool* p_open, ImGuiWindowFlags flags) {
    return ImGui::Begin(name.c_str(), p_open, flags);
}

void ImGui_End() {
    ImGui::End();
}

// BeginChild and EndChild
bool ImGui_BeginChild(const std::string& str_id) {
    return ImGui::BeginChild(str_id.c_str());
}

bool ImGui_BeginChild(const std::string& id, const std::array<float, 2>& size, bool border, ImGuiWindowFlags flags) {
    // Pass the ID, size, border, and flags to ImGui::BeginChild
    return ImGui::BeginChild(id.c_str(), ImVec2(size[0], size[1]), border, flags);
}


void ImGui_EndChild() {
    ImGui::EndChild();
}

// BeginGroup and EndGroup
void ImGui_BeginGroup() {
    ImGui::BeginGroup();
}

void ImGui_EndGroup() {
    ImGui::EndGroup();
}

// Separator
void ImGui_Separator() {
    ImGui::Separator();
}

// SameLine
void ImGui_SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f) {
    ImGui::SameLine(offset_from_start_x, spacing);
}

// Spacing
void ImGui_Spacing() {
    ImGui::Spacing();
}

// Indent and Unindent
void ImGui_Indent(float indent_w = 0.0f) {
    ImGui::Indent(indent_w);
}

void ImGui_Unindent(float indent_w = 0.0f) {
    ImGui::Unindent(indent_w);
}

////////// LAYOUT 

// Columns
void ImGui_Columns(int count = 1, const std::string& id = "", bool border = true) {
    ImGui::Columns(count, id.empty() ? nullptr : id.c_str(), border);
}

void ImGui_NextColumn() {
    ImGui::NextColumn();
}

void ImGui_EndColumns() {
    ImGui::Columns(1); // Reset columns
}

// SetNextWindowSize
void ImGui_SetNextWindowSize(float width, float height) {
    ImGui::SetNextWindowSize(ImVec2(width, height));
}

// SetNextWindowPos
void ImGui_SetNextWindowPos(float x, float y) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
}

void ImGui_SetNextWindowCollapsed(bool collapsed, ImGuiCond cond = 0) {
	ImGui::SetNextWindowCollapsed(collapsed, cond);
}
/////////////MENUS AND TOOLBARS

// Menu Bars
bool ImGui_BeginMenuBar() {
    return ImGui::BeginMenuBar();
}

void ImGui_EndMenuBar() {
    ImGui::EndMenuBar();
}

bool ImGui_BeginMainMenuBar() {
    return ImGui::BeginMainMenuBar();
}

void ImGui_EndMainMenuBar() {
    ImGui::EndMainMenuBar();
}

// Menus
bool ImGui_BeginMenu(const std::string& label) {
    return ImGui::BeginMenu(label.c_str());
}

void ImGui_EndMenu() {
    ImGui::EndMenu();
}

// MenuItem
bool ImGui_MenuItem(const std::string& label) {
    return ImGui::MenuItem(label.c_str());
}

// Popups
void ImGui_OpenPopup(const std::string& str_id) {
    ImGui::OpenPopup(str_id.c_str());
}

bool ImGui_BeginPopup(const std::string& str_id) {
    return ImGui::BeginPopup(str_id.c_str());
}

void ImGui_EndPopup() {
    ImGui::EndPopup();
}

// Modals
bool ImGui_BeginPopupModal(const std::string& name, bool* p_open = nullptr) {
    return ImGui::BeginPopupModal(name.c_str(), p_open);
}

void ImGui_EndPopupModal() {
    ImGui::EndPopup();
}

// CloseCurrentPopup
void ImGui_CloseCurrentPopup() {
    ImGui::CloseCurrentPopup();
}


// Tables
// Overload 1: BeginTable with only the required parameters
bool ImGui_BeginTable(const std::string& id, int columns) {
    return ImGui::BeginTable(id.c_str(), columns);
}

// Overload 2: BeginTable with flags
bool ImGui_BeginTable(const std::string& id, int columns, ImGuiTableFlags flags) {
    return ImGui::BeginTable(id.c_str(), columns, flags);
}

// Overload 3: BeginTable with flags and outer_size
bool ImGui_BeginTable(const std::string& id, int columns, ImGuiTableFlags flags, float width, float height) {
    return ImGui::BeginTable(id.c_str(), columns, flags, ImVec2(width, height));
}

void ImGui_EndTable() {
    ImGui::EndTable();
}

void ImGui_TableNextRow() {
    ImGui::TableNextRow();
}

void ImGui_TableNextColumn() {
    ImGui::TableNextColumn();
}

void ImGui_TableSetupColumn(const std::string& label) {
    ImGui::TableSetupColumn(label.c_str());
}

void ImGui_TableSetupColumn(const std::string& label, ImGuiTableColumnFlags flags) {
    ImGui::TableSetupColumn(label.c_str(), flags);
}

void ImGui_TableSetColumnIndex(int column_index) {
    ImGui::TableSetColumnIndex(column_index);
}

void ImGui_TableGetSortSpecs() {
	ImGui::TableGetSortSpecs();
}

void ImGui_TableHeadersRow() {
    ImGui::TableHeadersRow();
}

int ImGui_TableGetColumnCount() {
    return ImGui::TableGetColumnCount();
}

int ImGui_TableGetColumnIndex() {
	return ImGui::TableGetColumnIndex();
}

int ImGui_TableGetRowIndex() {
	return ImGui::TableGetRowIndex();
}



// Tabs
bool ImGui_BeginTabBar(const std::string& str_id) {
    return ImGui::BeginTabBar(str_id.c_str());
}

void ImGui_EndTabBar() {
    ImGui::EndTabBar();
}

bool ImGui_BeginTabItem(const std::string& label) {
    return ImGui::BeginTabItem(label.c_str());
}

bool ImGui_BeginTabItem(const std::string& label, bool popen) {
	return ImGui::BeginTabItem(label.c_str(), &popen);
}

bool ImGui_BeginTabItem(const std::string& label, bool p_open, ImGuiTabItemFlags flags) {
    return ImGui::BeginTabItem(label.c_str(), &p_open, flags);
}

void ImGui_EndTabItem() {
    ImGui::EndTabItem();
}

//#include "imgui_internal.h"

// GetWindowDrawList
void ImDrawList_AddLine(float x1, float y1, float x2, float y2, ImU32 col, float thickness) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), col, thickness);
}

void ImDrawList_AddRect(float x1, float y1, float x2, float y2, ImU32 col, float rounding, int rounding_corners_flags, float thickness) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), col, rounding, rounding_corners_flags, thickness);
}

void ImDrawList_AddCircle(float x, float y, float radius, ImU32 col, int num_segments, float thickness) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCircle(ImVec2(x, y), radius, col, num_segments, thickness);
}

void ImDrawList_AddText(float x, float y, ImU32 col, const std::string& text) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddText(ImVec2(x, y), col, text.c_str());
}

void ImDrawList_AddRectFilled(float x1, float y1, float x2, float y2, ImU32 col, float rounding, int rounding_corners_flags) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), col, rounding, rounding_corners_flags);
}

void ImDrawList_AddCircleFilled(float x, float y, float radius, ImU32 col, int num_segments) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCircleFilled(ImVec2(x, y), radius, col, num_segments);
}

void ImDrawList_AddTriangle(float x1, float y1, float x2, float y2, float x3, float y3, ImU32 col, float thickness) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddTriangle(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), col, thickness);
}

void ImDrawList_AddTriangleFilled(float x1, float y1, float x2, float y2, float x3, float y3, ImU32 col) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddTriangleFilled(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), col);
}

void ImDrawList_AddQuad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, ImU32 col, float thickness) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddQuad(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ImVec2(x4, y4), col, thickness);
}

void ImDrawList_AddQuadFilled(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, ImU32 col) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddQuadFilled(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ImVec2(x4, y4), col);
}


//get_scroll_max_x
float ImGui_GetScrollMaxX() {
    return ImGui::GetScrollMaxX();
}

//get_scroll_max_y
float ImGui_GetScrollMaxY() {
    return ImGui::GetScrollMaxY();
}

//get_scroll_x
float ImGui_GetScrollX() {
    return ImGui::GetScrollX();
}

//get_scroll_y
float ImGui_GetScrollY() {
    return ImGui::GetScrollY();
}

//get_style
ImGuiStyle& GetStyle() {
    return ImGui::GetStyle();

}

//get_cursor_pos
std::array<float, 2> ImGui_GetCursorPos() {
    ImVec2 pos = ImGui::GetCursorPos();
    return { pos.x, pos.y };
}

//set_cursor_pos
void ImGui_SetCursorPos(float x, float y) {
	ImGui::SetCursorPos(ImVec2(x, y));
}

//get_cursor_pos_x
float ImGui_GetCursorPosX() {
    return ImGui::GetCursorPosX();
}

//set_cursor_pos_x
void ImGui_SetCursorPosX(float x) {
	ImGui::SetCursorPosX(x);
}

//get_cursor_pos_y
float ImGui_GetCursorPosY() {
    return ImGui::GetCursorPosY();
}

//set_cursor_pos_y
void ImGui_SetCursorPosY(float y) {
	ImGui::SetCursorPosY(y);
}

//get)cursor_start_pos
std::array<float, 2> ImGui_GetCursorStartPos() {
    ImVec2 pos = ImGui::GetCursorStartPos();
    return { pos.x, pos.y };
}

//is_rect_visible
bool ImGui_IsRectVisible(float width, float height) {
    return ImGui::IsRectVisible(ImVec2(width, height));
}

// Windows

// GetWindowPos
std::array<float, 2> ImGui_GetWindowPos() {
    ImVec2 pos = ImGui::GetWindowPos();
    return { pos.x, pos.y };
}

// GetWindowSize
std::array<float, 2> ImGui_GetWindowSize() {
    ImVec2 size = ImGui::GetWindowSize();
    return { size.x, size.y };
}

// GetWindowWidth
float ImGui_GetWindowWidth() {
    return ImGui::GetWindowWidth();
}

// GetWindowHeight
float ImGui_GetWindowHeight() {
    return ImGui::GetWindowHeight();
}

// GetContentRegionAvail
std::array<float, 2> ImGui_GetContentRegionAvail() {
    ImVec2 size = ImGui::GetContentRegionAvail();
    return { size.x, size.y };
}

// GetContentRegionMax
std::array<float, 2> ImGui_GetContentRegionMax() {
    ImVec2 size = ImGui::GetContentRegionMax();
    return { size.x, size.y };
}

// GetWindowContentRegionMin
std::array<float, 2> ImGui_GetWindowContentRegionMin() {
    ImVec2 size = ImGui::GetWindowContentRegionMin();
    return { size.x, size.y };
}

// GetWindowContentRegionMax
std::array<float, 2> ImGui_GetWindowContentRegionMax() {
    ImVec2 size = ImGui::GetWindowContentRegionMax();
    return { size.x, size.y };
}


// Mouse
bool ImGui_IsMouseClicked(int button) {
    return ImGui::IsMouseClicked(button);
}

bool ImGui_IsMouseDoubleClicked(int button) {
	return ImGui::IsMouseDoubleClicked(button);
}

bool ImGui_IsMouseDown(int button) {
	return ImGui::IsMouseDown(button);
}

bool ImGui_IsMouseReleased(int button) {
	return ImGui::IsMouseReleased(button);
}

bool ImGui_IsMouseDragging(int button, float lock_threshold) {
	return ImGui::IsMouseDragging(button, lock_threshold);
}


// Hovering
bool ImGui_IsItemHovered() {
    return ImGui::IsItemHovered();
}

// Is Window Open
bool ImGui_IsWindowCollapsed() {
    return ImGui::IsWindowCollapsed();
}
// Active
bool ImGui_IsItemActive() {
    return ImGui::IsItemActive();
}

// Keyboard
bool ImGui_IsKeyPressed(int key) {
    ImGuiKey imgui_key = static_cast<ImGuiKey>(key);
    return ImGui::IsKeyPressed(imgui_key);

    //return ImGui::IsKeyPressed(key);
}


// Demo Window
void ImGui_ShowDemoWindow() {
    ImGui::ShowDemoWindow();
}

// Tooltip
void ImGui_SetTooltip(const std::string& text) {
    ImGui::SetTooltip("%s", text.c_str());
}

void ShowTooltip(const char* tooltipText)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("%s", tooltipText);
        ImGui::EndTooltip();
    }
}

void BeginTooltip() {
    ImGui::BeginTooltip();
}

void EndTooltip() {
    ImGui::EndTooltip();
}

// Log to Clipboard
void ImGui_LogToClipboard() {
    ImGui::LogToClipboard();
}

//tree nodes
// Overload 1: TreeNode with label only
bool ImGui_TreeNode(const std::string& label) {
    return ImGui::TreeNode(label.c_str());
}

// Overload 2: TreeNode with format
bool ImGui_TreeNode(const std::string& str_id, const std::string& fmt) {
    return ImGui::TreeNode(str_id.c_str(), "%s", fmt.c_str());
}

// Overload 3: TreeNode with string ID and TreeNodeFlags
bool ImGui_TreeNodeEx(const std::string& str_id, ImGuiTreeNodeFlags flags) {
    return ImGui::TreeNodeEx(str_id.c_str(), flags);
}

// Overload 4: TreeNode with string ID, format, and TreeNodeFlags
bool ImGui_TreeNodeEx(const std::string& str_id, ImGuiTreeNodeFlags flags, const std::string& fmt) {
    return ImGui::TreeNodeEx(str_id.c_str(), flags, "%s", fmt.c_str());
}

void ImGui_TreePop() {
    ImGui::TreePop();
}

float ImGui_GetTreeNodeToLabelSpacing() {
    return ImGui::GetTreeNodeToLabelSpacing();
}

void ImGui_SetNextItemOpen(bool is_open, ImGuiCond cond = 0) {
    ImGui::SetNextItemOpen(is_open, cond);
}

void ImGui_SetNextItemWidth(float item_width) {
	ImGui::SetNextItemWidth(item_width);
}
// collapsing headers
// Function to wrap ImGui::CollapsingHeader without flags
bool ImGui_CollapsingHeader(const std::string& label) {
    return ImGui::CollapsingHeader(label.c_str());
}

// Function to wrap ImGui::CollapsingHeader with flags
bool ImGui_CollapsingHeader(const std::string& label, ImGuiTreeNodeFlags flags) {

    return ImGui::CollapsingHeader(label.c_str(), flags);
}


int GetColumnIndex(const ImGuiTableColumnSortSpecs& spec) {
    return spec.ColumnIndex;  // Access bit field
}

int GetSortDirection(const ImGuiTableColumnSortSpecs& spec) {
    return spec.SortDirection;  // Access bit field
}

void ClearSortSpecsDirty(ImGuiTableSortSpecs* specs) {
    specs->SpecsDirty = false;  // Allow clearing the flag safely
}

void Dummy(const int width, const int height) {
	ImGui::Dummy(ImVec2(width, height));
}

PYBIND11_EMBEDDED_MODULE(PyImGui, m) {
    py::enum_<ImGuiSortDirection_>(m, "SortDirection")
        .value("NoDirection", ImGuiSortDirection_None)
        .value("Ascending", ImGuiSortDirection_Ascending)
        .value("Descending", ImGuiSortDirection_Descending)
        .export_values();
        
    py::class_<ImGuiTableColumnSortSpecs>(m, "TableColumnSortSpecs")
        .def_property_readonly("ColumnIndex", &GetColumnIndex)  // Use getter
        .def_property_readonly("SortDirection", &GetSortDirection); // Use getter

    py::class_<ImGuiTableSortSpecs>(m, "TableSortSpecs")
        .def_readonly("SpecsCount", &ImGuiTableSortSpecs::SpecsCount)
        .def_readonly("SpecsDirty", &ImGuiTableSortSpecs::SpecsDirty)
        .def_property_readonly("Specs", [](const ImGuiTableSortSpecs& self) {
        return py::cast(self.Specs, py::return_value_policy::reference);
            });


    py::enum_<ImGuiWindowFlags_>(m, "WindowFlags")
        .value("NoFlag", ImGuiWindowFlags_None)
        .value("NoTitleBar", ImGuiWindowFlags_NoTitleBar)
        .value("NoResize", ImGuiWindowFlags_NoResize)
        .value("NoMove", ImGuiWindowFlags_NoMove)
        .value("NoScrollbar", ImGuiWindowFlags_NoScrollbar)
        .value("NoScrollWithMouse", ImGuiWindowFlags_NoScrollWithMouse)
        .value("NoCollapse", ImGuiWindowFlags_NoCollapse)
        .value("AlwaysAutoResize", ImGuiWindowFlags_AlwaysAutoResize)
        .value("NoBackground", ImGuiWindowFlags_NoBackground)
        .value("NoSavedSettings", ImGuiWindowFlags_NoSavedSettings)
        .value("NoMouseInputs", ImGuiWindowFlags_NoMouseInputs)
        .value("MenuBar", ImGuiWindowFlags_MenuBar)
        .value("HorizontalScrollbar", ImGuiWindowFlags_HorizontalScrollbar)
        .value("NoFocusOnAppearing", ImGuiWindowFlags_NoFocusOnAppearing)
        .value("NoBringToFrontOnFocus", ImGuiWindowFlags_NoBringToFrontOnFocus)
        .value("AlwaysVerticalScrollbar", ImGuiWindowFlags_AlwaysVerticalScrollbar)
        .value("AlwaysHorizontalScrollbar", ImGuiWindowFlags_AlwaysHorizontalScrollbar)
        //.value("AlwaysUseWindowPadding", ImGuiWindowFlags_AlwaysUseWindowPadding)
        .value("NoNavInputs", ImGuiWindowFlags_NoNavInputs)
        .value("NoNavFocus", ImGuiWindowFlags_NoNavFocus)
        .value("UnsavedDocument", ImGuiWindowFlags_UnsavedDocument)
        .export_values()
        .def("__or__", [](ImGuiWindowFlags_ a, ImGuiWindowFlags_ b) {
        return static_cast<ImGuiWindowFlags_>(static_cast<int>(a) | static_cast<int>(b));
            });

    // Input Text Flags
    py::enum_<ImGuiInputTextFlags_>(m, "InputTextFlags")
        .value("NoFlag", ImGuiInputTextFlags_None)
        .value("CharsDecimal", ImGuiInputTextFlags_CharsDecimal)
        .value("CharsHexadecimal", ImGuiInputTextFlags_CharsHexadecimal)
        .value("CharsUppercase", ImGuiInputTextFlags_CharsUppercase)
        .value("CharsNoBlank", ImGuiInputTextFlags_CharsNoBlank)
        .value("AutoSelectAll", ImGuiInputTextFlags_AutoSelectAll)
        .value("EnterReturnsTrue", ImGuiInputTextFlags_EnterReturnsTrue)
        .value("CallbackCompletion", ImGuiInputTextFlags_CallbackCompletion)
        .value("CallbackHistory", ImGuiInputTextFlags_CallbackHistory)
        .value("CallbackAlways", ImGuiInputTextFlags_CallbackAlways)
        .value("CallbackCharFilter", ImGuiInputTextFlags_CallbackCharFilter)
        .value("AllowTabInput", ImGuiInputTextFlags_AllowTabInput)
        .value("CtrlEnterForNewLine", ImGuiInputTextFlags_CtrlEnterForNewLine)
        .value("NoHorizontalScroll", ImGuiInputTextFlags_NoHorizontalScroll)
        //.value("AlwaysInsertMode", ImGuiInputTextFlags_AlwaysInsertMode)
        .value("ReadOnly", ImGuiInputTextFlags_ReadOnly)
        .value("Password", ImGuiInputTextFlags_Password)
        .value("NoUndoRedo", ImGuiInputTextFlags_NoUndoRedo)
        .value("CharsScientific", ImGuiInputTextFlags_CharsScientific)
        .value("CallbackResize", ImGuiInputTextFlags_CallbackResize)
        .value("CallbackEdit", ImGuiInputTextFlags_CallbackEdit)
        .export_values()
        .def("__or__", [](ImGuiInputTextFlags_ a, ImGuiInputTextFlags_ b) {
        return static_cast<ImGuiInputTextFlags_>(static_cast<int>(a) | static_cast<int>(b));
            });

    // Tree Node Flags
    py::enum_<ImGuiTreeNodeFlags_>(m, "TreeNodeFlags")
        .value("NoFlag", ImGuiTreeNodeFlags_None)
        .value("Selected", ImGuiTreeNodeFlags_Selected)
        .value("Framed", ImGuiTreeNodeFlags_Framed)
        //.value("AllowItemOverlap", ImGuiTreeNodeFlags_AllowItemOverlap)
        .value("NoTreePushOnOpen", ImGuiTreeNodeFlags_NoTreePushOnOpen)
        .value("NoAutoOpenOnLog", ImGuiTreeNodeFlags_NoAutoOpenOnLog)
        .value("DefaultOpen", ImGuiTreeNodeFlags_DefaultOpen)
        .value("OpenOnDoubleClick", ImGuiTreeNodeFlags_OpenOnDoubleClick)
        .value("OpenOnArrow", ImGuiTreeNodeFlags_OpenOnArrow)
        .value("Leaf", ImGuiTreeNodeFlags_Leaf)
        .value("Bullet", ImGuiTreeNodeFlags_Bullet)
        .value("FramePadding", ImGuiTreeNodeFlags_FramePadding)
        .value("SpanAvailWidth", ImGuiTreeNodeFlags_SpanAvailWidth)
        .value("SpanFullWidth", ImGuiTreeNodeFlags_SpanFullWidth)
        .value("NavLeftJumpsBackHere", ImGuiTreeNodeFlags_NavLeftJumpsBackHere)
        .value("CollapsingHeader", ImGuiTreeNodeFlags_CollapsingHeader)
        .export_values()
        .def("__or__", [](ImGuiTreeNodeFlags_ a, ImGuiTreeNodeFlags_ b) {
        return static_cast<ImGuiTreeNodeFlags_>(static_cast<int>(a) | static_cast<int>(b));
            });

    // Selectable Flags
    py::enum_<ImGuiSelectableFlags_>(m, "SelectableFlags")
        .value("NoFlag", ImGuiSelectableFlags_None)
        .value("DontClosePopups", ImGuiSelectableFlags_DontClosePopups)
        .value("SpanAllColumns", ImGuiSelectableFlags_SpanAllColumns)
        .value("AllowDoubleClick", ImGuiSelectableFlags_AllowDoubleClick)
        .value("Disabled", ImGuiSelectableFlags_Disabled)
        //.value("AllowItemOverlap", ImGuiSelectableFlags_AllowItemOverlap)
        .export_values()
        .def("__or__", [](ImGuiSelectableFlags_ a, ImGuiSelectableFlags_ b) {
        return static_cast<ImGuiSelectableFlags_>(static_cast<int>(a) | static_cast<int>(b));
            });

    // Table Flags
    py::enum_<ImGuiTableFlags_>(m, "TableFlags")
        .value("NoFlag", ImGuiTableFlags_None)
        .value("Resizable", ImGuiTableFlags_Resizable)
        .value("Reorderable", ImGuiTableFlags_Reorderable)
        .value("Hideable", ImGuiTableFlags_Hideable)
        .value("Sortable", ImGuiTableFlags_Sortable)
        .value("NoSavedSettings", ImGuiTableFlags_NoSavedSettings)
        .value("ContextMenuInBody", ImGuiTableFlags_ContextMenuInBody)
        .value("RowBg", ImGuiTableFlags_RowBg)
        .value("BordersInnerH", ImGuiTableFlags_BordersInnerH)
        .value("BordersOuterH", ImGuiTableFlags_BordersOuterH)
        .value("BordersInnerV", ImGuiTableFlags_BordersInnerV)
        .value("BordersOuterV", ImGuiTableFlags_BordersOuterV)
        .value("BordersH", ImGuiTableFlags_BordersH)
        .value("BordersV", ImGuiTableFlags_BordersV)
        .value("Borders", ImGuiTableFlags_Borders)
        .value("NoBordersInBody", ImGuiTableFlags_NoBordersInBody)
        .value("NoBordersInBodyUntilResize", ImGuiTableFlags_NoBordersInBodyUntilResize)
        .value("SizingFixedFit", ImGuiTableFlags_SizingFixedFit)
        .value("SizingFixedSame", ImGuiTableFlags_SizingFixedSame)
        .value("SizingStretchProp", ImGuiTableFlags_SizingStretchProp)
        .value("SizingStretchSame", ImGuiTableFlags_SizingStretchSame)
        .value("NoHostExtendX", ImGuiTableFlags_NoHostExtendX)
        .value("NoHostExtendY", ImGuiTableFlags_NoHostExtendY)
        .value("NoKeepColumnsVisible", ImGuiTableFlags_NoKeepColumnsVisible)
        .value("PreciseWidths", ImGuiTableFlags_PreciseWidths)
        .value("NoClip", ImGuiTableFlags_NoClip)
        .value("PadOuterX", ImGuiTableFlags_PadOuterX)
        .value("NoPadOuterX", ImGuiTableFlags_NoPadOuterX)
        .value("NoPadInnerX", ImGuiTableFlags_NoPadInnerX)
        .value("ScrollX", ImGuiTableFlags_ScrollX)
        .value("ScrollY", ImGuiTableFlags_ScrollY)
        .value("SortMulti", ImGuiTableFlags_SortMulti)
        .value("SortTristate", ImGuiTableFlags_SortTristate)
        .export_values()
        .def("__or__", [](ImGuiTableFlags_ a, ImGuiTableFlags_ b) {
        return static_cast<ImGuiTableFlags_>(static_cast<int>(a) | static_cast<int>(b));
            })
        .def("__and__", [](ImGuiTableFlags_ a, ImGuiTableFlags_ b) {
        return static_cast<ImGuiTableFlags_>(static_cast<int>(a) & static_cast<int>(b));
            });

    // Table Column Flags
    py::enum_<ImGuiTableColumnFlags_>(m, "TableColumnFlags")
        .value("NoFlag", ImGuiTableColumnFlags_None)
        .value("DefaultHide", ImGuiTableColumnFlags_DefaultHide)
        .value("DefaultSort", ImGuiTableColumnFlags_DefaultSort)
        .value("WidthStretch", ImGuiTableColumnFlags_WidthStretch)
        .value("WidthFixed", ImGuiTableColumnFlags_WidthFixed)
        .value("NoResize", ImGuiTableColumnFlags_NoResize)
        .value("NoReorder", ImGuiTableColumnFlags_NoReorder)
        .value("NoHide", ImGuiTableColumnFlags_NoHide)
        .value("NoClip", ImGuiTableColumnFlags_NoClip)
        .value("NoSort", ImGuiTableColumnFlags_NoSort)
        .value("NoSortAscending", ImGuiTableColumnFlags_NoSortAscending)
        .value("NoSortDescending", ImGuiTableColumnFlags_NoSortDescending)
        .value("IndentEnable", ImGuiTableColumnFlags_IndentEnable)
        .value("IndentDisable", ImGuiTableColumnFlags_IndentDisable)
        .value("IsEnabled", ImGuiTableColumnFlags_IsEnabled)
        .value("IsVisible", ImGuiTableColumnFlags_IsVisible)
        .value("IsSorted", ImGuiTableColumnFlags_IsSorted)
        .value("IsHovered", ImGuiTableColumnFlags_IsHovered)
        .export_values()
        .def("__or__", [](ImGuiTableColumnFlags_ a, ImGuiTableColumnFlags_ b) {
        return static_cast<ImGuiTableColumnFlags_>(static_cast<int>(a) | static_cast<int>(b));
            });

    // Table Row Flags
    py::enum_<ImGuiTableRowFlags_>(m, "TableRowFlags")
        .value("NoFlag", ImGuiTableRowFlags_None)
        .value("Headers", ImGuiTableRowFlags_Headers)
        .export_values();



    // Focused Flags
    py::enum_<ImGuiFocusedFlags_>(m, "FocusedFlags")
        .value("NoFlag", ImGuiFocusedFlags_None)
        .value("ChildWindows", ImGuiFocusedFlags_ChildWindows)
        .value("RootWindow", ImGuiFocusedFlags_RootWindow)
        .value("AnyWindow", ImGuiFocusedFlags_AnyWindow)
        .value("RootAndChildWindows", ImGuiFocusedFlags_RootAndChildWindows)
        .export_values();

    // Hovered Flags
    py::enum_<ImGuiHoveredFlags_>(m, "HoveredFlags")
        .value("NoFlag", ImGuiHoveredFlags_None)
        .value("ChildWindows", ImGuiHoveredFlags_ChildWindows)
        .value("RootWindow", ImGuiHoveredFlags_RootWindow)
        .value("AnyWindow", ImGuiHoveredFlags_AnyWindow)
        .value("AllowWhenBlockedByPopup", ImGuiHoveredFlags_AllowWhenBlockedByPopup)
        .value("AllowWhenBlockedByActiveItem", ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
        .value("AllowWhenOverlapped", ImGuiHoveredFlags_AllowWhenOverlapped)
        .value("AllowWhenDisabled", ImGuiHoveredFlags_AllowWhenDisabled)
        .export_values()
        .def("__or__", [](ImGuiHoveredFlags_ a, ImGuiHoveredFlags_ b) {
        return static_cast<ImGuiHoveredFlags_>(static_cast<int>(a) | static_cast<int>(b));
            });



    // Bind the ImGuiIO structure
    py::class_<ImGuiIO>(m, "ImGuiIO")
        .def_readwrite("display_size", &ImGuiIO::DisplaySize)
        .def_readwrite("delta_time", &ImGuiIO::DeltaTime)
        .def_readwrite("ini_saving_rate", &ImGuiIO::IniSavingRate)
        .def_readwrite("ini_filename", &ImGuiIO::IniFilename)
        .def_readwrite("log_filename", &ImGuiIO::LogFilename)
        .def_readwrite("mouse_double_click_time", &ImGuiIO::MouseDoubleClickTime)
        .def_readwrite("mouse_double_click_max_dist", &ImGuiIO::MouseDoubleClickMaxDist)
        .def_readwrite("mouse_drag_threshold", &ImGuiIO::MouseDragThreshold)
        .def_readwrite("mouse_pos", &ImGuiIO::MousePos)
        .def_readwrite("mouse_wheel", &ImGuiIO::MouseWheel)
        .def_readwrite("mouse_wheel_h", &ImGuiIO::MouseWheelH)
        .def_readwrite("key_ctrl", &ImGuiIO::KeyCtrl)
        .def_readwrite("key_shift", &ImGuiIO::KeyShift)
        .def_readwrite("key_alt", &ImGuiIO::KeyAlt)
        .def_readwrite("key_super", &ImGuiIO::KeySuper)
        .def_readwrite("framerate", &ImGuiIO::Framerate)
        .def_readwrite("metrics_render_vertices", &ImGuiIO::MetricsRenderVertices)
        .def_readwrite("metrics_render_indices", &ImGuiIO::MetricsRenderIndices)
        .def_readwrite("metrics_active_windows", &ImGuiIO::MetricsActiveWindows)
        //.def_readwrite("metrics_active_allocations", &ImGuiIO::MetricsActiveAllocations)
        .def_readwrite("want_capture_mouse", &ImGuiIO::WantCaptureMouse)
        .def_readwrite("want_capture_keyboard", &ImGuiIO::WantCaptureKeyboard)
        .def_readwrite("want_text_input", &ImGuiIO::WantTextInput)
        .def_readwrite("want_set_mouse_pos", &ImGuiIO::WantSetMousePos)
        .def_readwrite("want_save_ini_settings", &ImGuiIO::WantSaveIniSettings)
        .def_readwrite("mouse_pos_prev", &ImGuiIO::MousePosPrev)
        .def_readwrite("app_focus_lost", &ImGuiIO::AppFocusLost)
        // Add more ImGuiIO members as needed
        ;

    // Bind the ImGui::GetIO function
    m.def("get_io", []() -> ImGuiIO& {
        return ImGui::GetIO();
        }, py::return_value_policy::reference);


    py::enum_<ImGuiColWrapper>(m, "ImGuiCol")
        .value("Text", ImGuiColWrapper::Text)
        .value("TextDisabled", ImGuiColWrapper::TextDisabled)
        .value("WindowBg", ImGuiColWrapper::WindowBg)
        .value("ChildBg", ImGuiColWrapper::ChildBg)
        .value("PopupBg", ImGuiColWrapper::PopupBg)
        .value("Border", ImGuiColWrapper::Border)
        .value("BorderShadow", ImGuiColWrapper::BorderShadow)
        .value("FrameBg", ImGuiColWrapper::FrameBg)
        .value("FrameBgHovered", ImGuiColWrapper::FrameBgHovered)
        .value("FrameBgActive", ImGuiColWrapper::FrameBgActive)
        .value("TitleBg", ImGuiColWrapper::TitleBg)
        .value("TitleBgActive", ImGuiColWrapper::TitleBgActive)
        .value("TitleBgCollapsed", ImGuiColWrapper::TitleBgCollapsed)
        .value("MenuBarBg", ImGuiColWrapper::MenuBarBg)
        .value("ScrollbarBg", ImGuiColWrapper::ScrollbarBg)
        .value("ScrollbarGrab", ImGuiColWrapper::ScrollbarGrab)
        .value("ScrollbarGrabHovered", ImGuiColWrapper::ScrollbarGrabHovered)
        .value("ScrollbarGrabActive", ImGuiColWrapper::ScrollbarGrabActive)
        .value("CheckMark", ImGuiColWrapper::CheckMark)
        .value("SliderGrab", ImGuiColWrapper::SliderGrab)
        .value("SliderGrabActive", ImGuiColWrapper::SliderGrabActive)
        .value("Button", ImGuiColWrapper::Button)
        .value("ButtonHovered", ImGuiColWrapper::ButtonHovered)
        .value("ButtonActive", ImGuiColWrapper::ButtonActive)
        .value("Header", ImGuiColWrapper::Header)
        .value("HeaderHovered", ImGuiColWrapper::HeaderHovered)
        .value("HeaderActive", ImGuiColWrapper::HeaderActive)
        .value("Separator", ImGuiColWrapper::Separator)
        .value("SeparatorHovered", ImGuiColWrapper::SeparatorHovered)
        .value("SeparatorActive", ImGuiColWrapper::SeparatorActive)
        .value("ResizeGrip", ImGuiColWrapper::ResizeGrip)
        .value("ResizeGripHovered", ImGuiColWrapper::ResizeGripHovered)
        .value("ResizeGripActive", ImGuiColWrapper::ResizeGripActive)
        .value("Tab", ImGuiColWrapper::Tab)
        .value("TabHovered", ImGuiColWrapper::TabHovered)
        .value("TabActive", ImGuiColWrapper::TabActive)
        //.value("TabSelected", ImGuiColWrapper::TabSelected)
        //.value("TabSelectedOverline", ImGuiColWrapper::TabSelectedOverline)
        //.value("TabDimmed", ImGuiColWrapper::TabDimmed)
        //.value("TabDimmedSelected", ImGuiColWrapper::TabDimmedSelected)
        //.value("TabDimmedSelectedOverline", ImGuiColWrapper::TabDimmedSelectedOverline)
        //.value("DockingPreview", ImGuiColWrapper::DockingPreview)
        //.value("DockingEmptyBg", ImGuiColWrapper::DockingEmptyBg)
        .value("TabUnfocused", ImGuiColWrapper::TabUnfocused)
        .value("TabUnfocusedActive", ImGuiColWrapper::TabUnfocusedActive)
        .value("PlotLines", ImGuiColWrapper::PlotLines)
        .value("PlotLinesHovered", ImGuiColWrapper::PlotLinesHovered)
        .value("PlotHistogram", ImGuiColWrapper::PlotHistogram)
        .value("PlotHistogramHovered", ImGuiColWrapper::PlotHistogramHovered)
        .value("TableHeaderBg", ImGuiColWrapper::TableHeaderBg)
        .value("TableBorderStrong", ImGuiColWrapper::TableBorderStrong)
        .value("TableBorderLight", ImGuiColWrapper::TableBorderLight)
        .value("TableRowBg", ImGuiColWrapper::TableRowBg)
        .value("TableRowBgAlt", ImGuiColWrapper::TableRowBgAlt)
        .value("TextSelectedBg", ImGuiColWrapper::TextSelectedBg)
        .value("DragDropTarget", ImGuiColWrapper::DragDropTarget)
        .value("NavHighlight", ImGuiColWrapper::NavHighlight)
        .value("NavWindowingHighlight", ImGuiColWrapper::NavWindowingHighlight)
        .value("NavWindowingDimBg", ImGuiColWrapper::NavWindowingDimBg)
        .value("ModalWindowDimBg", ImGuiColWrapper::ModalWindowDimBg)
        .export_values()
        .def("__or__", [](ImGuiColWrapper a, ImGuiColWrapper b) {
        return static_cast<ImGuiColWrapper>(static_cast<int>(a) | static_cast<int>(b));
            });

    // Basic Widgets
    m.def("text", &ImGui_Text, "Displays a text in ImGui");
    m.def("text_wrapped", &ImGui_TextWrapped, "Displays a text_wrapped in ImGui");
    m.def("text_colored", &ImGui_TextColored, "Displays a text in a colored font in ImGui");
    m.def("text_disabled", &ImGui_TextDisabled, "Displays a text in a disabled font in ImGui");
    //m.def("text_ex", &ImGui_TextEx, "Displays a text with flags in ImGui");
    m.def("text_unformatted", &ImGui_TextUnformatted, "Displays an unformatted text in ImGui");
	m.def("text_scaled", &ImGui_TextScaled, "Displays a scaled text in ImGui");


    m.def("get_text_line_height", &ImGui_GetTextLineHeight, "Returns the text line height in ImGui");
    m.def("get_text_line_height_with_spacing", &ImGui_GetTextLineHeightWithSpacing, "Returns the text line height with spacing in ImGui");
    m.def("calc_text_size", &ImGui_CalcTextSize, "Calculates the size of a text in ImGui");

    m.def("button", &ImGui_Button, py::arg("label"), py::arg("width") = 0, py::arg("height") = 0, "Creates an ImGui button with an optional size.");
    m.def("checkbox", &ImGui_Checkbox, "Creates a checkbox in ImGui");
    //m.def("radio_button", &ImGui_RadioButton, "Creates a radio button in ImGui");
    m.def("radio_button", [](const std::string& label, int v, int button_index) {
        auto result = ImGui_RadioButton(label, v, button_index);
        return result.second;  // Return the updated index value
        });

    m.def("slider_float", &ImGui_SliderFloat, "Creates a slider for floats in ImGui");
    m.def("slider_int", &ImGui_SliderInt, "Creates a slider for integers in ImGui");
    m.def("input_text", py::overload_cast<const std::string&, const std::string&>(&ImGui_InputText), "InputText without flags");
    m.def("input_text", py::overload_cast<const std::string&, const std::string&, ImGuiInputTextFlags>(&ImGui_InputText), "InputText with flags");
    m.def("input_float", &ImGui_InputFloat, "Creates a float input in ImGui");
    m.def("input_int", &ImGui_InputInt, "Creates an integer input in ImGui");
    m.def("combo", &ImGui_Combo, "Creates a combo box in ImGui");
	m.def("selectable", &ImGui_Selectable, "Creates a selectable item in ImGui");
    m.def("color_edit3", &ImGui_ColorEdit3, "A function to create a color editor (RGB)");
    m.def("color_edit4", &ImGui_ColorEdit4, "Creates an RGBA color editor in ImGui");

    m.def("get_scroll_max_x", &ImGui_GetScrollMaxX, "Returns the maximum scroll value in the x-direction");
    m.def("get_scroll_max_y", &ImGui_GetScrollMaxY, "Returns the maximum scroll value in the y-direction");
    m.def("get_scroll_x", &ImGui_GetScrollX, "Returns the current scroll value in the x-direction");
    m.def("get_scroll_y", &ImGui_GetScrollY, "Returns the current scroll value in the y-direction");

    // Bind ImGui::GetStyle function
    m.def("get_style", &GetStyle, py::return_value_policy::reference);

    
    // Optionally, expose ImGuiStyle members for Python to access and modify
    py::class_<ImGuiStyle>(m, "ImGuiStyle")
        // Floats
        .def_readwrite("Alpha", &ImGuiStyle::Alpha)
        .def_readwrite("DisabledAlpha", &ImGuiStyle::DisabledAlpha)
        .def_readwrite("WindowRounding", &ImGuiStyle::WindowRounding)
        .def_readwrite("WindowBorderSize", &ImGuiStyle::WindowBorderSize)
        .def_readwrite("ChildRounding", &ImGuiStyle::ChildRounding)
        .def_readwrite("ChildBorderSize", &ImGuiStyle::ChildBorderSize)
        .def_readwrite("PopupRounding", &ImGuiStyle::PopupRounding)
        .def_readwrite("PopupBorderSize", &ImGuiStyle::PopupBorderSize)
        .def_readwrite("FrameRounding", &ImGuiStyle::FrameRounding)
        .def_readwrite("FrameBorderSize", &ImGuiStyle::FrameBorderSize)
        .def_readwrite("IndentSpacing", &ImGuiStyle::IndentSpacing)
        .def_readwrite("ColumnsMinSpacing", &ImGuiStyle::ColumnsMinSpacing)
        .def_readwrite("ScrollbarSize", &ImGuiStyle::ScrollbarSize)
        .def_readwrite("ScrollbarRounding", &ImGuiStyle::ScrollbarRounding)
        .def_readwrite("GrabMinSize", &ImGuiStyle::GrabMinSize)
        .def_readwrite("GrabRounding", &ImGuiStyle::GrabRounding)
        .def_readwrite("LogSliderDeadzone", &ImGuiStyle::LogSliderDeadzone)
        .def_readwrite("TabRounding", &ImGuiStyle::TabRounding)
        .def_readwrite("TabBorderSize", &ImGuiStyle::TabBorderSize)
        .def_readwrite("TabMinWidthForCloseButton", &ImGuiStyle::TabMinWidthForCloseButton)
        .def_readwrite("MouseCursorScale", &ImGuiStyle::MouseCursorScale)
        .def_readwrite("CurveTessellationTol", &ImGuiStyle::CurveTessellationTol)
        .def_readwrite("CircleTessellationMaxError", &ImGuiStyle::CircleTessellationMaxError)

        // Vectors
        .def_readwrite("WindowPadding", &ImGuiStyle::WindowPadding)
        .def_readwrite("WindowMinSize", &ImGuiStyle::WindowMinSize)
        .def_readwrite("WindowTitleAlign", &ImGuiStyle::WindowTitleAlign)
        .def_readwrite("FramePadding", &ImGuiStyle::FramePadding)
        .def_readwrite("ItemSpacing", &ImGuiStyle::ItemSpacing)
        .def_readwrite("ItemInnerSpacing", &ImGuiStyle::ItemInnerSpacing)
        .def_readwrite("CellPadding", &ImGuiStyle::CellPadding)
        .def_readwrite("TouchExtraPadding", &ImGuiStyle::TouchExtraPadding)
        .def_readwrite("ButtonTextAlign", &ImGuiStyle::ButtonTextAlign)
        .def_readwrite("SelectableTextAlign", &ImGuiStyle::SelectableTextAlign)
        .def_readwrite("SeparatorTextAlign", &ImGuiStyle::SeparatorTextAlign)
        .def_readwrite("SeparatorTextPadding", &ImGuiStyle::SeparatorTextPadding)
        .def_readwrite("DisplayWindowPadding", &ImGuiStyle::DisplayWindowPadding)
        .def_readwrite("DisplaySafeAreaPadding", &ImGuiStyle::DisplaySafeAreaPadding)

        // Booleans
        .def_readwrite("AntiAliasedLines", &ImGuiStyle::AntiAliasedLines)
        .def_readwrite("AntiAliasedLinesUseTex", &ImGuiStyle::AntiAliasedLinesUseTex)
        .def_readwrite("AntiAliasedFill", &ImGuiStyle::AntiAliasedFill)

        // Directions (Enums)
        .def_readwrite("WindowMenuButtonPosition", &ImGuiStyle::WindowMenuButtonPosition)
        .def_readwrite("ColorButtonPosition", &ImGuiStyle::ColorButtonPosition)

        // Hover Behavior
        .def_readwrite("HoverStationaryDelay", &ImGuiStyle::HoverStationaryDelay)
        .def_readwrite("HoverDelayShort", &ImGuiStyle::HoverDelayShort)
        .def_readwrite("HoverDelayNormal", &ImGuiStyle::HoverDelayNormal)
        .def_readwrite("HoverFlagsForTooltipMouse", &ImGuiStyle::HoverFlagsForTooltipMouse)
        .def_readwrite("HoverFlagsForTooltipNav", &ImGuiStyle::HoverFlagsForTooltipNav)
        // Methods
        .def(py::init<>())  // Default constructor
        .def("ScaleAllSizes", &ImGuiStyle::ScaleAllSizes, py::arg("scale_factor"));  // Scaling function
    



    m.def("get_cursor_pos", &ImGui_GetCursorPos, "Returns the cursor position in ImGui");
    m.def("get_cursor_pos_x", &ImGui_GetCursorPosX, "Returns the cursor position in the x-direction");
    m.def("get_cursor_pos_y", &ImGui_GetCursorPosY, "Returns the cursor position in the y-direction");
	m.def("set_cursor_pos", &ImGui_SetCursorPos, "Sets the cursor position in ImGui");
	m.def("set_cursor_pos_x", &ImGui_SetCursorPosX, "Sets the cursor position in the x-direction");
	m.def("set_cursor_pos_y", &ImGui_SetCursorPosY, "Sets the cursor position in the y-direction");
    m.def("get_cursor_start_pos", &ImGui_GetCursorStartPos, "Returns the cursor start position in ImGui");
    m.def("is_rect_visible", &ImGui_IsRectVisible, "Checks if a rectangle is visible in ImGui");



    m.def("push_style_color", &ImGui_PushStyleColor, "Push a style color in ImGui");

    m.def("pop_style_color", &ImGui_PopStyleColor, "Pops a style color in ImGui");
    m.def("push_style_var", &ImGui_PushStyleVar, "Pushes a style variable with a single value.");
    m.def("push_style_var2", &ImGui_PushStyleVar2, "Pushes a style variable with two values.");

    m.def("pop_style_var", &ImGui_PopStyleVar, "Pops a style variable in ImGui");
    m.def("push_item_width", &ImGui_PushItemWidth, "Pushes an item width in ImGui");
    m.def("pop_item_width", &ImGui_PopItemWidth, "Pops an item width in ImGui");
    m.def("push_text_wrap_pos", &ImGui_PushTextWrapPos, "Pushes a text wrap position in ImGui");
    m.def("pop_text_wrap_pos", &ImGui_PopTextWrapPos, "Pops a text wrap position in ImGui");
    //m.def("push_allow_keyboard_focus", &ImGui_PushAllowKeyboardFocus, "Pushes an allow keyboard focus in ImGui");
    //m.def("pop_allow_keyboard_focus", &ImGui_PopAllowKeyboardFocus, "Pops an allow keyboard focus in ImGui");
    m.def("push_button_repeat", &ImGui_PushButtonRepeat, "Pushes a button repeat in ImGui");
    m.def("pop_button_repeat", &ImGui_PopButtonRepeat, "Pops a button repeat in ImGui");

    m.def("progress_bar", &ImGui_ProgressBar, "Displays a progress bar in ImGui");
    m.def("bullet_text", &ImGui_BulletText, "Displays bullet text in ImGui");

    // Windows, Panels, and Groups
    m.def("begin", py::overload_cast<const std::string&>(&ImGui_Begin), "Begin an ImGui window with just a name");
    m.def("begin", py::overload_cast<const std::string&, ImGuiWindowFlags>(&ImGui_Begin), "Begin an ImGui window with a name and window flags");
    m.def("begin", py::overload_cast<const std::string&, bool*, ImGuiWindowFlags>(&ImGui_Begin), "Begin an ImGui window with a name, open condition, and window flags");

    m.def("end", &ImGui_End, "Ends the current ImGui window");
    //m.def("begin_child", &ImGui_BeginChild, "Begins a new child window in ImGui")
    m.def("begin_child", py::overload_cast<const std::string&>(&ImGui_BeginChild), py::arg("id"), "Begin a child window with just the ID");
    m.def("begin_child", py::overload_cast<const std::string&, const std::array<float, 2>&, bool, ImGuiWindowFlags>(&ImGui_BeginChild), py::arg("id"), py::arg("size") = std::array<float, 2>{0.0f, 0.0f}, py::arg("border") = false, py::arg("flags") = 0, "Begin a child window with ID, size, border, and flags");
    m.def("end_child", &ImGui_EndChild, "Ends the current child window in ImGui");
    m.def("begin_group", &ImGui_BeginGroup, "Begins a new group in ImGui");
    m.def("end_group", &ImGui_EndGroup, "Ends the current group in ImGui");
    m.def("separator", &ImGui_Separator, "Inserts a separator line in ImGui");
    m.def("same_line", &ImGui_SameLine, "Positions widgets on the same line in ImGui");
    m.def("spacing", &ImGui_Spacing, "Inserts vertical spacing in ImGui");
    m.def("indent", &ImGui_Indent, "Indents widgets in ImGui");
    m.def("unindent", &ImGui_Unindent, "Unindents widgets in ImGui");
    m.def("is_window_collapsed", &ImGui_IsWindowCollapsed, "ImGui is_window_collapsed function");

    // Layout
    m.def("columns", &ImGui_Columns, "Creates columns in ImGui");
    m.def("next_column", &ImGui_NextColumn, "Moves to the next column in ImGui");
    m.def("end_columns", &ImGui_EndColumns, "Ends the columns in ImGui");
    m.def("set_next_window_size", &ImGui_SetNextWindowSize, "Sets the size of the next window in ImGui");
    m.def("set_next_window_pos", &ImGui_SetNextWindowPos, "Sets the position of the next window in ImGui");
	m.def("set_next_window_collapsed", &ImGui_SetNextWindowCollapsed, "Sets the collapsed state of the next window in ImGui");

    // Menus and Toolbars
    m.def("begin_menu_bar", &ImGui_BeginMenuBar, "Begins a menu bar in ImGui");
    m.def("end_menu_bar", &ImGui_EndMenuBar, "Ends the menu bar in ImGui");
    m.def("begin_main_menu_bar", &ImGui_BeginMainMenuBar, "Begins the main menu bar in ImGui");
    m.def("end_main_menu_bar", &ImGui_EndMainMenuBar, "Ends the main menu bar in ImGui");
    m.def("begin_menu", &ImGui_BeginMenu, "Begins a menu in ImGui");
    m.def("end_menu", &ImGui_EndMenu, "Ends the menu in ImGui");
    m.def("menu_item", &ImGui_MenuItem, "Creates a menu item in ImGui");

    // Popups and Modals
    m.def("open_popup", &ImGui_OpenPopup, "Opens a popup in ImGui");
    m.def("begin_popup", &ImGui_BeginPopup, "Begins a popup in ImGui");
    m.def("end_popup", &ImGui_EndPopup, "Ends the popup in ImGui");
    m.def("begin_popup_modal", &ImGui_BeginPopupModal, "Begins a modal popup in ImGui");
    m.def("end_popup_modal", &ImGui_EndPopupModal, "Ends the modal popup in ImGui");
    m.def("close_current_popup", &ImGui_CloseCurrentPopup, "Closes the current popup in ImGui");

    // Tables
    m.def("begin_table", py::overload_cast<const std::string&, int>(&ImGui_BeginTable));
    m.def("begin_table", py::overload_cast<const std::string&, int, ImGuiTableFlags>(&ImGui_BeginTable));
    m.def("begin_table", py::overload_cast<const std::string&, int, ImGuiTableFlags, float, float>(&ImGui_BeginTable));
    m.def("end_table", &ImGui_EndTable, "Ends the table in ImGui");
    m.def("table_setup_column", py::overload_cast<const std::string&>(&ImGui_TableSetupColumn), py::arg("label"));
    m.def("table_setup_column", py::overload_cast<const std::string&, ImGuiTableColumnFlags>(&ImGui_TableSetupColumn), py::arg("label"), py::arg("flags"));
    m.def("table_headers_row", &ImGui_TableHeadersRow, "Submit a headers row for a table");
	m.def("table_get_column_count", &ImGui_TableGetColumnCount, "Returns the number of columns in the table");
	m.def("table_get_column_index", &ImGui_TableGetColumnIndex, "Returns the current column index in the table");
	m.def("table_get_row_index", &ImGui_TableGetRowIndex, "Returns the current row index in the table");
    m.def("table_next_row", &ImGui_TableNextRow, "Moves to the next row in the table");
    m.def("table_next_column", &ImGui_TableNextColumn, "Moves to the next column in the table");
    m.def("table_set_column_index", &ImGui_TableSetColumnIndex, "Sets the current column index in the table");
    m.def("table_get_sort_specs", []() -> ImGuiTableSortSpecs* {
        return ImGui::TableGetSortSpecs();  // Return a pointer to the sort specs
        }, py::return_value_policy::reference);
    m.def("clear_sort_specs_dirty", &ClearSortSpecsDirty, "Clear the dirty flag in sort specs.");

    // Tabs
    m.def("begin_tab_bar", &ImGui_BeginTabBar, "Begins a tab bar in ImGui");
    m.def("end_tab_bar", &ImGui_EndTabBar, "Ends the tab bar in ImGui");
    // Overload bindings
    m.def("begin_tab_item",
        py::overload_cast<const std::string&>(&ImGui_BeginTabItem),
        "Begin tab item with label");

    m.def("begin_tab_item",
        py::overload_cast<const std::string&, bool>(&ImGui_BeginTabItem),
        "Begin tab item with label and popen");

    m.def("begin_tab_item",
        py::overload_cast<const std::string&, bool, ImGuiTabItemFlags>(&ImGui_BeginTabItem),
        "Begin tab item with full params");

    m.def("end_tab_item", &ImGui_EndTabItem, "Ends the tab item in ImGui");

    // Drawing
    m.def("draw_list_add_line", &ImDrawList_AddLine, "Draws a line in the draw list");
    m.def("draw_list_add_rect", &ImDrawList_AddRect, "Draws a rectangle in the draw list");
    m.def("draw_list_add_rect_filled", &ImDrawList_AddRectFilled, "Draws a filled rectangle in the draw list");
    m.def("draw_list_add_circle", &ImDrawList_AddCircle, "Draws a circle in the draw list");
    m.def("draw_list_add_circle_filled", &ImDrawList_AddCircleFilled, "Draws a filled circle in the draw list");
    m.def("draw_list_add_triangle", &ImDrawList_AddTriangle, "Draws a triangle in the draw list");
    m.def("draw_list_add_triangle_filled", &ImDrawList_AddTriangleFilled, "Draws a filled triangle in the draw list");
    m.def("draw_list_add_text", &ImDrawList_AddText, "Draws text in the draw list");
    m.def("draw_list_add_quad", &ImDrawList_AddQuad, "Draws a quadrilateral in the draw list");
    m.def("draw_list_add_quad_filled", &ImDrawList_AddQuadFilled, "Draws a filled quadrilateral in the draw list, taking four points (x1, y1), (x2, y2), (x3, y3), (x4, y4) and a color.");



    //Windows
    m.def("get_window_pos", &ImGui_GetWindowPos, "Get the current window position (x, y)");
    m.def("get_window_size", &ImGui_GetWindowSize, "Get the current window size (width, height)");
    m.def("get_window_width", &ImGui_GetWindowWidth, "Get the current window width");
    m.def("get_window_height", &ImGui_GetWindowHeight, "Get the current window height");
    m.def("get_content_region_avail", &ImGui_GetContentRegionAvail, "Get the available content region size (width, height)");
    m.def("get_content_region_max", &ImGui_GetContentRegionMax, "Get the maximum content region size (width, height)");
    m.def("get_window_content_region_min", &ImGui_GetWindowContentRegionMin, "Get the minimum content region within the window (x, y)");
    m.def("get_window_content_region_max", &ImGui_GetWindowContentRegionMax, "Get the maximum content region within the window (x, y)");


    // Input Handling
    m.def("is_mouse_clicked", &ImGui_IsMouseClicked, "Checks if the mouse button is clicked");
	m.def("is_mouse_double_clicked", &ImGui_IsMouseDoubleClicked, "Checks if the mouse button is double clicked");
	m.def("is_mouse_down", &ImGui_IsMouseDown, "Checks if the mouse button is down");
	m.def("is_mouse_released", &ImGui_IsMouseReleased, "Checks if the mouse button is released");
	m.def("is_mouse_dragging", &ImGui_IsMouseDragging, "Checks if the mouse is dragging");
    m.def("is_item_hovered", &ImGui_IsItemHovered, "Checks if the last item is hovered");
    m.def("is_item_active", &ImGui_IsItemActive, "Checks if the last item is active");
    m.def("is_key_pressed", &ImGui_IsKeyPressed, "Checks if a key is pressed");

    // Miscellaneous
    m.def("show_demo_window", &ImGui_ShowDemoWindow, "Shows the ImGui demo window");
    m.def("set_tooltip", &ImGui_SetTooltip, "Sets a tooltip in ImGui");
    m.def("show_tooltip", &ShowTooltip, "Shows a tooltip in ImGui");
    m.def("begin_tooltip", &BeginTooltip, "Begins a tooltip in ImGui");
    m.def("end_tooltip", &EndTooltip, "Ends a tooltip in ImGui");
    m.def("log_to_clipboard", &ImGui_LogToClipboard, "Logs text to the clipboard in ImGui");

    // Tree Nodes
    m.def("tree_node", py::overload_cast<const std::string&>(&ImGui_TreeNode));
    m.def("tree_node", py::overload_cast<const std::string&, const std::string&>(&ImGui_TreeNode));
    m.def("tree_node_ex", py::overload_cast<const std::string&, ImGuiTreeNodeFlags>(&ImGui_TreeNodeEx));
    m.def("tree_node_ex", py::overload_cast<const std::string&, ImGuiTreeNodeFlags, const std::string&>(&ImGui_TreeNodeEx));

    m.def("tree_pop", &ImGui_TreePop);
    m.def("get_tree_node_to_label_spacing", &ImGui_GetTreeNodeToLabelSpacing);
    m.def("set_next_item_open", &ImGui_SetNextItemOpen, py::arg("is_open"), py::arg("cond") = 0);
	m.def("set_next_item_width", &ImGui_SetNextItemWidth, py::arg("item_width"));

    m.def("collapsing_header", py::overload_cast<const std::string&>(&ImGui_CollapsingHeader));
    m.def("collapsing_header", py::overload_cast<const std::string&, ImGuiTreeNodeFlags>(&ImGui_CollapsingHeader));

    //Dummy
	m.def("dummy", &Dummy, "Creates a dummy in ImGui");


}


