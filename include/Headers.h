#pragma once

#ifndef PCH_H
#define PCH_H

#define GAMEVARS_SIZE sizeof(MemPlayerStruct) 
#define SHM_NAME "GWHeroAISharedMemory"

enum SkillTarget { Enemy, EnemyCaster, EnemyMartial, Ally, AllyCaster, AllyMartial, OtherAlly, DeadAlly, Self, Corpse, Minion, Spirit, Pet, EnemyMartialMelee, EnemyMartialRanged, AllyMartialMelee, AllyMartialRanged };
enum SkillNature { Offensive, OffensiveCaster, OffensiveMartial, Enchantment_Removal, Healing, Hex_Removal, Condi_Cleanse, Buff, EnergyBuff, Neutral, SelfTargetted, Resurrection, Interrupt };
//enum WeaponType { bow = 1, axe, hammer, daggers, scythe, spear, sword, wand = 10, staff1 = 12, staff2 = 14 }; // 1=bow, 2=axe, 3=hammer, 4=daggers, 5=scythe, 6=spear, 7=sWORD, 10=wand, 12=staff, 14=staff};

enum WeaponType { None =0, bow = 1, axe=2, hammer=3, daggers=4, scythe =5, spear=6, sword=7, scepter =8,staff=9,staff2 = 10, scepter2 = 12, staff3=13,staff4 = 14 }; // 1=bow, 2=axe, 3=hammer, 4=daggers, 5=scythe, 6=spear, 7=sWORD, 10=wand, 12=staff, 14=staff};


enum RemoteCommand {rc_None, rc_ForceFollow,rc_Pcons, rc_Title, rc_GetQuest, rc_Resign, rc_Chest, rc_ID, rc_Salvage};

enum eState { Idle, Looting, Following, Scatter };
enum combatState { cIdle, cInCastingRoutine };



// Windows headersnice ide
#include <Windows.h>
#include <sysinfoapi.h>

// STL headers
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <regex>

// DirectX headers
#include <d3d9.h>

// ImGui headers
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"

#include <iomanip>  // For std::put_time
#include <ctime>    // For std::localtime and time_t
#include <sstream>
#include <fstream>  // For std::ifstream

#include "GuiUtils.h"
#include "IconsFontAwesome5.h"
#include <nlohmann/json.hpp>


//DLL External Functions


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

enum class SafeServerRegion {
    International,
    America,
    Korea,
    Europe,
    China,
    Japan,
    Unknown
};

enum class SafeLanguage {
    English,
    Korean,
    French,
    German,
    Italian,
    Spanish,
    TraditionalChinese,
    Japanese,
    Polish,
    Russian,
    BorkBorkBork,
    Unknown
};


enum class SafeCampaign {
    Core,
    Prophecies,
    Factions,
    Nightfall,
    EyeOfTheNorth,
    BonusMissionPack,
    Undefined
};

enum class SafeRegionType {
    AllianceBattle,
    Arena,
    ExplorableZone,
    GuildBattleArea,
    GuildHall,
    MissionOutpost,
    CooperativeMission,
    CompetitiveMission,
    EliteMission,
    Challenge,
    Outpost,
    ZaishenBattle,
    HeroesAscent,
    City,
    MissionArea,
    HeroBattleOutpost,
    HeroBattleArea,
    EotnMission,
    Dungeon,
    Marketplace,
    Unknown,
    DevRegion
};

enum class SafeContinent {
    Kryta,
    DevContinent,
    Cantha,
    BattleIsles,
    Elona,
    RealmOfTorment,
    Undefined
};

namespace GW {
    namespace Constants {
        enum class Rarity : uint8_t {
            White,
            Blue,
            Purple,
            Gold,
            Green
        };
        enum class Bag : uint8_t;
    }
    namespace UI {
        enum class UIMessage : uint32_t;
    }
}

enum class SalvageAllType : uint8_t {
    None,
    White,
    BlueAndLower,
    PurpleAndLower,
    GoldAndLower
};

enum class IdentifyAllType : uint8_t {
    None,
    All,
    Blue,
    Purple,
    Gold
};

enum class SafeItemType {
    Salvage,
    Axe,
    Bag,
    Boots,
    Bow,
    Bundle,
    Chestpiece,
    Rune_Mod,
    Usable,
    Dye,
    Materials_Zcoins,
    Offhand,
    Gloves,
    Hammer,
    Headpiece,
    CC_Shards,
    Key,
    Leggings,
    Gold_Coin,
    Quest_Item,
    Wand,
    Shield,
    Staff,
    Sword,
    Kit,
    Trophy,
    Scroll,
    Daggers,
    Present,
    Minipet,
    Scythe,
    Spear,
    Storybook,
    Costume,
    Costume_Headpiece,
    Unknown
};

enum class SafeDyeColor {
    NoColor,
    Blue,
    Green,
    Purple,
    Red,
    Yellow,
    Brown,
    Orange,
    Silver,
    Black,
    Gray,
    White,
    Pink
};

