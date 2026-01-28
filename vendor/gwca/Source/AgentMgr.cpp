#include "stdafx.h"

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/AgentContext.h>
#include <GWCA/Context/GadgetContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Logger/Logger.h>

namespace {
    using namespace GW;

    uint32_t dialog_agent_id = 0;

    typedef void (*SendDialog_pt)(uint32_t dialog_id);
    SendDialog_pt SendAgentDialog_Func = 0, SendAgentDialog_Ret = 0;
    SendDialog_pt SendGadgetDialog_Func = 0, SendGadgetDialog_Ret = 0;

    void OnSendAgentDialog_Func(uint32_t dialog_id) {
        GW::Hook::EnterHook();
        UI::SendUIMessage(UI::UIMessage::kSendAgentDialog, (void*)dialog_id);
        GW::Hook::LeaveHook();
    };
    void OnSendGadgetDialog_Func(uint32_t dialog_id) {
        GW::Hook::EnterHook();
        UI::SendUIMessage(UI::UIMessage::kSendGadgetDialog, (void*)dialog_id);
        GW::Hook::LeaveHook();
    };

    typedef void(*ChangeTarget_pt)(uint32_t agent_id, uint32_t auto_target_id);
    ChangeTarget_pt ChangeTarget_Func = 0, ChangeTarget_Ret = 0;

    uint32_t current_target_id = 0;

    typedef void(*CallTarget_pt)(CallTargetType type, uint32_t agent_id);
    CallTarget_pt CallTarget_Func = 0, CallTarget_Ret = 0;

    void OnChangeTarget_Func(uint32_t agent_id, uint32_t auto_target_id) {
        GW::Hook::EnterHook();
        UI::UIPacket::kSendChangeTarget packet = { agent_id, auto_target_id };
        UI::SendUIMessage(UI::UIMessage::kSendChangeTarget, &packet);
        GW::Hook::LeaveHook();
    };

    void OnCallTarget_Func(CallTargetType type, uint32_t target_id) {
        GW::Hook::EnterHook();
        UI::UIPacket::kSendCallTarget packet = { type, target_id };
        UI::SendUIMessage(UI::UIMessage::kSendCallTarget, &packet);
        GW::Hook::LeaveHook();
    };

    enum class InteractionActionType : uint32_t {
        Enemy,
        Player,
        NPC,
        Item,
        Follow,
        Gadget
    };

    typedef void(*MoveTo_pt)(float* pos);
    MoveTo_pt MoveTo_Func = 0, MoveTo_Ret = 0;

    typedef void(*DoWorldActon_pt)(WorldActionId action_id, uint32_t agent_id, bool suppress_call_target);
    DoWorldActon_pt DoWorldActon_Func = 0, DoWorldActon_Ret = 0;

    void OnDoWorldActon_Func(WorldActionId action_id, uint32_t agent_id, bool suppress_call_target) {
        GW::Hook::EnterHook();
        UI::UIPacket::kSendWorldAction packet = {
            action_id, agent_id, suppress_call_target
        };
        UI::SendUIMessage(UI::UIMessage::kSendWorldAction, &packet);
        GW::Hook::LeaveHook();
    }

    uintptr_t AgentArrayPtr = 0;
    uintptr_t PlayerAgentIdPtr = 0;
    uintptr_t IsAutoRunningPtr = 0;

    AgentList *AgentListPtr = nullptr;

