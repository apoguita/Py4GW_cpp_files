#pragma once
#include "Headers.h"

namespace py = pybind11;

//All the bindings for ImGui are on the cpp file
//this file is just to include the header

enum class WindowFlags {
    None = ImGuiWindowFlags_None,
    NoTitleBar = ImGuiWindowFlags_NoTitleBar,
    NoResize = ImGuiWindowFlags_NoResize,
    NoMove = ImGuiWindowFlags_NoMove,
    NoScrollbar = ImGuiWindowFlags_NoScrollbar,
    NoScrollWithMouse = ImGuiWindowFlags_NoScrollWithMouse,
    NoCollapse = ImGuiWindowFlags_NoCollapse,
    AlwaysAutoResize = ImGuiWindowFlags_AlwaysAutoResize,
    NoBackground = ImGuiWindowFlags_NoBackground,
    NoSavedSettings = ImGuiWindowFlags_NoSavedSettings,
    NoMouseInputs = ImGuiWindowFlags_NoMouseInputs,
    MenuBar = ImGuiWindowFlags_MenuBar,
    HorizontalScrollbar = ImGuiWindowFlags_HorizontalScrollbar,
    NoFocusOnAppearing = ImGuiWindowFlags_NoFocusOnAppearing,
    NoBringToFrontOnFocus = ImGuiWindowFlags_NoBringToFrontOnFocus,
    AlwaysVerticalScrollbar = ImGuiWindowFlags_AlwaysVerticalScrollbar,
    AlwaysHorizontalScrollbar = ImGuiWindowFlags_AlwaysHorizontalScrollbar,
    //AlwaysUseWindowPadding = ImGuiWindowFlags_AlwaysUseWindowPadding,
    NoNavInputs = ImGuiWindowFlags_NoNavInputs,
    NoNavFocus = ImGuiWindowFlags_NoNavFocus,
    UnsavedDocument = ImGuiWindowFlags_UnsavedDocument,
};

enum class InputTextFlags {
    Nothing = ImGuiInputTextFlags_None,
    CharsDecimal = ImGuiInputTextFlags_CharsDecimal,
    CharsHexadecimal = ImGuiInputTextFlags_CharsHexadecimal,
    CharsUppercase = ImGuiInputTextFlags_CharsUppercase,
    CharsNoBlank = ImGuiInputTextFlags_CharsNoBlank,
    AutoSelectAll = ImGuiInputTextFlags_AutoSelectAll,
    EnterReturnsTrue = ImGuiInputTextFlags_EnterReturnsTrue,
    CallbackCompletion = ImGuiInputTextFlags_CallbackCompletion,
    CallbackHistory = ImGuiInputTextFlags_CallbackHistory,
    CallbackAlways = ImGuiInputTextFlags_CallbackAlways,
    CallbackCharFilter = ImGuiInputTextFlags_CallbackCharFilter,
    AllowTabInput = ImGuiInputTextFlags_AllowTabInput,
    CtrlEnterForNewLine = ImGuiInputTextFlags_CtrlEnterForNewLine,
    NoHorizontalScroll = ImGuiInputTextFlags_NoHorizontalScroll,
    //AlwaysInsertMode = ImGuiInputTextFlags_AlwaysInsertMode,
    ReadOnly = ImGuiInputTextFlags_ReadOnly,
    Password = ImGuiInputTextFlags_Password,
    NoUndoRedo = ImGuiInputTextFlags_NoUndoRedo,
    CharsScientific = ImGuiInputTextFlags_CharsScientific,
    CallbackResize = ImGuiInputTextFlags_CallbackResize,
    CallbackEdit = ImGuiInputTextFlags_CallbackEdit,
};

