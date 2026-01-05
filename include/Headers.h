#pragma once

#ifndef PCH_H
#define PCH_H

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
#include <d3dx9.h>

// ImGui headers
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"

#include <iomanip>  // For std::put_time
#include <ctime>    // For std::localtime and time_t
#include <sstream>
#include <fstream>  // For std::ifstream


#include "IconsFontAwesome5.h"
#include <nlohmann/json.hpp>


#include <d3d11.h>
#include <DirectXMath.h>
#include <future>
//#include "IconsFontAwesome6.h"
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
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/PreGameContext.h>
#include <GWCA/GameEntities/Pathing.h>

#include <GWCA/GameEntities/Attribute.h>

#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Context/AgentContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>

#include <GWCA/Managers/MerchantMgr.h>
#include <GWCA/Context/TradeContext.h>
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
#include <GWCA/Context/GameplayContext.h>
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
#include <GWCA/Logger/Logger.h>

#include <pybind11/pybind11.h>
#include <pybind11/embed.h> 
#include <pybind11/stl.h>

#include "Ini_handler.h"
#include "Timer.h"

#include <commdlg.h>

//DLL External Functions

//SkillArray enums
enum SkillTarget { Enemy, EnemyCaster, EnemyMartial, Ally, AllyCaster, AllyMartial, OtherAlly, DeadAlly, Self, Corpse, Minion, Spirit, Pet, EnemyMartialMelee, EnemyMartialRanged, AllyMartialMelee, AllyMartialRanged };
enum SkillNature { Offensive, OffensiveCaster, OffensiveMartial, Enchantment_Removal, Healing, Hex_Removal, Condi_Cleanse, Buff, EnergyBuff, Neutral, SelfTargetted, Resurrection, Interrupt };
enum WeaponType { None = 0, bow = 1, axe = 2, hammer = 3, daggers = 4, scythe = 5, spear = 6, sword = 7, scepter = 8, staff = 9, staff2 = 10, scepter2 = 12, staff3 = 13, staff4 = 14 }; // 1=bow, 2=axe, 3=hammer, 4=daggers, 5=scythe, 6=spear, 7=sWORD, 10=wand, 12=staff, 14=staff};

enum eClickHandler { eHeroFlag };

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


    typedef void(__stdcall* UseHeroSkillInstant_t)(uint32_t hero_agent_id, uint32_t skill_slot, uint32_t target_id);

    static UseHeroSkillInstant_t UseHeroSkillInstant_Func = nullptr;


}

extern bool salvaging;
extern bool salvage_listeners_attached;
extern bool salvage_transaction_done;
extern GW::HookEntry salvage_hook_entry;
extern GW::Packet::StoC::SalvageSession current_salvage_session;


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
extern int quoted_value;  
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

extern bool show_console;

extern IDirect3DDevice9* g_d3d_device;

extern std::chrono::steady_clock::time_point last_agent_array_update;

extern Timer reset_merchant_window_item;
extern Timer salvage_timer;

//manually added libs
#include "FontManager.h"
#include "py_imgui.h"
#include "SkillArray.h"
#include "ItemExtension.h"

#include "py_agent.h"
#include "py_items.h"
#include "py_inventory.h"
#include "py_map.h"
#include "py_missionmap.h"
#include "py_skills.h"
#include "py_skillbar.h"
#include "py_merchant.h"
#include "py_trading.h"

#include "TextureManager.h"
#include "py_overlay.h"
#include "py_camera.h"
#include "py_2d_renderer.h"
#include "py_pathing_maps.h"
#include "py_party.h"
#include "py_effects.h"



#include "py_ui.h"


#include "py_player.h"



#include "py_pinghandler.h"

#include "py_quest.h"

#include "py_combat_events.h"

#include "SpecialSkilldata.h"
//#include "HeroAI.h"





#endif // PCH_H







