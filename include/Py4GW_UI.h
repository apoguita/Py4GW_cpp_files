#pragma once
#include "Headers.h"

#include <array>
#include <string>
#include <variant>
#include <vector>

enum class OpType : uint16_t {
    Begin,
    End,
    Text,
    TextColored,
    BulletText,
    Separator,
    Spacing,
    SameLine,
    Button,
    PushStyleColor,
    PopStyleColor,
    TextWrapped,
    RadioButton,
    CollapsingHeader,
    PushItemWidth,
    PopItemWidth,
    SetNextWindowSize,
    SetNextWindowPos,
    BeginTooltip,
    EndTooltip,
    TextDisabled,
    BeginDisabled,
    EndDisabled,
    BeginGroup,
    EndGroup,
    Indent,
    Unindent,
    SmallButton,
    MenuItem,
    OpenPopup,
    CloseCurrentPopup,
    BeginTable,
    EndTable,
    TableSetupColumn,
    TableNextColumn,
    TableNextRow,
    TableSetColumnIndex,
    TableHeadersRow,
    BeginChild,
    EndChild,
    BeginTabBar,
    EndTabBar,
    BeginTabItem,
    EndTabItem,
    BeginPopup,
    EndPopup,
    BeginPopupModal,
    EndPopupModal,
    BeginCombo,
    EndCombo,
    Selectable,
    NewLine,
    InvisibleButton,
    ProgressBar,
    CalcTextSize,
    GetContentRegionAvail,
    IsItemHovered,
    SetClipboardText,
    GetClipboardText,
    ColorEdit4,
    SetTooltip,
    ShowTooltip,
    IsWindowCollapsed,
    GetWindowPos,
    GetWindowSize,
    Checkbox,
    InputText,
    InputInt,
    InputFloat,
    SliderInt,
    SliderFloat,
    Combo,
    PythonCallable,
    Dummy,
    GetCursorScreenPos,
    SetCursorScreenPos,
    DrawListAddLine,
    DrawListAddRect,
    DrawListAddRectFilled,
    DrawListAddText,
    Count
};

// Shared signature pool: reusable payload shapes.
struct SigStr {
    std::string text;
};
struct SigStrColor {
    std::string text;
    std::array<float, 4> color = { 1.f, 1.f, 1.f, 1.f };
};

struct SigSameLine {
    float offset_from_start_x = 0.0f;
    float spacing = -1.0f;
};

struct SigButton {
    std::string text;
    std::string str_var;
    float width = 0.0f;
    float height = 0.0f;
    uint32_t slot = 0xFFFFFFFFu;
};

struct SigStyleColor {
    int idx = 0;
    std::array<float, 4> color = { 1.f, 1.f, 1.f, 1.f };
};

struct SigInt {
    int value = 0;
};

struct SigFloat {
    float value = 0.0f;
};

struct SigBool {
    bool value = false;
};

struct SigVec2Int {
    float x = 0.0f;
    float y = 0.0f;
    int value = 0;
};

struct SigVec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct SigDrawLine {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    int color = 0;
    float thickness = 1.0f;
};

struct SigDrawRect {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    int color = 0;
    float rounding = 0.0f;
    int rounding_corners_flags = 0;
    float thickness = 1.0f;
};

struct SigDrawText {
    float x = 0.0f;
    float y = 0.0f;
    int color = 0;
    std::string text;
};

struct SigStrIntFloat {
    std::string text;
    int value = 0;
    float value2 = 0.0f;
};
struct SigChild {
    std::string text;
    float width = 0.0f;
    float height = 0.0f;
    bool border = false;
    int flags = 0;
};

struct SigIntFloat {
    int value = 0;
    float value2 = 0.0f;
};

struct SigSelectable {
    std::string text;
    std::string str_var;
    int flags = 0;
    float width = 0.0f;
    float height = 0.0f;
    uint32_t slot = 0xFFFFFFFFu;
};

struct SigStrVar2 {
    std::string text;
    std::string str_var;
    std::string str_var2;
    uint32_t slot = 0xFFFFFFFFu;
    uint32_t slot2 = 0xFFFFFFFFu;
};

struct SigProgress {
    float fraction = 0.0f;
    float size_arg_x = -1.0f;
    float size_arg_y = 0.0f;
    bool has_y = false;
    std::string overlay;
};

struct SigStrVarInt {
    std::string text;
    std::string str_var;
    int value = 0;
    bool return_value = false;
    uint32_t slot = 0xFFFFFFFFu;
};

struct SigStrVarIntRange {
    std::string text;
    std::string str_var;
    int min_value = 0;
    int max_value = 0;
    bool return_value = false;
    uint32_t slot = 0xFFFFFFFFu;
};

struct SigStrVarFloatRange {
    std::string text;
    std::string str_var;
    float min_value = 0.0f;
    float max_value = 0.0f;
    bool return_value = false;
    uint32_t slot = 0xFFFFFFFFu;
};

struct SigStrVarItems {
    std::string text;
    std::string str_var;
    std::vector<std::string> items;
    bool return_value = false;
    uint32_t slot = 0xFFFFFFFFu;
};

struct SigCallable {
    py::object callable = py::none();
};

using CommandPayload = std::variant<
    std::monostate,
    SigStr,
    SigStrColor,
    SigSameLine,
    SigButton,
    SigStyleColor,
    SigInt,
    SigFloat,
    SigBool,
    SigVec2Int,
    SigVec2,
    SigDrawLine,
    SigDrawRect,
    SigDrawText,
    SigStrIntFloat,
    SigIntFloat,
    SigChild,
    SigSelectable,
    SigStrVar2,
    SigProgress,
    SigStrVarInt,
    SigStrVarIntRange,
    SigStrVarFloatRange,
    SigStrVarItems,
    SigCallable
>;

struct Command {
    OpType type;
    CommandPayload payload;
};


















