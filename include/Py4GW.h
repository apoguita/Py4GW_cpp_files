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
    //static HeroAI* heroAI;
   
    
};

#endif // MYCLASS_H