    HookEntry UIMessage_Entry;
    constexpr std::array ui_messages_to_hook = {
        UI::UIMessage::kDialogBody,
        UI::UIMessage::kSendAgentDialog,
        UI::UIMessage::kSendGadgetDialog,
        UI::UIMessage::kSendMoveToWorldPoint,
        UI::UIMessage::kSendCallTarget,
        UI::UIMessage::kSendInteractGadget,
        UI::UIMessage::kSendInteractItem,
        UI::UIMessage::kSendInteractNPC,
        UI::UIMessage::kSendInteractPlayer,
        UI::UIMessage::kSendInteractEnemy,
        UI::UIMessage::kSendChangeTarget,
        UI::UIMessage::kChangeTarget,
        UI::UIMessage::kSendWorldAction
    };
    void OnUIMessage(GW::HookStatus* status, UI::UIMessage message_id, void* wparam, void*) {
        if (status->blocked)
            return;
        switch (message_id) {
        case UI::UIMessage::kDialogBody: {
            const auto packet = static_cast<UI::DialogBodyInfo*>(wparam);
            dialog_agent_id = packet->agent_id;
        } break;
        case UI::UIMessage::kSendAgentDialog: {
            if (SendAgentDialog_Ret) {
                SendAgentDialog_Ret((uint32_t)wparam);
            }
        } break;
        case UI::UIMessage::kSendChangeTarget: {
            if (ChangeTarget_Ret) {
                const auto packet = static_cast<UI::UIPacket::kSendChangeTarget*>(wparam);
                ChangeTarget_Ret(packet->target_id, packet->auto_target_id);
            }
        } break;
        case UI::UIMessage::kSendGadgetDialog: {
            if (SendGadgetDialog_Ret) {
                SendGadgetDialog_Ret((uint32_t)wparam);
            }
        } break;
        case UI::UIMessage::kChangeTarget: {
            const auto msg = static_cast<GW::UI::ChangeTargetUIMsg*>(wparam);
            current_target_id = msg->manual_target_id;
        } break;
        case UI::UIMessage::kSendCallTarget: {
            if (CallTarget_Ret) {
                const auto packet = static_cast<UI::UIPacket::kSendCallTarget*>(wparam);
                CallTarget_Ret(packet->call_type, packet->agent_id);
            }
        } break;
        case UI::UIMessage::kSendWorldAction: {
            if (DoWorldActon_Ret) {
                const auto msg = static_cast<UI::UIPacket::kSendWorldAction*>(wparam);
                DoWorldActon_Ret(msg->action_id, msg->agent_id, msg->suppress_call_target);
            }
        } break;
        }
    }

