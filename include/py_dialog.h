#pragma once

#include "Headers.h"

#include <condition_variable>
#include <optional>
#include <unordered_map>

struct DialogInfo {
    uint32_t dialog_id = 0;
    uint32_t flags = 0;
    uint32_t frame_type = 0;
    uint32_t event_handler = 0;
    uint32_t content_id = 0;
    uint32_t property_id = 0;
    std::wstring content = L"";
    uint32_t agent_id = 0;
};

struct ActiveDialogInfo {
    uint32_t dialog_id = 0;
    uint32_t context_dialog_id = 0;
    uint32_t agent_id = 0;
    bool dialog_id_authoritative = false;
    std::wstring message = L"";
};

struct DialogButtonInfo {
    uint32_t dialog_id = 0;
    uint32_t button_icon = 0;
    std::string message = "";
    std::string message_decoded = "";
    bool message_decode_pending = false;
};

struct DialogTextDecodedInfo {
    uint32_t dialog_id = 0;
    std::string text = "";
    bool pending = false;
};

struct DialogEventLog {
    uint64_t tick = 0;
    uint32_t message_id = 0;
    bool incoming = false;
    bool is_frame_message = false;
    uint32_t frame_id = 0;
    std::vector<uint8_t> w_bytes;
    std::vector<uint8_t> l_bytes;
};

struct DialogCallbackJournalEntry {
    uint64_t tick = 0;
    uint32_t message_id = 0;
    bool incoming = false;
    uint32_t dialog_id = 0;
    uint32_t context_dialog_id = 0;
    uint32_t agent_id = 0;
    uint32_t map_id = 0;
    uint32_t model_id = 0;
    bool dialog_id_authoritative = false;
    bool context_dialog_id_inferred = false;
    std::string npc_uid = "";
    std::string event_type = "";
    std::string text = "";
};

namespace DialogMemory {
    using DialogLoader_GetText_fn = void* (__cdecl*)(uint32_t dialog_id);

    constexpr uint32_t MAX_DIALOG_ID = 0x39u;
    constexpr uint32_t FLAGS_STRIDE = 0x24u;
    constexpr uint32_t CONTENT_STRIDE = 0x24u;
    constexpr uint32_t PROPERTY_STRIDE = 0x24u;

    constexpr uintptr_t EVENT_HANDLER_BASE = 0x00913918u;
    constexpr uintptr_t FRAME_TYPE_BASE = 0x0091391Cu;
    constexpr uintptr_t FLAGS_BASE = 0x00913920u;
    constexpr uintptr_t CONTENT_ID_BASE = 0x00913924u;
    constexpr uintptr_t PROPERTY_ID_BASE = 0x00913928u;
    constexpr uintptr_t BUTTON_IDS_BASE = 0x00913930u;
    constexpr uintptr_t CONFIG_TABLE = 0x00902980u;
    constexpr uintptr_t PROPERTY_TABLE = 0x00BAEC00u;
    constexpr uintptr_t DIALOG_CONFIG_TABLE_PTR = 0x00BBFCD4u;
    constexpr uintptr_t DIALOG_LOADER_GETTEXT = 0x0079EEF0u;
}

class Dialog {
public:
    Dialog() = default;

    static void Initialize();
    static void Terminate();

    static bool IsDialogAvailable(uint32_t dialog_id);
    static DialogInfo GetDialogInfo(uint32_t dialog_id);
    static uint32_t GetLastSelectedDialogId();
    static ActiveDialogInfo GetActiveDialog();
    static std::vector<DialogButtonInfo> GetActiveDialogButtons();
    static bool IsDialogActive();
    static bool IsDialogDisplayed(uint32_t dialog_id);
    static std::vector<DialogInfo> EnumerateAvailableDialogs();

    static std::string GetDialogTextDecoded(uint32_t dialog_id);
    static bool IsDialogTextDecodePending(uint32_t dialog_id);
    static std::vector<DialogTextDecodedInfo> GetDecodedDialogTextStatus();

