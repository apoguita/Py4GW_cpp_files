#pragma once
#ifndef MYCLASS_H
#define MYCLASS_H

#include "Headers.h"

class Py4GW {
public:
    static Py4GW& Instance() {
        static Py4GW instance;
        return instance;
    }

    bool Initialize();
    void Terminate();
    void Draw(IDirect3DDevice9* device);
    void Update();
	void set_gw_window_handle(HWND handle) { if (!gw_window_handle) gw_window_handle = handle; }
    static HWND get_gw_window_handle();

    void DebugMessage(const wchar_t* message) { GW::Chat::WriteChat(GW::Chat::CHANNEL_GWCA1, message, L"Py4GW"); }

    // void DrawMainWindow();

    void OnTransactionComplete();
    void OnPriceReceived(uint32_t item_id, uint32_t price);
    void OnNormalMerchantItemsReceived(GW::Packet::StoC::WindowItems* pak);
    void OnNormalMerchantItemsStreamEnd(GW::Packet::StoC::WindowItemsEnd* pak);
    void InitializeMerchantCallbacks();

    void DetachTransactionListeners();

    GW::HookEntry QuotedItemPrice_Entry;     // Entry for price callback
    GW::HookEntry TransactionComplete_Entry; // Entry for transaction callback
    GW::HookEntry ItemStreamEnd_Entry;       // Entry for item stream end callback
    GW::HookEntry WindowItems_Entry;         // Entry for item stream callback
    GW::HookEntry WindowItemsEnd_Entry;      // Entry for item stream end callback


    bool visible = true;
    bool checkbox = false;

private:
    Py4GW() = default;
    ~Py4GW() = default;
    Py4GW(const Py4GW&) = delete;
    void operator=(const Py4GW&) = delete;
    static HeroAI* heroAI;
   
public:
    static bool HeroAI_IsAIEnabled();
    static void HeroAI_SetAIEnabled(bool state);
    static int  HeroAI_GetMyPartyPos();
    // ********** Python Vars **********
    static bool HeroAI_IsActive(int party_pos);
    static void HeroAI_SetLooting(int party_pos, bool state);
    static bool HeroAI_GetLooting(int party_pos);
    static void HeroAI_SetFollowing(int party_pos, bool state);
    static bool HeroAI_GetFollowing(int party_pos);
    static void HeroAI_SetCombat(int party_pos, bool state);
    static bool HeroAI_GetCombat(int party_pos);
    static void HeroAI_SetCollision(int party_pos, bool state);
    static bool HeroAI_GetCollision(int party_pos);
    static void HeroAI_SetTargetting(int party_pos, bool state);
    static bool HeroAI_GetTargetting(int party_pos);
    static void HeroAI_SetSkill(int party_pos, int skill_pos, bool state);
    static bool HeroAI_GetSkill(int party_pos, int skill_pos);

    static void HeroAI_FlagAIHero(int party_pos, float x, float y);
    static void HeroAI_UnFlagAIHero(int party_pos);
    static float HeroAI_GetEnergy(int party_pos);
    static float HeroAI_GetEnergyRegen(int party_pos);
    static int  HeroAI_GetPythonAgentID(int party_pos);
    static void HeroAI_Resign(int party_pos);
    static void HeroAI_TakeQuest(uint32_t party_pos, uint32_t quest_id);
    static void HeroAI_Identify_cmd(int party_pos);
    static void HeroAI_Salvage_cmd(int party_pos);
    
};

#endif // MYCLASS_H