enum class TreeNodeFlags {
    Nothing = ImGuiTreeNodeFlags_None,
    Selected = ImGuiTreeNodeFlags_Selected,
    Framed = ImGuiTreeNodeFlags_Framed,
    //AllowItemOverlap = ImGuiTreeNodeFlags_AllowItemOverlap,
    NoTreePushOnOpen = ImGuiTreeNodeFlags_NoTreePushOnOpen,
    NoAutoOpenOnLog = ImGuiTreeNodeFlags_NoAutoOpenOnLog,
    DefaultOpen = ImGuiTreeNodeFlags_DefaultOpen,
    OpenOnDoubleClick = ImGuiTreeNodeFlags_OpenOnDoubleClick,
    OpenOnArrow = ImGuiTreeNodeFlags_OpenOnArrow,
    Leaf = ImGuiTreeNodeFlags_Leaf,
    Bullet = ImGuiTreeNodeFlags_Bullet,
    FramePadding = ImGuiTreeNodeFlags_FramePadding,
    SpanAvailWidth = ImGuiTreeNodeFlags_SpanAvailWidth,
    SpanFullWidth = ImGuiTreeNodeFlags_SpanFullWidth,
    NavLeftJumpsBackHere = ImGuiTreeNodeFlags_NavLeftJumpsBackHere,
    CollapsingHeader = ImGuiTreeNodeFlags_CollapsingHeader,
};

enum class SelectableFlags {
    Nothing = ImGuiSelectableFlags_None,
    DontClosePopups = ImGuiSelectableFlags_DontClosePopups,
    SpanAllColumns = ImGuiSelectableFlags_SpanAllColumns,
    AllowDoubleClick = ImGuiSelectableFlags_AllowDoubleClick,
    Disabled = ImGuiSelectableFlags_Disabled,
    //AllowItemOverlap = ImGuiSelectableFlags_AllowItemOverlap,
};

enum class TableFlags {
    Nothing = ImGuiTableFlags_None,
    Resizable = ImGuiTableFlags_Resizable,
    Reorderable = ImGuiTableFlags_Reorderable,
    Hideable = ImGuiTableFlags_Hideable,
    Sortable = ImGuiTableFlags_Sortable,
    NoSavedSettings = ImGuiTableFlags_NoSavedSettings,
    ContextMenuInBody = ImGuiTableFlags_ContextMenuInBody,
    RowBg = ImGuiTableFlags_RowBg,
    BordersInnerH = ImGuiTableFlags_BordersInnerH,
    BordersOuterH = ImGuiTableFlags_BordersOuterH,
    BordersInnerV = ImGuiTableFlags_BordersInnerV,
    BordersOuterV = ImGuiTableFlags_BordersOuterV,
    BordersH = ImGuiTableFlags_BordersH,
    BordersV = ImGuiTableFlags_BordersV,
    Borders = ImGuiTableFlags_Borders,
    NoBordersInBody = ImGuiTableFlags_NoBordersInBody,
    NoBordersInBodyUntilResize = ImGuiTableFlags_NoBordersInBodyUntilResize,
    SizingFixedFit = ImGuiTableFlags_SizingFixedFit,
    SizingFixedSame = ImGuiTableFlags_SizingFixedSame,
    SizingStretchProp = ImGuiTableFlags_SizingStretchProp,
    SizingStretchSame = ImGuiTableFlags_SizingStretchSame,
    NoHostExtendX = ImGuiTableFlags_NoHostExtendX,
    NoHostExtendY = ImGuiTableFlags_NoHostExtendY,
    NoKeepColumnsVisible = ImGuiTableFlags_NoKeepColumnsVisible,
    PreciseWidths = ImGuiTableFlags_PreciseWidths,
    NoClip = ImGuiTableFlags_NoClip,
    PadOuterX = ImGuiTableFlags_PadOuterX,
    NoPadOuterX = ImGuiTableFlags_NoPadOuterX,
    NoPadInnerX = ImGuiTableFlags_NoPadInnerX,
    ScrollX = ImGuiTableFlags_ScrollX,
    ScrollY = ImGuiTableFlags_ScrollY,
    SortMulti = ImGuiTableFlags_SortMulti,
    SortTristate = ImGuiTableFlags_SortTristate,
};