    void Init() {

        //Logger::Instance().LogInfo("############ AgentMgr initialization started ############");

        uintptr_t address = 0;

        ChangeTarget_Func = (ChangeTarget_pt)GW::Scanner::ToFunctionStart(Scanner::FindAssertion("AvSelect.cpp", "!(autoAgentId && !ManagerFindAgent(autoAgentId))",0,0));

        address = Scanner::Find("\x8b\x0c\x90\x85\xc9\x74\x19", "xxxxxxx", -0x4);
        if (address && Scanner::IsValidPtr(*(uintptr_t*)address))
            AgentArrayPtr = *(uintptr_t*)address;

        //address = Scanner::Find("\x5D\xE9\x00\x00\x00\x00\x55\x8B\xEC\x53","xx????xxxx", -0xE);
        address = Scanner::Find("\x5d\xe9\x00\x00\x00\x00\x55\x8b\xec\x56\x57\x8b\x7d", "xx????xxxxxxx", -0xE);
        if (address && Scanner::IsValidPtr(*(uintptr_t*)address))
            PlayerAgentIdPtr = *(uintptr_t*)address;	

        // @Cleanup: try to do this via UI controls to avoid more signature scans
        address = Scanner::Find("\x89\x4b\x24\x8b\x4b\x28\x83\xe9\x00", "xxxxxxxxx");
        if (Scanner::IsValidPtr(address,ScannerSection::Section_TEXT)) {
            SendAgentDialog_Func = (SendDialog_pt)Scanner::FunctionFromNearCall(address + 0x15);
            SendGadgetDialog_Func = (SendDialog_pt)Scanner::FunctionFromNearCall(address + 0x25);
        }

        address = Scanner::Find("\x83\xc4\x0c\x85\xff\x74\x0b\x56\x6a\x03", "xxxxxxxxxx", -0x5);
        MoveTo_Func = (MoveTo_pt)Scanner::FunctionFromNearCall(address);

        DoWorldActon_Func = (DoWorldActon_pt)GW::Scanner::ToFunctionStart(Scanner::FindAssertion("GmCoreAction.cpp", "action < WORLD_ACTIONS",0,0)); // This hits twice, but we want the first function

        int success = HookBase::CreateHook((void**)&DoWorldActon_Func, OnDoWorldActon_Func, (void**)&DoWorldActon_Ret);
        Logger::AssertHook("DoWorldActon_Func", success);

        // NOTE: CallTarget_Func initialization is currently missing - hook is disabled
        // The CallTarget functionality still works through UI messages (kSendCallTarget)
        // TODO: Add Scanner::Find for CallTarget_Func if direct hooking is needed
        if (CallTarget_Func) {
            success = HookBase::CreateHook((void**)&CallTarget_Func, OnCallTarget_Func, (void**)&CallTarget_Ret);
            Logger::AssertHook("CallTarget_Func", success);
        }

        success = HookBase::CreateHook((void**)&SendAgentDialog_Func, OnSendAgentDialog_Func, (void**)&SendAgentDialog_Ret);
		Logger::AssertHook("SendAgentDialog_Func", success);
        success = HookBase::CreateHook((void**)&SendGadgetDialog_Func, OnSendGadgetDialog_Func, (void**)&SendGadgetDialog_Ret);
		Logger::AssertHook("SendGadgetDialog_Func", success);
        success = HookBase::CreateHook((void**)&ChangeTarget_Func, OnChangeTarget_Func, (void**)&ChangeTarget_Ret);
		Logger::AssertHook("ChangeTarget_Func", success);

        GWCA_INFO("[SCAN] AgentArrayPtr = %p", AgentArrayPtr);
        GWCA_INFO("[SCAN] PlayerAgentIdPtr = %p", PlayerAgentIdPtr);
        GWCA_INFO("[SCAN] MoveTo_Func = %p", MoveTo_Func);
        GWCA_INFO("[SCAN] ChangeTargetFunction = %p", ChangeTarget_Func);
        GWCA_INFO("[SCAN] SendAgentDialog_Func = %p", SendAgentDialog_Func);
        GWCA_INFO("[SCAN] SendGadgetDialog_Func = %p", SendGadgetDialog_Func);


        Logger::AssertAddress("AgentArrayPtr", (uintptr_t)AgentArrayPtr, "Agent Module");
        Logger::AssertAddress("PlayerAgentIdPtr", (uintptr_t)PlayerAgentIdPtr, "Agent Module");
		Logger::AssertAddress("MoveTo_Func", (uintptr_t)MoveTo_Func, "Agent Module");
		Logger::AssertAddress("ChangeTarget_Func", (uintptr_t)ChangeTarget_Func, "Agent Module");
		Logger::AssertAddress("SendAgentDialog_Func", (uintptr_t)SendAgentDialog_Func, "Agent Module");
		Logger::AssertAddress("SendGadgetDialog_Func", (uintptr_t)SendGadgetDialog_Func, "Agent Module");

        //Logger::Instance().LogInfo("############ AgentMgr initialization complete ############");


    }
    void EnableHooks() {
        //return; // Temporarily disable gamethread hooks to investigate issues
        if (CallTarget_Func)
            HookBase::EnableHooks(CallTarget_Func);
        if (DoWorldActon_Func)
            HookBase::EnableHooks(DoWorldActon_Func);
        if (SendAgentDialog_Func)
            HookBase::EnableHooks(SendAgentDialog_Func);
        if (SendGadgetDialog_Func)
            HookBase::EnableHooks(SendGadgetDialog_Func);

        for (auto ui_message : ui_messages_to_hook) {
            UI::RegisterUIMessageCallback(&UIMessage_Entry, ui_message, OnUIMessage, 0x1);
        }
    }
    void DisableHooks() {
        if (CallTarget_Func)
            HookBase::DisableHooks(CallTarget_Func);
        if (DoWorldActon_Func)
            HookBase::DisableHooks(DoWorldActon_Func);
        if (SendAgentDialog_Func)
            HookBase::DisableHooks(SendAgentDialog_Func);
        if (SendGadgetDialog_Func)
            HookBase::DisableHooks(SendGadgetDialog_Func);

        UI::RemoveUIMessageCallback(&UIMessage_Entry);
    }
    void Exit() {
        if (CallTarget_Func)
            HookBase::RemoveHook(CallTarget_Func);
        if (DoWorldActon_Func)
            HookBase::RemoveHook(DoWorldActon_Func);
        if (SendAgentDialog_Func)
            HookBase::RemoveHook(SendAgentDialog_Func);
        if (SendGadgetDialog_Func)
            HookBase::RemoveHook(SendGadgetDialog_Func);
    }
}