    static std::vector<DialogEventLog> GetDialogEventLogs();
    static std::vector<DialogEventLog> GetDialogEventLogsReceived();
    static std::vector<DialogEventLog> GetDialogEventLogsSent();
    static void ClearDialogEventLogs();
    static void ClearDialogEventLogsReceived();
    static void ClearDialogEventLogsSent();

    static std::vector<DialogCallbackJournalEntry> GetDialogCallbackJournal();
    static std::vector<DialogCallbackJournalEntry> GetDialogCallbackJournalReceived();
    static std::vector<DialogCallbackJournalEntry> GetDialogCallbackJournalSent();
    static void ClearDialogCallbackJournal();
    static void ClearDialogCallbackJournalReceived();
    static void ClearDialogCallbackJournalSent();
    static void ClearDialogCallbackJournalFiltered(
        std::optional<std::string> npc_uid = std::nullopt,
        std::optional<bool> incoming = std::nullopt,
        std::optional<uint32_t> message_id = std::nullopt,
        std::optional<std::string> event_type = std::nullopt);

    static void ClearCache();

private:
    static ActiveDialogInfo ReadActiveDialog();

    static void RegisterDialogUiHooks();
    static void UnregisterDialogUiHooks();

    static void AppendDialogEventLog(
        GW::UI::UIMessage msgid,
        bool incoming,
        bool is_frame_message,
        uint32_t frame_id,
        const void* wparam,
        size_t wparam_size,
        const void* lparam,
        size_t lparam_size);

    static void AppendDialogCallbackJournalEntry(
        uint64_t tick,
        uint32_t message_id,
        bool incoming,
        const char* event_type,
        uint32_t dialog_id,
        uint32_t context_dialog_id,
        uint32_t agent_id,
        bool dialog_id_authoritative,
        bool context_dialog_id_inferred,
        std::optional<uint32_t> map_id = std::nullopt,
        std::optional<uint32_t> model_id = std::nullopt,
        const std::string& text = std::string{});

    static void OnDialogUIMessage(
        GW::HookStatus*,
        GW::UI::UIMessage message_id,
        void* wparam,
        void* lparam);
    static void __cdecl OnDialogBodyDecoded(void* param, const wchar_t* s);
    static void __cdecl OnDialogButtonDecoded(void* param, const wchar_t* s);

    static std::mutex dialog_mutex;
    static ActiveDialogInfo active_dialog_cache;
    static std::vector<DialogButtonInfo> active_dialog_buttons;
    static std::unordered_map<uint32_t, std::string> decoded_button_label_cache;
    static std::unordered_map<uint32_t, bool> decoded_button_label_pending;
    static bool dialog_hook_registered;
    static GW::HookEntry dialog_ui_message_entry_body;
    static GW::HookEntry dialog_ui_message_entry_button;
    static GW::HookEntry dialog_ui_message_entry_send_agent;
    static GW::HookEntry dialog_ui_message_entry_send_gadget;
    static uint32_t last_selected_dialog_id;
    static uint32_t pending_body_context_dialog_id;
    static uint32_t pending_body_context_agent_id;
    static std::vector<DialogEventLog> dialog_event_logs;
    static std::vector<DialogEventLog> dialog_event_logs_received;
    static std::vector<DialogEventLog> dialog_event_logs_sent;
    static std::vector<DialogCallbackJournalEntry> dialog_callback_journal;
    static std::vector<DialogCallbackJournalEntry> dialog_callback_journal_received;
    static std::vector<DialogCallbackJournalEntry> dialog_callback_journal_sent;
    static std::condition_variable dialog_async_decode_drained;
    static uint32_t pending_async_decode_count;
    static uint64_t decode_epoch;
    static bool dialog_shutdown_requested;
    static uint64_t active_dialog_body_decode_nonce;
};