enum class ProfessionType {
    None,
    Warrior,
    Ranger,
    Monk,
    Necromancer,
    Mesmer,
    Elementalist,
    Assassin,
    Ritualist,
    Paragon,
    Dervish
};

enum class AllegianceType {
    Unknown,
    Ally,            // 0x1 = ally/non-attackable
    Neutral,         // 0x2 = neutral
    Enemy,           // 0x3 = enemy
    SpiritPet,       // 0x4 = spirit/pet
    Minion,          // 0x5 = minion
    NpcMinipet       // 0x6 = npc/minipet
};

enum class PyWeaponType {
    Unknown,            // 0 = unknown
    Bow,                // 1 = bow
    Axe,                // 2 = axe
    Hammer,             // 3 = hammer
    Daggers,            // 4 = daggers
    Scythe,             // 5 = scythe
    Spear,              // 6 = spear
    Sword,              // 7 = sword
    Scepter,            // 8 = scepter
    Scepter2,           // 9 = scepter
    Wand,               // 10 = wand
    Staff1,             // 11
    Staff,              // 12
    Staff2,             // 13
    Staff3,             // 14
    Unknown1,
    Unknown2,
    Unknown3,
    Unknown4,
    Unknown5,
    Unknown6,
    Unknown7,
    Unknown8,
    Unknown9,
    Unknown10
};

enum class SafeAttribute {
    FastCasting,
    IllusionMagic,
    DominationMagic,
    InspirationMagic,
    BloodMagic,
    DeathMagic,
    SoulReaping,
    Curses,
    AirMagic,
    EarthMagic,
    FireMagic,
    WaterMagic,
    EnergyStorage,
    HealingPrayers,
    SmitingPrayers,
    ProtectionPrayers,
    DivineFavor,
    Strength,
    AxeMastery,
    HammerMastery,
    Swordsmanship,
    Tactics,
    BeastMastery,
    Expertise,
    WildernessSurvival,
    Marksmanship,
    Unknown1,
    Unknown2,
    Unknown3,
    DaggerMastery,
    DeadlyArts,
    ShadowArts,
    Communing,
    RestorationMagic,
    ChannelingMagic,
    CriticalStrikes,
    SpawningPower,
    SpearMastery,
    Command,
    Motivation,
    Leadership,
    ScytheMastery,
    WindPrayers,
    EarthPrayers,
    Mysticism,
    None
};

enum eClickHandler { eHeroFlag };


enum class HeroType {
    None,
    Norgu,
    Goren,
    Tahlkora,
    MasterOfWhispers,
    AcolyteJin,
    Koss,
    Dunkoro,
    AcolyteSousuke,
    Melonni,
    ZhedShadowhoof,
    GeneralMorgahn,
    MagridTheSly,
    Zenmai,
    Olias,
    Razah,
    MOX,
    KeiranThackeray,
    Jora,
    PyreFierceshot,
    Anton,
    Livia,
    Hayda,
    Kahmu,
    Gwen,
    Xandra,
    Vekk,
    Ogden,
    MercenaryHero1,
    MercenaryHero2,
    MercenaryHero3,
    MercenaryHero4,
    MercenaryHero5,
    MercenaryHero6,
    MercenaryHero7,
    MercenaryHero8,
    Miku,
    ZeiRi
};

#include <d3d11.h>
#include <DirectXMath.h>
#include <future>
#include "IconsFontAwesome6.h"
#include <windows.h>
#include <string>
#include <iostream>
#include <codecvt>
#include <mutex>
#include <future>



// ----------   GW INCLUDES
#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Hook.h>

#include <GWCA/Utilities/Macros.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Pathing.h>

#include <GWCA/GameEntities/Attribute.h>

#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <GWCA/Managers/UIMgr.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>

#include <GWCA/Managers/MerchantMgr.h>
#include <GWCA/Managers/TradeMgr.h>

#include <GWCA/GameEntities/Player.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/GameEntities/Title.h>

#include <GWCA/GameEntities/Camera.h>
#include <GWCA/Managers/CameraMgr.h> 
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/RenderMgr.h>

#include <GWCA/Packets/StoC.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/ChatMgr.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/Managers/QuestMgr.h>

#include <GWCA/Managers/EffectMgr.h>


#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/GuildMgr.h>


#include <GWCA/GameEntities/Hero.h>

typedef float(__cdecl* ScreenToWorldPoint_pt)(GW::Vec3f* vec, float screen_x, float screen_y, int unk1);
extern ScreenToWorldPoint_pt ScreenToWorldPoint_Func;

typedef uint32_t(__cdecl* MapIntersect_pt)(GW::Vec3f* origin, GW::Vec3f* unit_direction, GW::Vec3f* hit_point, int* propLayer);
extern MapIntersect_pt MapIntersect_Func;

extern uintptr_t ptrBase_address;
extern uintptr_t ptrNdcScreenCoords;