namespace GW {

    Module AgentModule = {
        "AgentModule",      // name
        NULL,               // param
        ::Init,             // init_module
        ::Exit,             // exit_module
        ::EnableHooks,               // enable_hooks
        ::DisableHooks,               // disable_hooks
    };

    namespace Agents {

        bool GetIsAgentTargettable(const GW::Agent* agent) {
            if (!agent) return false;
            if (const auto living = agent->GetAsAgentLiving()) {
                if (living->IsPlayer())
                    return true;
                const GW::NPC* npc = GW::Agents::GetNPCByID(living->player_number);
                if (npc && (npc->npc_flags & 0x10000) == 0)
                    return true;
            }
            else if (const auto gadget = agent->GetAsAgentGadget()) {
                if (GetAgentEncName(gadget))
                    return true;
            }
            return false;
        }
        bool SendDialog(uint32_t dialog_id) {
            const auto a = GW::Agents::GetAgentByID(dialog_agent_id);
            if (!a) return false;
            if (a->GetIsGadgetType()) {
                return UI::SendUIMessage(UI::UIMessage::kSendGadgetDialog, (void*)dialog_id);
            }
            else {
                return UI::SendUIMessage(UI::UIMessage::kSendAgentDialog, (void*)dialog_id);
            }
        }

        AgentArray* GetAgentArray() {
            auto* agents = (AgentArray*)AgentArrayPtr;
            return agents && agents->valid() ? agents : nullptr;
        }
        uint32_t GetControlledCharacterId() {
            const auto w = GetWorldContext();
            return w && w->playerControlledChar ? w->playerControlledChar->agent_id : 0;
        }
        uint32_t GetObservingId() {
            return *(uint32_t*)PlayerAgentIdPtr;
        }
        uint32_t GetTargetId() {
            return current_target_id;
        }

        bool ChangeTarget(AgentID agent_id) {
            UI::UIPacket::kSendChangeTarget packet = { agent_id, 0 };
            return UI::SendUIMessage(UI::UIMessage::kSendChangeTarget, &packet);
        }

        bool ChangeTarget(const Agent* agent) {
            return agent ? ChangeTarget(agent->agent_id) : false;
        }

        bool Move(float x, float y, uint32_t zplane /*= 0*/) {
            GamePos pos;
            pos.x = x;
            pos.y = y;
            pos.zplane = zplane;
            return Move(pos);
        }

        bool Move(GamePos pos) {
            if (!MoveTo_Func)
                return false;
            float arg[4] = { .0f }; // idk 4th float
            arg[0] = pos.x;
            arg[1] = pos.y;
            arg[2] = (float)pos.zplane;
            MoveTo_Func(arg);
            return true;
        }
        uint32_t GetAmountOfPlayersInInstance() {
            auto* w = GetWorldContext();
            // -1 because the 1st array element is nil
            return w && w->players.valid() ? w->players.size() - 1 : 0;
        }

        MapAgentArray* GetMapAgentArray() {
            auto* w = GetWorldContext();
            return w ? &w->map_agents : nullptr;
        }

        MapAgent* GetMapAgentByID(uint32_t agent_id) {
            auto* agents = agent_id ? GetMapAgentArray() : nullptr;
            return agents && agent_id < agents->size() ? &agents->at(agent_id) : nullptr;
        }

        Agent* GetAgentByID(uint32_t agent_id) {
            /*old code*/
            //auto* agents = agent_id ? GetAgentArray() : nullptr;
            //return agents && agent_id < agents->size() ? agents->at(agent_id) : nullptr;

            auto* agents = agent_id ? GetAgentArray() : nullptr;
            Agent* agent = agents && agent_id < agents->size() ? agents->at(agent_id) : nullptr;
            if (!agent) return nullptr;

            const auto ac = GW::GetAgentContext();
            if (!(ac->agent_movement.size() > agent->agent_id &&
                ac->agent_movement[agent->agent_id]))
                return nullptr;

            return agent;

        }

