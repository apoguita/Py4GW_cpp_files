#pragma once
#include "Headers.h"

HWND gw_client_window_handle = nullptr;
bool dll_shutdown = false;
std::string dllDirectory = "";
bool is_dragging = false;
bool is_dragging_imgui = false;
bool dragging_initialized = false;
bool global_mouse_clicked = false;

MapIntersect_pt MapIntersect_Func = 0;
ScreenToWorldPoint_pt ScreenToWorldPoint_Func = 0;

uintptr_t ptrBase_address =0;
uintptr_t ptrNdcScreenCoords = 0;

Timer reset_merchant_window_item;
Timer salvage_timer;
bool salvaging = false;
bool salvage_listeners_attached = false;
bool salvage_transaction_done = false;
bool salvage_ui_open = false;


GW::HookEntry salvage_hook_entry;
GW::Packet::StoC::SalvageSession current_salvage_session{};

MouseClickCaptureData* MouseClickCaptureDataPtr = nullptr;
uint32_t* GameCursorState = nullptr;
CaptureMouseClickType* CaptureMouseClickTypePtr = nullptr;

uint32_t quoted_item_id = 0;
int quoted_value = 0;   // Stores quoted price
bool transaction_complete = false; // Tracks transaction completion

MemMgrClass ptrGetter;

std::vector<uint32_t> merch_items;
std::vector<uint32_t> merch_items2;

std::vector<uint32_t> merchant_window_items;

ClickHandlerStruct ClickHandler;

IDirect3DDevice9* g_d3d_device = nullptr;