enum class TableColumnFlags {
    Nothing = ImGuiTableColumnFlags_None,
    DefaultHide = ImGuiTableColumnFlags_DefaultHide,
    DefaultSort = ImGuiTableColumnFlags_DefaultSort,
    WidthStretch = ImGuiTableColumnFlags_WidthStretch,
    WidthFixed = ImGuiTableColumnFlags_WidthFixed,
    NoResize = ImGuiTableColumnFlags_NoResize,
    NoReorder = ImGuiTableColumnFlags_NoReorder,
    NoHide = ImGuiTableColumnFlags_NoHide,
    NoClip = ImGuiTableColumnFlags_NoClip,
    NoSort = ImGuiTableColumnFlags_NoSort,
    NoSortAscending = ImGuiTableColumnFlags_NoSortAscending,
    NoSortDescending = ImGuiTableColumnFlags_NoSortDescending,
    IndentEnable = ImGuiTableColumnFlags_IndentEnable,
    IndentDisable = ImGuiTableColumnFlags_IndentDisable,
    IsEnabled = ImGuiTableColumnFlags_IsEnabled,
    IsVisible = ImGuiTableColumnFlags_IsVisible,
    IsSorted = ImGuiTableColumnFlags_IsSorted,
    IsHovered = ImGuiTableColumnFlags_IsHovered,
};

enum class TableRowFlags {
    Nothing = ImGuiTableRowFlags_None,
    Headers = ImGuiTableRowFlags_Headers,
};

enum class FocusedFlags {
    Nothing = ImGuiFocusedFlags_None,
    ChildWindows = ImGuiFocusedFlags_ChildWindows,
    RootWindow = ImGuiFocusedFlags_RootWindow,
    AnyWindow = ImGuiFocusedFlags_AnyWindow,
    RootAndChildWindows = ImGuiFocusedFlags_RootAndChildWindows,
};

enum class ImDrawFlagsWrapper {
	None = ImDrawFlags_None,
	Closed = ImDrawFlags_Closed,
	RoundCornersTopLeft = ImDrawFlags_RoundCornersTopLeft,
	RoundCornersTopRight = ImDrawFlags_RoundCornersTopRight,
	RoundCornersBottomLeft = ImDrawFlags_RoundCornersBottomLeft,
	RoundCornersBottomRight = ImDrawFlags_RoundCornersBottomRight,
	RoundCornersNone = ImDrawFlags_RoundCornersNone,
	RoundCornersTop = ImDrawFlags_RoundCornersTop,
	RoundCornersBottom = ImDrawFlags_RoundCornersBottom,
	RoundCornersLeft = ImDrawFlags_RoundCornersLeft,
	RoundCornersRight = ImDrawFlags_RoundCornersRight,
	RoundCornersAll = ImDrawFlags_RoundCornersAll,
	RoundCornersDefault = ImDrawFlags_RoundCornersAll
};

enum class HoveredFlags {
    Nothing = ImGuiHoveredFlags_None,
    ChildWindows = ImGuiHoveredFlags_ChildWindows,
    RootWindow = ImGuiHoveredFlags_RootWindow,
    AnyWindow = ImGuiHoveredFlags_AnyWindow,
    AllowWhenBlockedByPopup = ImGuiHoveredFlags_AllowWhenBlockedByPopup,
    AllowWhenBlockedByActiveItem = ImGuiHoveredFlags_AllowWhenBlockedByActiveItem,
    AllowWhenOverlapped = ImGuiHoveredFlags_AllowWhenOverlapped,
    AllowWhenDisabled = ImGuiHoveredFlags_AllowWhenDisabled,
};