        AgentLiving* GetAgentLivingByID(uint32_t agent_id) { //Reimplement; this was removed from GWCA
            auto* agents = agent_id ? GetAgentArray() : nullptr;
            if (agents && agent_id < agents->size()) {
                Agent* a = agents->at(agent_id);
                if (a && a->GetIsLivingType()) {
                    return a->GetAsAgentLiving();
                }
            }
            return nullptr;
        }

        Agent* GetPlayerByID(uint32_t player_id) {
            return GetAgentByID(PlayerMgr::GetPlayerAgentId(player_id));
        }

        AgentLiving* GetControlledCharacter() {
            const auto a = GetAgentByID(GetControlledCharacterId());
            return a ? a->GetAsAgentLiving() : nullptr;
        }

        bool IsObserving() {
            return !GW::Map::GetIsObserving() && GetControlledCharacterId() != GetObservingId();
        }

        AgentLiving* GetTargetAsAgentLiving()
        {
            Agent* a = GetTarget();
            return a ? a->GetAsAgentLiving() : nullptr;
        }

        bool InteractAgent(const Agent* agent, bool call_target) {
            if (!agent)
                return false;
            UI::UIPacket::kSendWorldAction packet = { WorldActionId::InteractEnemy, agent->agent_id, call_target };
            if (agent->GetIsItemType()) {
                packet.action_id = WorldActionId::InteractItem;
                return UI::SendUIMessage(UI::UIMessage::kSendWorldAction, &packet);
            }
            if (agent->GetIsGadgetType()) {
                packet.action_id = WorldActionId::InteractGadget;
                return UI::SendUIMessage(UI::UIMessage::kSendWorldAction, &packet);
            }
            const auto living = agent->GetAsAgentLiving();
            if (!living)
                return false;
            if (living->allegiance == GW::Constants::Allegiance::Enemy) {
                packet.action_id = WorldActionId::InteractEnemy;
                return UI::SendUIMessage(UI::UIMessage::kSendWorldAction, &packet);
            }
            if (living->allegiance == GW::Constants::Allegiance::Npc_Minipet) {
                packet.action_id = WorldActionId::InteractNPC;
                return UI::SendUIMessage(UI::UIMessage::kSendWorldAction, &packet);
            }
            packet.action_id = WorldActionId::InteractPlayerOrOther;
            return UI::SendUIMessage(UI::UIMessage::kSendWorldAction, &packet);
        }


        bool CallTarget(const AgentLiving* agent) {
            if (!agent)
                return false;

            if (agent->allegiance == GW::Constants::Allegiance::Enemy) {
                const auto* target = GW::Agents::GetTarget();
                if (!target)
                    return false;
                UI::UIPacket::kSendCallTarget call_packet = UI::UIPacket::kSendCallTarget{
                    GW::CallTargetType::AttackingOrTargetting,
                    target->agent_id
                };
                return GW::UI::SendUIMessage(GW::UI::UIMessage::kSendCallTarget, &call_packet);
            }
            UI::UIPacket::kSendWorldAction packet = { WorldActionId::InteractPlayerOrOther, agent->agent_id, true };
            return UI::SendUIMessage(UI::UIMessage::kSendWorldAction, &packet);
        }

        bool CallTarget(uint32_t agent_id) {
            return CallTarget(GW::Agents::GetAgentLivingByID(agent_id));
        }

        wchar_t* GetPlayerNameByLoginNumber(uint32_t login_number) {
            return PlayerMgr::GetPlayerName(login_number);
        }

        uint32_t GetAgentIdByLoginNumber(uint32_t login_number) {
            auto* player = PlayerMgr::GetPlayerByID(login_number);
            return player ? player->agent_id : 0;
        }

        uint32_t GetHeroAgentID(uint32_t hero_index) {
            return PartyMgr::GetHeroAgentID(hero_index);
        }

        uint32_t GetHenchmanAgentId(uint32_t henchmanId) {
            NPCArray* npcs = GetNPCArray();
            if (npcs) {
                uint32_t agentId = 0;
                for (uint32_t i = 0; i < npcs->size(); ++i) {
                    NPC npc = (*npcs)[i];
                    if (npc.model_file_id == 0) continue;
                    agentId++;
                    if (henchmanId == i) return agentId;
                }
            }
            return 0;
        }