namespace GW {
    typedef struct {
        GamePos pos;
        const PathingTrapezoid* t;
    } PathPoint;

    typedef void(__cdecl* FindPath_pt)(PathPoint* start, PathPoint* goal, float range, uint32_t maxCount, uint32_t* count, PathPoint* pathArray);
    static FindPath_pt FindPath_Func = nullptr;

    static void InitPathfinding() {
        //FindPath_Func = (FindPath_pt)GW::Scanner::Find("\x83\xec\x20\x53\x8b\x5d\x1c\x56\x57\xe8", "xxxxxxxxxx", -0x3);
        FindPath_Func = (FindPath_pt)GW::Scanner::Find("\x83\xec\x20\x53\x56\x57\xe8\x92\x8a\xdd", "xxxxxxxxxx", -0x3);
    }

    typedef void(__stdcall* UseHeroSkillInstant_t)(uint32_t hero_agent_id, uint32_t skill_slot, uint32_t target_id);

    static UseHeroSkillInstant_t UseHeroSkillInstant_Func = nullptr;


}



extern bool salvaging;
extern bool salvage_listeners_attached;
extern bool salvage_transaction_done;
extern GW::HookEntry salvage_hook_entry;
extern GW::Packet::StoC::SalvageSession current_salvage_session;

//hero stuff
enum FlaggingState : uint32_t {
    FlagState_All = 0,
    FlagState_Hero1,
    FlagState_Hero2,
    FlagState_Hero3,
    FlagState_Hero4,
    FlagState_Hero5,
    FlagState_Hero6,
    FlagState_Hero7,
    def_readonly
};

enum CaptureMouseClickType : uint32_t {
    CaptureType_None [[maybe_unused]] = 0,
    CaptureType_FlagHero [[maybe_unused]] = 1,
    CaptureType_SalvageWithUpgrades [[maybe_unused]] = 11,
    CaptureType_SalvageMaterials [[maybe_unused]] = 12,
    CaptureType_Idenfify [[maybe_unused]] = 13
};

struct MouseClickCaptureData {
    struct sub1 {
        uint8_t pad0[0x3C];

        struct sub2 {
            uint8_t pad1[0x14];

            struct sub3 {
                uint8_t pad2[0x24];

                struct sub4 {
                    uint8_t pad3[0x2C];

                    struct sub5 {
                        uint8_t pad4[0x4];
                        FlaggingState* flagging_hero;
                    } *sub5;
                } *sub4;
            } *sub3;
        } *sub2;
    } *sub1;
};

extern HWND gw_client_window_handle;

extern bool dll_shutdown;               // Flag to indicate when the DLL should close
extern std::string dllDirectory;       // Path to the directory containing the DLL

extern bool is_dragging;
extern bool is_dragging_imgui;
extern bool dragging_initialized;
extern bool global_mouse_clicked;

static GW::HookEntry Update_Entry;               // Hook for the update callback
static volatile bool running = false;             // Main loop control flag
static bool imgui_initialized = false;            // ImGui initialization state
static WNDPROC OldWndProc = nullptr;             // Original window procedure
static HWND gw_window_handle = nullptr;          // Guild Wars window handle

struct ClickHandlerStruct {
    bool WaitForClick = false;
    eClickHandler msg;
    int data;
    bool IsPartyWide = false;
};

// Declare the variable as extern
extern ClickHandlerStruct ClickHandler;

extern MouseClickCaptureData* MouseClickCaptureDataPtr;

extern uint32_t* GameCursorState;
extern CaptureMouseClickType* CaptureMouseClickTypePtr;

extern uint32_t quoted_item_id;
extern uint32_t quoted_value;  
extern bool transaction_complete; 

extern std::vector<uint32_t> merch_items;
extern std::vector<uint32_t> merch_items2;

extern std::vector<uint32_t> merchant_window_items;

extern std::string global_agent_name;
extern bool name_ready;

extern std::vector<std::string> global_chat_messages;  // Stores multiple decoded messages
extern bool chat_log_ready;  // Indicates if decoding is done

extern std::string global_item_name;
extern bool item_name_ready;



#include <pybind11/pybind11.h>
#include <pybind11/embed.h> 
#include <pybind11/stl.h>

#include "Ini_handler.h"
#include "SkillArray.h"
#include "Timer.h"
extern Timer reset_merchant_window_item;
extern Timer salvage_timer;

#include "Buffs.h"
#include "CandidateArray.h"
#include "MemMgrClass.h"


//manually added libs
#include "Logger.h"
#include <commdlg.h>
#include "py_ui.h"
#include "py_imgui.h"
#include "py_map.h"
#include "py_items.h"
#include "py_agent.h"
#include "py_player.h"
#include "py_party.h"
#include "py_skills.h"
#include "py_merchant.h"
#include "py_pinghandler.h"
#include "py_overlay.h"
#include "py_quest.h"

#include "SpecialSkilldata.h"
#include "HeroAI.h"





#endif // PCH_H