// Define a wrapper enum class for ImGuiCol
enum class ImGuiColWrapper {
    Text = ImGuiCol_Text,
    TextDisabled = ImGuiCol_TextDisabled,
    WindowBg = ImGuiCol_WindowBg,
    ChildBg = ImGuiCol_ChildBg,
    PopupBg = ImGuiCol_PopupBg,
    Border = ImGuiCol_Border,
    BorderShadow = ImGuiCol_BorderShadow,
    FrameBg = ImGuiCol_FrameBg,
    FrameBgHovered = ImGuiCol_FrameBgHovered,
    FrameBgActive = ImGuiCol_FrameBgActive,
    TitleBg = ImGuiCol_TitleBg,
    TitleBgActive = ImGuiCol_TitleBgActive,
    TitleBgCollapsed = ImGuiCol_TitleBgCollapsed,
    MenuBarBg = ImGuiCol_MenuBarBg,
    ScrollbarBg = ImGuiCol_ScrollbarBg,
    ScrollbarGrab = ImGuiCol_ScrollbarGrab,
    ScrollbarGrabHovered = ImGuiCol_ScrollbarGrabHovered,
    ScrollbarGrabActive = ImGuiCol_ScrollbarGrabActive,
    CheckMark = ImGuiCol_CheckMark,
    SliderGrab = ImGuiCol_SliderGrab,
    SliderGrabActive = ImGuiCol_SliderGrabActive,
    Button = ImGuiCol_Button,
    ButtonHovered = ImGuiCol_ButtonHovered,
    ButtonActive = ImGuiCol_ButtonActive,
    Header = ImGuiCol_Header,
    HeaderHovered = ImGuiCol_HeaderHovered,
    HeaderActive = ImGuiCol_HeaderActive,
    Separator = ImGuiCol_Separator,
    SeparatorHovered = ImGuiCol_SeparatorHovered,
    SeparatorActive = ImGuiCol_SeparatorActive,
    ResizeGrip = ImGuiCol_ResizeGrip,
    ResizeGripHovered = ImGuiCol_ResizeGripHovered,
    ResizeGripActive = ImGuiCol_ResizeGripActive,
    Tab = ImGuiCol_Tab,
    /*
    TabSelected = ImGuiCol_TabSelected,
    TabSelectedOverline = ImGuiCol_TabSelectedOverline,
    TabDimmed =ImGuiCol_TabDimmed,
    TabDimmedSelected = ImGuiCol_TabDimmedSelected,
    TabDimmedSelectedOverline = ImGuiCol_TabDimmedSelectedOverline,
    DockingPreview = ImGuiCol_DockingPreview,
    DockingEmptyBg = ImGuiCol_DockingEmptyBg,
    */
    TabHovered = ImGuiCol_TabHovered,
    TabActive = ImGuiCol_TabActive,
    TabUnfocused = ImGuiCol_TabUnfocused,
    TabUnfocusedActive = ImGuiCol_TabUnfocusedActive,
    PlotLines = ImGuiCol_PlotLines,
    PlotLinesHovered = ImGuiCol_PlotLinesHovered,
    PlotHistogram = ImGuiCol_PlotHistogram,
    PlotHistogramHovered = ImGuiCol_PlotHistogramHovered,
    TableHeaderBg = ImGuiCol_TableHeaderBg,
    TableBorderStrong = ImGuiCol_TableBorderStrong,
    TableBorderLight = ImGuiCol_TableBorderLight,
    TableRowBg = ImGuiCol_TableRowBg,
    TableRowBgAlt = ImGuiCol_TableRowBgAlt,
    TextSelectedBg = ImGuiCol_TextSelectedBg,
    DragDropTarget = ImGuiCol_DragDropTarget,
    NavHighlight = ImGuiCol_NavHighlight,
    NavWindowingHighlight = ImGuiCol_NavWindowingHighlight,
    NavWindowingDimBg = ImGuiCol_NavWindowingDimBg,
    ModalWindowDimBg = ImGuiCol_ModalWindowDimBg
};




// Bitwise OR operator for WindowFlags enum class
inline WindowFlags operator|(WindowFlags lhs, WindowFlags rhs) {
    using T = std::underlying_type_t<WindowFlags>;
    return static_cast<WindowFlags>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

// Bitwise AND operator for completeness (if needed)
inline WindowFlags operator&(WindowFlags lhs, WindowFlags rhs) {
    using T = std::underlying_type_t<WindowFlags>;
    return static_cast<WindowFlags>(static_cast<T>(lhs) & static_cast<T>(rhs));
}
    

// Bitwise OR operator for TableFlags enum class
inline TableFlags operator|(TableFlags lhs, TableFlags rhs) {
    using T = std::underlying_type_t<TableFlags>;
    return static_cast<TableFlags>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

// Bitwise AND operator for completeness (if needed)
inline TableFlags operator&(TableFlags lhs, TableFlags rhs) {
    using T = std::underlying_type_t<TableFlags>;
    return static_cast<TableFlags>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

inline ImDrawFlagsWrapper operator|(ImDrawFlagsWrapper lhs, ImDrawFlagsWrapper rhs) {
	using T = std::underlying_type_t<ImDrawFlagsWrapper>;
	return static_cast<ImDrawFlagsWrapper>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

inline ImDrawFlagsWrapper operator&(ImDrawFlagsWrapper lhs, ImDrawFlagsWrapper rhs) {
	using T = std::underlying_type_t<ImDrawFlagsWrapper>;
	return static_cast<ImDrawFlagsWrapper>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