        bool CancelAction() {
            Agent* player = GetControlledCharacter();
            if (player)
            {
                return UI::Keypress(UI::ControlAction_CancelAction);
            }
            return false;
        }


        PlayerArray* GetPlayerArray() {
            auto* w = GetWorldContext();
            return w && w->players.valid() ? &w->players : nullptr;
        }

        NPCArray* GetNPCArray() {
            auto* w = GetWorldContext();
            return w && w->npcs.valid() ? &w->npcs : nullptr;
        }

        NPC* GetNPCByID(uint32_t npc_id) {
            auto* npcs = GetNPCArray();
            return npcs && npc_id < npcs->size() ? &npcs->at(npc_id) : nullptr;
        }

        wchar_t* GetAgentEncName(uint32_t agent_id) {
            const Agent* agent = GetAgentByID(agent_id);
            if (agent) {
                return GetAgentEncName(agent);
            }
            GW::AgentInfoArray& agent_infos = GetWorldContext()->agent_infos;
            if (!agent_infos.valid() || agent_id >= agent_infos.size()) {
                return nullptr;
            }
            return agent_infos[agent_id].name_enc;
        }

        wchar_t* GetAgentEncName(const Agent* agent) {
            if (!agent)
                return nullptr;
            if (agent->GetIsLivingType()) {
                const AgentLiving* ag = agent->GetAsAgentLiving();
                if (ag->login_number) {
                    PlayerArray* players = GetPlayerArray();
                    if (!players)
                        return nullptr;
                    Player* player = &players->at(ag->login_number);
                    if (player)
                        return player->name_enc;
                }
                // @Remark:
                // For living npcs it's not elegant, but the game does it as well. See arround GetLivingName(AgentID id)@007C2A00.
                // It first look in the AgentInfo arrays, if it doesn't find it, it does a bunch a shit and fallback on NPCArray.
                // If we only use NPCArray, we have a problem because 2 agents can share the same PlayerNumber.
                // In Isle of Nameless, few npcs (Zaischen Weapond Collector) share the PlayerNumber with "The Guide" so using NPCArray only won't work.
                // But, the dummies (Suit of xx Armor) don't have there NameString in AgentInfo array, so we need NPCArray.
                Array<AgentInfo>& agent_infos = GetWorldContext()->agent_infos;
                if (ag->agent_id >= agent_infos.size()) return nullptr;
                if (agent_infos[ag->agent_id].name_enc)
                    return agent_infos[ag->agent_id].name_enc;
                NPC* npc = GetNPCByID(ag->player_number);
                return npc ? npc->name_enc : nullptr;
            }
            if (agent->GetIsGadgetType()) {
                AgentContext* ctx = GetAgentContext();
                GadgetContext* gadget = GetGameContext()->gadget;
                if (!ctx || !gadget) return nullptr;
                auto* GadgetIds = ctx->agent_summary_info[agent->agent_id].extra_info_sub;
                if (!GadgetIds)
                    return nullptr;
                if (GadgetIds->gadget_name_enc)
                    return GadgetIds->gadget_name_enc;
                size_t id = GadgetIds->gadget_id;
                if (gadget->GadgetInfo.size() <= id) return nullptr;
                if (gadget->GadgetInfo[id].name_enc)
                    return gadget->GadgetInfo[id].name_enc;
                return nullptr;
            }
            if (agent->GetIsItemType()) {
                const AgentItem* ag = agent->GetAsAgentItem();
                Item* item = Items::GetItemById(ag->item_id);
                return item ? item->name_enc : nullptr;
            }
            return nullptr;
        }

        bool AsyncGetAgentName(const Agent* agent, std::wstring& res) {
            wchar_t* str = GetAgentEncName(agent);
            if (!str) return false;
            UI::AsyncDecodeStr(str, &res);
            return true;
        }


		bool AsyncDecodeStr(wchar_t* enc_str, std::wstring& res) {
            if (!enc_str) return false;
            UI::AsyncDecodeStr(enc_str, &res);
            return true;	
		}
    }
} // namespace GW
