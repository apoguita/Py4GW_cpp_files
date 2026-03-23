#pragma once
#include "Headers.h"
#include "py_dialog.h"
#include "py_dialog_catalog.h"
#include "PyPointers.h"

#include <Windows.h>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <new>
#include <GWCA/Logger/Logger.h>

using namespace pybind11;

namespace {
    struct DialogMapStateSnapshot {
        uint32_t map_id = 0;
        bool map_ready = false;
    };

    std::string WideToUtf8Safe(const wchar_t* wstr) {
        if (!wstr) {
            return {};
        }
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) {
            return {};
        }
        try {
            std::string out(static_cast<size_t>(len), '\0');
            const int written = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), len, nullptr, nullptr);
            if (written <= 0) {
                return {};
            }
            out.resize(static_cast<size_t>(written - 1));
            return out;
        } catch (...) {
            return {};
        }
    }

    DialogMapStateSnapshot GetDialogMapStateSafe() {
        DialogMapStateSnapshot snapshot{};
        __try {
            snapshot.map_id = static_cast<uint32_t>(GW::Map::GetMapID());
            const auto instance_type = GW::Map::GetInstanceType();
            snapshot.map_ready = GW::Map::GetIsMapLoaded() &&
                !GW::Map::GetIsObserving() &&
                instance_type != GW::Constants::InstanceType::Loading;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            snapshot.map_id = 0;
            snapshot.map_ready = false;
        }
        return snapshot;
    }

    uint32_t GetCurrentMapIdSafe() {
        __try {
            return static_cast<uint32_t>(GW::Map::GetMapID());
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    uint32_t GetAgentModelIdSafe(uint32_t agent_id) {
        if (!agent_id) {
            return 0;
        }
        __try {
            GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
            if (!agent) {
                return 0;
            }
            GW::AgentLiving* living = agent->GetAsAgentLiving();
            if (!living) {
                return 0;
            }
            return static_cast<uint32_t>(living->player_number);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    std::string BuildNpcUid(uint32_t map_id, uint32_t model_id, uint32_t agent_id) {
        if (!agent_id) {
            return {};
        }
        char buffer[96] = {};
        std::snprintf(buffer, sizeof(buffer), "%u:%u:%u", map_id, model_id, agent_id);
        return std::string(buffer);
    }

    struct DialogBodyDecodeRequest {
        uint64_t tick = 0;
        uint32_t message_id = 0;
        uint32_t agent_id = 0;
        uint32_t context_dialog_id = 0;
        uint32_t map_id = 0;
        uint32_t model_id = 0;
        uint64_t decode_epoch = 0;
        uint64_t decode_nonce = 0;
        wchar_t* encoded = nullptr;
    };

    struct DialogButtonDecodeRequest {
        uint64_t tick = 0;
        uint32_t message_id = 0;
        uint32_t dialog_id = 0;
        uint32_t context_dialog_id = 0;
        uint32_t agent_id = 0;
        uint32_t map_id = 0;
        uint32_t model_id = 0;
        uint64_t decode_epoch = 0;
        wchar_t* encoded = nullptr;
    };

    bool SafeIsValidEncStr(const wchar_t* enc_str) {
        __try {
            return GW::UI::IsValidEncStr(enc_str);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    wchar_t* DupWideStringSafe(const wchar_t* src) {
        if (!src) {
            return nullptr;
        }
        __try {
            size_t len = wcslen(src);
            auto* buf = new (std::nothrow) wchar_t[len + 1];
            if (!buf) {
                return nullptr;
            }
            std::wmemcpy(buf, src, len + 1);
            return buf;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    constexpr auto kDialogAsyncDrainTimeout = std::chrono::milliseconds(500);
    constexpr auto kDialogCallbackResumeDelay = std::chrono::milliseconds(100);

    constexpr size_t kMaxDialogEventLogs = 512;
    constexpr size_t kMaxDialogCallbackJournal = 1000;
    constexpr size_t kMaxActiveDialogButtons = 64;
    constexpr size_t kMaxDecodedButtonLabelCache = 256;
    constexpr size_t kMaxDecodedButtonLabelPending = 128;

    bool CopyBytesNoFault(const void* src, size_t size, void* dst) {
        __try {
            std::memcpy(dst, src, size);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    void CopyBytesSafe(const void* src, size_t size, std::vector<uint8_t>& out) {
        out.clear();
        if (!src || size == 0) {
            return;
        }
        try {
            out.resize(size);
        } catch (...) {
            out.clear();
            return;
        }
        if (!CopyBytesNoFault(src, size, out.data())) {
            out.clear();
        }
    }

    bool CopyDialogButtonInfoSafe(const void* src, GW::UI::DialogButtonInfo& out) {
        std::memset(&out, 0, sizeof(out));
        if (!src) {
            return false;
        }
        __try {
            std::memcpy(&out, src, sizeof(out));
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            std::memset(&out, 0, sizeof(out));
            return false;
        }
    }

    bool CopyDialogBodyInfoSafe(const void* src, GW::UI::DialogBodyInfo& out) {
        std::memset(&out, 0, sizeof(out));
        if (!src) {
            return false;
        }
        __try {
            std::memcpy(&out, src, sizeof(out));
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            std::memset(&out, 0, sizeof(out));
            return false;
        }
    }

    bool SafeAsyncDecodeStr(const wchar_t* encoded, GW::UI::DecodeStr_Callback callback, void* callback_param) {
        if (!encoded || !callback) {
            return false;
        }
        __try {
            GW::UI::AsyncDecodeStr(encoded, callback, callback_param);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    void ReleaseDialogBodyDecodeRequest(DialogBodyDecodeRequest* req) {
        if (!req) {
            return;
        }
        delete[] req->encoded;
        delete req;
    }

    void ReleaseDialogButtonDecodeRequest(DialogButtonDecodeRequest* req) {
        if (!req) {
            return;
        }
        delete[] req->encoded;
        delete req;
    }

    // defined below as Dialog::AppendDialogEventLog

    bool TryUnregisterDialogUiHooksRaw(
        GW::HookEntry* body_entry,
        GW::HookEntry* button_entry,
        GW::HookEntry* send_agent_entry,
        GW::HookEntry* send_gadget_entry) {
        __try {
            GW::UI::RemoveUIMessageCallback(body_entry, GW::UI::UIMessage::kDialogBody);
            GW::UI::RemoveUIMessageCallback(button_entry, GW::UI::UIMessage::kDialogButton);
            GW::UI::RemoveUIMessageCallback(send_agent_entry, GW::UI::UIMessage::kSendAgentDialog);
            GW::UI::RemoveUIMessageCallback(send_gadget_entry, GW::UI::UIMessage::kSendGadgetDialog);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    int DialogCallbackJournalEventPriority(const std::string& event_type) {
        if (event_type == "recv_body") {
            return 0;
        }
        if (event_type == "recv_choice") {
            return 1;
        }
        if (event_type == "sent_choice") {
            return 2;
        }
        return 3;
    }

    bool DialogCallbackJournalChronologicalLess(
        const DialogCallbackJournalEntry& lhs,
        const DialogCallbackJournalEntry& rhs) {
        if (lhs.tick != rhs.tick) {
            return lhs.tick < rhs.tick;
        }

        const int lhs_priority = DialogCallbackJournalEventPriority(lhs.event_type);
        const int rhs_priority = DialogCallbackJournalEventPriority(rhs.event_type);
        if (lhs_priority != rhs_priority) {
            return lhs_priority < rhs_priority;
        }

        if (lhs.incoming != rhs.incoming) {
            return lhs.incoming && !rhs.incoming;
        }

        return false;
    }

    std::vector<DialogCallbackJournalEntry> SortDialogCallbackJournalEntries(
        std::vector<DialogCallbackJournalEntry> entries) {
        std::stable_sort(
            entries.begin(),
            entries.end(),
            DialogCallbackJournalChronologicalLess);
        return entries;
    }

}

void BindDialogInfo(py::module_& m) {
    py::class_<DialogInfo>(m, "DialogInfo")
        .def(py::init<>())
        .def_readwrite("dialog_id", &DialogInfo::dialog_id)
        .def_readwrite("flags", &DialogInfo::flags)
        .def_readwrite("frame_type", &DialogInfo::frame_type)
        .def_readwrite("event_handler", &DialogInfo::event_handler)
        .def_readwrite("content_id", &DialogInfo::content_id)
        .def_readwrite("property_id", &DialogInfo::property_id)
        .def_readwrite("content", &DialogInfo::content)
        .def_readwrite("agent_id", &DialogInfo::agent_id);
}

void BindActiveDialogInfo(py::module_& m) {
    py::class_<ActiveDialogInfo>(m, "ActiveDialogInfo")
        .def(py::init<>())
        .def_readwrite("dialog_id", &ActiveDialogInfo::dialog_id)
        .def_readwrite("context_dialog_id", &ActiveDialogInfo::context_dialog_id)
        .def_readwrite("agent_id", &ActiveDialogInfo::agent_id)
        .def_readwrite("dialog_id_authoritative", &ActiveDialogInfo::dialog_id_authoritative)
        .def_readwrite("message", &ActiveDialogInfo::message);
}

void BindDialogButtonInfo(py::module_& m) {
    py::class_<DialogButtonInfo>(m, "DialogButtonInfo")
        .def(py::init<>())
        .def_readwrite("dialog_id", &DialogButtonInfo::dialog_id)
        .def_readwrite("button_icon", &DialogButtonInfo::button_icon)
        .def_readwrite("message", &DialogButtonInfo::message)
        .def_readwrite("message_decoded", &DialogButtonInfo::message_decoded)
        .def_readwrite("message_decode_pending", &DialogButtonInfo::message_decode_pending);
}

void BindDialogTextDecodedInfo(py::module_& m) {
    py::class_<DialogTextDecodedInfo>(m, "DialogTextDecodedInfo")
        .def(py::init<>())
        .def_readwrite("dialog_id", &DialogTextDecodedInfo::dialog_id)
        .def_readwrite("text", &DialogTextDecodedInfo::text)
        .def_readwrite("pending", &DialogTextDecodedInfo::pending);
}

void BindDialogEventLog(py::module_& m) {
    py::class_<DialogEventLog>(m, "DialogEventLog")
        .def(py::init<>())
        .def_readwrite("tick", &DialogEventLog::tick)
        .def_readwrite("message_id", &DialogEventLog::message_id)
        .def_readwrite("incoming", &DialogEventLog::incoming)
        .def_readwrite("is_frame_message", &DialogEventLog::is_frame_message)
        .def_readwrite("frame_id", &DialogEventLog::frame_id)
        .def_readwrite("w_bytes", &DialogEventLog::w_bytes)
        .def_readwrite("l_bytes", &DialogEventLog::l_bytes);
}

void BindDialogCallbackJournalEntry(py::module_& m) {
    py::class_<DialogCallbackJournalEntry>(m, "DialogCallbackJournalEntry")
        .def(py::init<>())
        .def_readwrite("tick", &DialogCallbackJournalEntry::tick)
        .def_readwrite("message_id", &DialogCallbackJournalEntry::message_id)
        .def_readwrite("incoming", &DialogCallbackJournalEntry::incoming)
        .def_readwrite("dialog_id", &DialogCallbackJournalEntry::dialog_id)
        .def_readwrite("context_dialog_id", &DialogCallbackJournalEntry::context_dialog_id)
        .def_readwrite("agent_id", &DialogCallbackJournalEntry::agent_id)
        .def_readwrite("map_id", &DialogCallbackJournalEntry::map_id)
        .def_readwrite("model_id", &DialogCallbackJournalEntry::model_id)
        .def_readwrite("dialog_id_authoritative", &DialogCallbackJournalEntry::dialog_id_authoritative)
        .def_readwrite("context_dialog_id_inferred", &DialogCallbackJournalEntry::context_dialog_id_inferred)
        .def_readwrite("npc_uid", &DialogCallbackJournalEntry::npc_uid)
        .def_readwrite("event_type", &DialogCallbackJournalEntry::event_type)
        .def_readwrite("text", &DialogCallbackJournalEntry::text);
}

PYBIND11_EMBEDDED_MODULE(PyDialog, m) {
    BindDialogInfo(m);
    BindActiveDialogInfo(m);
    BindDialogButtonInfo(m);
    BindDialogTextDecodedInfo(m);
    BindDialogEventLog(m);
    BindDialogCallbackJournalEntry(m);
    py::class_<Dialog>(m, "PyDialog")
        .def(py::init<>())
        .def_static("is_dialog_available", &Dialog::IsDialogAvailable, py::arg("dialog_id"))
        .def_static("get_dialog_info", &Dialog::GetDialogInfo, py::arg("dialog_id"))
        .def_static("get_last_selected_dialog_id", &Dialog::GetLastSelectedDialogId)
        .def_static("get_active_dialog", &Dialog::GetActiveDialog)
        .def_static("get_active_dialog_buttons", &Dialog::GetActiveDialogButtons)
        .def_static("is_dialog_active", &Dialog::IsDialogActive)
        .def_static("is_dialog_displayed", &Dialog::IsDialogDisplayed, py::arg("dialog_id"))
        .def_static("enumerate_available_dialogs", &Dialog::EnumerateAvailableDialogs)
        .def_static("get_dialog_text_decoded", &Dialog::GetDialogTextDecoded, py::arg("dialog_id"))
        .def_static("is_dialog_text_decode_pending", &Dialog::IsDialogTextDecodePending, py::arg("dialog_id"))
        .def_static("get_dialog_text_decode_status", &Dialog::GetDecodedDialogTextStatus)
        .def_static("get_dialog_event_logs", &Dialog::GetDialogEventLogs)
        .def_static("get_dialog_event_logs_received", &Dialog::GetDialogEventLogsReceived)
        .def_static("get_dialog_event_logs_sent", &Dialog::GetDialogEventLogsSent)
        .def_static("clear_dialog_event_logs", &Dialog::ClearDialogEventLogs)
        .def_static("clear_dialog_event_logs_received", &Dialog::ClearDialogEventLogsReceived)
        .def_static("clear_dialog_event_logs_sent", &Dialog::ClearDialogEventLogsSent)
        .def_static("get_dialog_callback_journal", &Dialog::GetDialogCallbackJournal)
        .def_static("get_dialog_callback_journal_received", &Dialog::GetDialogCallbackJournalReceived)
        .def_static("get_dialog_callback_journal_sent", &Dialog::GetDialogCallbackJournalSent)
        .def_static("clear_dialog_callback_journal", &Dialog::ClearDialogCallbackJournal)
        .def_static("clear_dialog_callback_journal_received", &Dialog::ClearDialogCallbackJournalReceived)
        .def_static("clear_dialog_callback_journal_sent", &Dialog::ClearDialogCallbackJournalSent)
        .def_static("clear_dialog_callback_journal_filtered", &Dialog::ClearDialogCallbackJournalFiltered,
            py::arg("npc_uid") = std::nullopt,
            py::arg("incoming") = std::nullopt,
            py::arg("message_id") = std::nullopt,
            py::arg("event_type") = std::nullopt)
        .def_static("clear_cache", &Dialog::ClearCache)
        .def_static("initialize", &Dialog::Initialize)
        .def_static("terminate", &Dialog::Terminate);
}

// Initialize static members
std::mutex Dialog::dialog_mutex;
ActiveDialogInfo Dialog::active_dialog_cache = {0, 0, 0, false, L""};
std::vector<DialogButtonInfo> Dialog::active_dialog_buttons;
std::unordered_map<uint32_t, std::string> Dialog::decoded_button_label_cache;
std::unordered_map<uint32_t, bool> Dialog::decoded_button_label_pending;
bool Dialog::dialog_hook_registered = false;
GW::HookEntry Dialog::dialog_ui_message_entry_body;
GW::HookEntry Dialog::dialog_ui_message_entry_button;
GW::HookEntry Dialog::dialog_ui_message_entry_send_agent;
GW::HookEntry Dialog::dialog_ui_message_entry_send_gadget;
uint32_t Dialog::last_selected_dialog_id = 0;
uint32_t Dialog::pending_body_context_dialog_id = 0;
uint32_t Dialog::pending_body_context_agent_id = 0;
std::vector<DialogEventLog> Dialog::dialog_event_logs;
std::vector<DialogEventLog> Dialog::dialog_event_logs_received;
std::vector<DialogEventLog> Dialog::dialog_event_logs_sent;
std::vector<DialogCallbackJournalEntry> Dialog::dialog_callback_journal;
std::vector<DialogCallbackJournalEntry> Dialog::dialog_callback_journal_received;
std::vector<DialogCallbackJournalEntry> Dialog::dialog_callback_journal_sent;
std::condition_variable Dialog::dialog_async_decode_drained;
uint32_t Dialog::pending_async_decode_count = 0;
uint64_t Dialog::decode_epoch = 0;
bool Dialog::dialog_shutdown_requested = false;
bool Dialog::dialog_callbacks_suspended = true;
uint64_t Dialog::active_dialog_body_decode_nonce = 0;
uint32_t Dialog::last_observed_map_id = 0;
bool Dialog::last_observed_map_ready = false;
uint64_t Dialog::dialog_callbacks_resume_tick = 0;

void Dialog::Initialize() {
    {
        std::scoped_lock lock(dialog_mutex);
        dialog_shutdown_requested = false;
    }
    DialogCatalog::Initialize();
    ClearCache();
    RegisterDialogUiHooks();
}

void Dialog::Terminate() {
    {
        std::scoped_lock lock(dialog_mutex);
        dialog_shutdown_requested = true;
        ++decode_epoch;
        ++active_dialog_body_decode_nonce;
    }
    UnregisterDialogUiHooks();
    std::unique_lock lock(dialog_mutex);
    bool logged_wait = false;
    while (pending_async_decode_count != 0) {
        const bool drained = dialog_async_decode_drained.wait_for(
            lock,
            kDialogAsyncDrainTimeout,
            [] { return Dialog::pending_async_decode_count == 0; });
        if (drained) {
            break;
        }
        if (!logged_wait) {
            Logger::LogStaticInfo("[Dialog] Async dialog decodes did not drain within the initial shutdown timeout; continuing to wait fail-closed.");
            logged_wait = true;
        }
    }
    lock.unlock();
    ClearCache();
    DialogCatalog::Terminate();
}

void Dialog::PollMapChange() {
    const DialogMapStateSnapshot map_state = GetDialogMapStateSafe();
    ObserveMapChange(map_state.map_id, map_state.map_ready, true);

    if (!map_state.map_ready || map_state.map_id == 0) {
        return;
    }

    const uint64_t now = GetTickCount64();
    std::scoped_lock lock(dialog_mutex);
    if (dialog_shutdown_requested || !dialog_callbacks_suspended) {
        return;
    }
    if (last_observed_map_id != map_state.map_id || !last_observed_map_ready) {
        return;
    }
    if (now < dialog_callbacks_resume_tick) {
        return;
    }
    dialog_callbacks_suspended = false;
    dialog_callbacks_resume_tick = 0;
}

// ================= Synchronous Methods (Direct Memory Access) =================

bool Dialog::IsDialogAvailable(uint32_t dialog_id) {
    return DialogCatalog::IsDialogAvailable(dialog_id);
}

DialogInfo Dialog::GetDialogInfo(uint32_t dialog_id) {
    return DialogCatalog::GetDialogInfo(dialog_id);
}

uint32_t Dialog::GetLastSelectedDialogId() {
    std::scoped_lock lock(dialog_mutex);
    return last_selected_dialog_id;
}

ActiveDialogInfo Dialog::GetActiveDialog() {
    return ReadActiveDialog();
}

std::vector<DialogButtonInfo> Dialog::GetActiveDialogButtons() {
    std::vector<DialogButtonInfo> out;
        {
            std::scoped_lock lock(dialog_mutex);
            out = active_dialog_buttons;
        }
        for (auto& btn : out) {
            if (btn.dialog_id == 0) {
                continue;
            }
            std::string label;
            bool pending = false;
            {
                std::scoped_lock lock(dialog_mutex);
                auto it = decoded_button_label_cache.find(btn.dialog_id);
                if (it != decoded_button_label_cache.end()) {
                    label = it->second;
                }
                auto pit = decoded_button_label_pending.find(btn.dialog_id);
                if (pit != decoded_button_label_pending.end()) {
                    pending = pit->second;
                }
            }
            if (label.empty()) {
                label = DialogCatalog::GetDialogTextDecoded(btn.dialog_id);
                pending = DialogCatalog::IsDialogTextDecodePending(btn.dialog_id);
            }
            btn.message_decoded = label;
            btn.message = label;
            btn.message_decode_pending = pending;
        }
        return out;
    }

    bool Dialog::IsDialogActive() {
        // "NPC Dialog" root frame hash used across Py4GW UI helpers.
        constexpr uint32_t kNpcDialogHash = 3856160816u;
        __try {
            const uint32_t frame_id = GW::UI::GetFrameIDByHash(kNpcDialogHash);
            if (!frame_id) {
                return false;
            }
            const GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
            if (!frame) {
                return false;
            }
            return frame->IsCreated() && frame->IsVisible();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

bool Dialog::IsDialogDisplayed(uint32_t dialog_id) {
    std::scoped_lock lock(dialog_mutex);
    if (dialog_id == 0) {
        return false;
    }
    return active_dialog_cache.dialog_id == dialog_id ||
        active_dialog_cache.context_dialog_id == dialog_id;
}

    std::vector<DialogInfo> Dialog::EnumerateAvailableDialogs() {
        return DialogCatalog::EnumerateAvailableDialogs();
    }

std::string Dialog::GetDialogTextDecoded(uint32_t dialog_id) {
    return DialogCatalog::GetDialogTextDecoded(dialog_id);
}

bool Dialog::IsDialogTextDecodePending(uint32_t dialog_id) {
    return DialogCatalog::IsDialogTextDecodePending(dialog_id);
}

std::vector<DialogTextDecodedInfo> Dialog::GetDecodedDialogTextStatus() {
    return DialogCatalog::GetDecodedDialogTextStatus();
}

void Dialog::ClearCache() {
    const DialogMapStateSnapshot map_state = GetDialogMapStateSafe();
    const uint64_t now = GetTickCount64();
    std::scoped_lock lock(dialog_mutex);
    active_dialog_cache = {0, 0, 0, false, L""};
    active_dialog_buttons.clear();
    last_selected_dialog_id = 0;
    pending_body_context_dialog_id = 0;
    pending_body_context_agent_id = 0;
    decoded_button_label_cache.clear();
    decoded_button_label_pending.clear();
    dialog_event_logs.clear();
    dialog_event_logs_received.clear();
    dialog_event_logs_sent.clear();
    dialog_callback_journal.clear();
    dialog_callback_journal_received.clear();
    dialog_callback_journal_sent.clear();
    ++decode_epoch;
    ++active_dialog_body_decode_nonce;
    last_observed_map_id = map_state.map_id;
    last_observed_map_ready = map_state.map_ready;
    dialog_callbacks_suspended = !map_state.map_ready;
    dialog_callbacks_resume_tick = map_state.map_ready ? 0 : now;
    DialogCatalog::ClearCache();
}

void Dialog::ObserveMapChange(uint32_t current_map_id, bool current_map_ready, bool log_transition) {
    const uint64_t now = GetTickCount64();
    uint32_t previous_map_id = 0;
    bool previous_map_ready = false;
    uint32_t pending_decode_snapshot = 0;
    bool should_log = false;
    {
        std::scoped_lock lock(dialog_mutex);
        previous_map_id = last_observed_map_id;
        previous_map_ready = last_observed_map_ready;
        const bool map_id_changed = previous_map_id != current_map_id;
        const bool map_ready_changed = previous_map_ready != current_map_ready;
        if (!map_id_changed && !map_ready_changed) {
            return;
        }

        last_observed_map_id = current_map_id;
        last_observed_map_ready = current_map_ready;
        if (dialog_shutdown_requested) {
            return;
        }

        if (!current_map_ready || map_id_changed) {
            dialog_callbacks_suspended = true;
            dialog_callbacks_resume_tick = now + static_cast<uint64_t>(kDialogCallbackResumeDelay.count());
        }

        const bool should_invalidate_runtime_state =
            (map_id_changed && previous_map_id != 0) ||
            (previous_map_ready && !current_map_ready);
        if (!should_invalidate_runtime_state) {
            return;
        }

        const bool had_runtime_state =
            active_dialog_cache.dialog_id != 0 ||
            active_dialog_cache.context_dialog_id != 0 ||
            active_dialog_cache.agent_id != 0 ||
            !active_dialog_cache.message.empty() ||
            !active_dialog_buttons.empty() ||
            last_selected_dialog_id != 0 ||
            pending_body_context_dialog_id != 0 ||
            pending_body_context_agent_id != 0 ||
            !decoded_button_label_pending.empty() ||
            pending_async_decode_count != 0;

        active_dialog_cache = {0, 0, 0, false, L""};
        active_dialog_buttons.clear();
        last_selected_dialog_id = 0;
        pending_body_context_dialog_id = 0;
        pending_body_context_agent_id = 0;
        decoded_button_label_cache.clear();
        decoded_button_label_pending.clear();
        ++decode_epoch;
        ++active_dialog_body_decode_nonce;

        if (log_transition && had_runtime_state) {
            pending_decode_snapshot = pending_async_decode_count;
            should_log = true;
        }
    }

    if (should_log) {
        Logger::LogStaticInfo(
            "[Dialog] Invalidated active dialog state on map transition/readiness change (" +
            std::to_string(previous_map_id) + ":" + (previous_map_ready ? "ready" : "not_ready") +
            " -> " + std::to_string(current_map_id) + ":" + (current_map_ready ? "ready" : "not_ready") +
            ", pending_async=" + std::to_string(pending_decode_snapshot) + ").");
    }
}

void Dialog::RegisterDialogUiHooks() {
    {
        std::scoped_lock lock(dialog_mutex);
        if (dialog_hook_registered) {
            return;
        }
    }
    GW::UI::RegisterUIMessageCallback(
        &dialog_ui_message_entry_body,
        GW::UI::UIMessage::kDialogBody,
        Dialog::OnDialogUIMessage,
        0x1);
    GW::UI::RegisterUIMessageCallback(
        &dialog_ui_message_entry_button,
        GW::UI::UIMessage::kDialogButton,
        Dialog::OnDialogUIMessage,
        0x1);
    GW::UI::RegisterUIMessageCallback(
        &dialog_ui_message_entry_send_agent,
        GW::UI::UIMessage::kSendAgentDialog,
        Dialog::OnDialogUIMessage,
        0x1);
    GW::UI::RegisterUIMessageCallback(
        &dialog_ui_message_entry_send_gadget,
        GW::UI::UIMessage::kSendGadgetDialog,
        Dialog::OnDialogUIMessage,
        0x1);
    std::scoped_lock lock(dialog_mutex);
    dialog_hook_registered = true;
}

void Dialog::UnregisterDialogUiHooks() {
    {
        std::scoped_lock lock(dialog_mutex);
        if (!dialog_hook_registered) {
            return;
        }
        dialog_hook_registered = false;
    }
    if (!TryUnregisterDialogUiHooksRaw(
            &dialog_ui_message_entry_body,
            &dialog_ui_message_entry_button,
            &dialog_ui_message_entry_send_agent,
            &dialog_ui_message_entry_send_gadget)) {
        Logger::LogStaticInfo("[Dialog] Failed to remove one or more dialog UI hooks during shutdown.");
    }
}

void Dialog::OnDialogUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
    const DialogMapStateSnapshot map_state = GetDialogMapStateSafe();
    ObserveMapChange(map_state.map_id, map_state.map_ready, false);
    if (!wparam) {
        return;
    }
    {
        std::scoped_lock lock(dialog_mutex);
        if (dialog_shutdown_requested || dialog_callbacks_suspended || !map_state.map_ready) {
            return;
        }
    }

    switch (message_id) {
        case GW::UI::UIMessage::kDialogButton: {
                GW::UI::DialogButtonInfo info_local{};
                if (!CopyDialogButtonInfoSafe(wparam, info_local)) {
                    return;
                }
                const auto* info = &info_local;
                const uint64_t tick = GetTickCount64();
                uint32_t context_dialog_id = 0;
                uint32_t context_agent_id = 0;
                uint64_t request_epoch = 0;
                {
                    std::scoped_lock lock(dialog_mutex);
                    context_dialog_id = active_dialog_cache.context_dialog_id
                        ? active_dialog_cache.context_dialog_id
                        : active_dialog_cache.dialog_id;
                    context_agent_id = active_dialog_cache.agent_id;
                    request_epoch = decode_epoch;
                }
                const uint32_t callback_map_id = map_state.map_id;
                const uint32_t callback_model_id = GetAgentModelIdSafe(context_agent_id);
                Dialog::AppendDialogEventLog(
                    message_id,
                    true,
                    false,
                    0,
                    info,
                    sizeof(GW::UI::DialogButtonInfo),
                    nullptr,
                    0
                );
                std::string label_utf8;
                bool label_pending = false;
                if (info->message) {
                    wchar_t* encoded_copy = DupWideStringSafe(info->message);
                    if (encoded_copy) {
                        if (!SafeIsValidEncStr(encoded_copy)) {
                            label_utf8 = WideToUtf8Safe(encoded_copy);
                            delete[] encoded_copy;
                        }
                        else {
                            auto* req = new (std::nothrow) DialogButtonDecodeRequest();
                            if (!req) {
                                delete[] encoded_copy;
                                break;
                            }
                            bool queue_async_label = false;
                            req->tick = tick;
                            req->message_id = static_cast<uint32_t>(message_id);
                            req->dialog_id = info->dialog_id;
                            req->context_dialog_id = context_dialog_id;
                            req->agent_id = context_agent_id;
                            req->map_id = callback_map_id;
                            req->model_id = callback_model_id;
                            req->decode_epoch = request_epoch;
                            req->encoded = encoded_copy;
                            bool release_req = false;
                            {
                                std::scoped_lock lock(dialog_mutex);
                                if (dialog_shutdown_requested ||
                                    dialog_callbacks_suspended ||
                                    req->decode_epoch != decode_epoch) {
                                    release_req = true;
                                }
                                else {
                                    try {
                                        const bool is_new_pending =
                                            decoded_button_label_pending.find(info->dialog_id) == decoded_button_label_pending.end();
                                        if (is_new_pending &&
                                            decoded_button_label_pending.size() >= kMaxDecodedButtonLabelPending) {
                                            release_req = true;
                                        }
                                        else {
                                            decoded_button_label_pending[info->dialog_id] = true;
                                            ++pending_async_decode_count;
                                            queue_async_label = true;
                                        }
                                    } catch (...) {
                                        decoded_button_label_pending.erase(info->dialog_id);
                                        release_req = true;
                                    }
                                }
                            }
                            if (release_req) {
                                ReleaseDialogButtonDecodeRequest(req);
                            }
                            else if (queue_async_label) {
                                if (SafeAsyncDecodeStr(req->encoded, Dialog::OnDialogButtonDecoded, req)) {
                                    label_pending = true;
                                }
                                else {
                                    {
                                        std::scoped_lock lock(dialog_mutex);
                                        if (pending_async_decode_count > 0) {
                                            --pending_async_decode_count;
                                        }
                                        decoded_button_label_pending.erase(info->dialog_id);
                                    }
                                    dialog_async_decode_drained.notify_all();
                                    ReleaseDialogButtonDecodeRequest(req);
                                }
                            }
                        }
                    }
                }

                {
                    std::scoped_lock lock(dialog_mutex);
                    try {
                        if (request_epoch == decode_epoch &&
                            !dialog_shutdown_requested &&
                            !dialog_callbacks_suspended &&
                            !label_utf8.empty()) {
                            decoded_button_label_cache[info->dialog_id] = label_utf8;
                            if (decoded_button_label_cache.size() > kMaxDecodedButtonLabelCache) {
                                decoded_button_label_cache.erase(decoded_button_label_cache.begin());
                            }
                            decoded_button_label_pending.erase(info->dialog_id);
                        }
                        if (request_epoch == decode_epoch &&
                            !dialog_shutdown_requested &&
                            !dialog_callbacks_suspended) {
                            DialogButtonInfo button{};
                            button.dialog_id = info->dialog_id;
                            button.button_icon = info->button_icon;
                            button.message = label_utf8;
                            button.message_decoded = label_utf8;
                            button.message_decode_pending = label_pending;
                            active_dialog_buttons.push_back(std::move(button));
                            if (active_dialog_buttons.size() > kMaxActiveDialogButtons) {
                                const size_t overflow = active_dialog_buttons.size() - kMaxActiveDialogButtons;
                                active_dialog_buttons.erase(
                                    active_dialog_buttons.begin(),
                                    active_dialog_buttons.begin() + overflow);
                            }
                        }
                    } catch (...) {
                        if (request_epoch == decode_epoch) {
                            decoded_button_label_pending.erase(info->dialog_id);
                        }
                    }
                }
                if (!label_pending) {
                    bool append_journal = false;
                    {
                        std::scoped_lock lock(dialog_mutex);
                        append_journal =
                            request_epoch == decode_epoch &&
                            !dialog_shutdown_requested &&
                            !dialog_callbacks_suspended;
                    }
                    if (append_journal) {
                        Dialog::AppendDialogCallbackJournalEntry(
                            tick,
                            static_cast<uint32_t>(message_id),
                            true,
                            "recv_choice",
                            info->dialog_id,
                            context_dialog_id,
                            context_agent_id,
                            true,
                            context_dialog_id != 0,
                            callback_map_id,
                            callback_model_id,
                            label_utf8
                        );
                    }
                }
            } break;
            case GW::UI::UIMessage::kDialogBody: {
                GW::UI::DialogBodyInfo info_local{};
                if (!CopyDialogBodyInfoSafe(wparam, info_local)) {
                    return;
                }
                const auto* info = &info_local;
                const uint64_t tick = GetTickCount64();
                const uint32_t callback_map_id = map_state.map_id;

                Dialog::AppendDialogEventLog(
                    message_id,
                    true,
                    false,
                    0,
                    info,
                    sizeof(GW::UI::DialogBodyInfo),
                    nullptr,
                    0
                );

                uint32_t context_dialog_id = 0;
                uint64_t request_epoch = 0;
                uint64_t decode_nonce = 0;
                bool body_state_active = false;
                {
                    std::scoped_lock lock(dialog_mutex);
                    if (dialog_shutdown_requested || dialog_callbacks_suspended) {
                        break;
                    }
                    active_dialog_cache.agent_id = info->agent_id;
                    if (pending_body_context_dialog_id != 0) {
                        const bool same_agent =
                            pending_body_context_agent_id == 0 ||
                            pending_body_context_agent_id == info->agent_id;
                        if (same_agent) {
                            context_dialog_id = pending_body_context_dialog_id;
                        }
                    }
                    pending_body_context_dialog_id = 0;
                    pending_body_context_agent_id = 0;
                    active_dialog_cache.dialog_id = 0;
                    active_dialog_cache.context_dialog_id = context_dialog_id;
                    active_dialog_cache.dialog_id_authoritative = false;
                    active_dialog_cache.message.clear();
                    active_dialog_buttons.clear();
                    request_epoch = decode_epoch;
                    decode_nonce = ++active_dialog_body_decode_nonce;
                    body_state_active = true;
                }
                if (!body_state_active) {
                    break;
                }

                bool append_immediate = true;
                std::string immediate_text;
                const uint32_t callback_model_id = GetAgentModelIdSafe(info->agent_id);

                if (info->message_enc) {
                    wchar_t* encoded_copy = DupWideStringSafe(info->message_enc);
                    if (encoded_copy) {
                        if (!SafeIsValidEncStr(encoded_copy)) {
                            std::wstring plain_text;
                            try {
                                plain_text.assign(encoded_copy);
                            } catch (...) {
                            }
                            immediate_text = WideToUtf8Safe(encoded_copy);
                            delete[] encoded_copy;
                            {
                                std::scoped_lock lock(dialog_mutex);
                                if (!dialog_shutdown_requested &&
                                    !dialog_callbacks_suspended &&
                                    request_epoch == decode_epoch &&
                                    active_dialog_cache.agent_id == info->agent_id &&
                                    active_dialog_body_decode_nonce == decode_nonce) {
                                    try {
                                        active_dialog_cache.message = plain_text;
                                    } catch (...) {
                                    }
                                }
                            }
                        }
                        else {
                            auto* req = new (std::nothrow) DialogBodyDecodeRequest();
                            if (!req) {
                                delete[] encoded_copy;
                                break;
                            }
                            bool queue_async_body = false;
                            req->tick = tick;
                            req->message_id = static_cast<uint32_t>(message_id);
                            req->agent_id = info->agent_id;
                            req->context_dialog_id = context_dialog_id;
                            req->map_id = callback_map_id;
                            req->model_id = callback_model_id;
                            req->decode_epoch = request_epoch;
                            req->decode_nonce = decode_nonce;
                            req->encoded = encoded_copy;
                            {
                                std::scoped_lock lock(dialog_mutex);
                                if (dialog_shutdown_requested ||
                                    dialog_callbacks_suspended ||
                                    req->decode_epoch != decode_epoch) {
                                    delete[] req->encoded;
                                    delete req;
                                } else {
                                    ++pending_async_decode_count;
                                    queue_async_body = true;
                                }
                            }
                            if (queue_async_body) {
                                if (SafeAsyncDecodeStr(req->encoded, Dialog::OnDialogBodyDecoded, req)) {
                                    append_immediate = false;
                                }
                                else {
                                    {
                                        std::scoped_lock lock(dialog_mutex);
                                        if (pending_async_decode_count > 0) {
                                            --pending_async_decode_count;
                                        }
                                    }
                                    dialog_async_decode_drained.notify_all();
                                    ReleaseDialogBodyDecodeRequest(req);
                                }
                            }
                        }
                    }
                }
                if (append_immediate) {
                    bool append_journal = false;
                    {
                        std::scoped_lock lock(dialog_mutex);
                        append_journal =
                            request_epoch == decode_epoch &&
                            !dialog_shutdown_requested &&
                            !dialog_callbacks_suspended;
                    }
                    if (append_journal) {
                        Dialog::AppendDialogCallbackJournalEntry(
                            tick,
                            static_cast<uint32_t>(message_id),
                            true,
                            "recv_body",
                            0,
                            context_dialog_id,
                            info->agent_id,
                            false,
                            context_dialog_id != 0,
                            callback_map_id,
                            callback_model_id,
                            immediate_text
                        );
                    }
                }
            } break;
            case GW::UI::UIMessage::kSendAgentDialog:
            case GW::UI::UIMessage::kSendGadgetDialog: {
                const uint64_t tick = GetTickCount64();
                uint32_t selected_id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam));
                Dialog::AppendDialogEventLog(
                    message_id,
                    false,
                    false,
                    0,
                    &selected_id,
                    sizeof(selected_id),
                    nullptr,
                    0
                );
                uint32_t context_dialog_id = 0;
                uint32_t context_agent_id = 0;
                std::string sent_text;
                bool emit_sent_choice = false;
                {
                    std::scoped_lock lock(dialog_mutex);
                    if (dialog_shutdown_requested || dialog_callbacks_suspended) {
                        break;
                    }
                    context_dialog_id = active_dialog_cache.context_dialog_id
                        ? active_dialog_cache.context_dialog_id
                        : active_dialog_cache.dialog_id;
                    context_agent_id = active_dialog_cache.agent_id;
                    auto label_it = decoded_button_label_cache.find(selected_id);
                    try {
                        if (label_it != decoded_button_label_cache.end()) {
                            sent_text = label_it->second;
                        }
                        else if (!DialogCatalog::TryGetCachedDialogTextDecoded(selected_id, sent_text)) {
                            sent_text.clear();
                        }
                    } catch (...) {
                        sent_text.clear();
                    }
                    last_selected_dialog_id = selected_id;
                    pending_body_context_dialog_id = selected_id;
                    pending_body_context_agent_id = context_agent_id;
                    active_dialog_cache.dialog_id = 0;
                    active_dialog_cache.context_dialog_id = selected_id;
                    active_dialog_cache.dialog_id_authoritative = false;
                    emit_sent_choice = true;
                }
                if (emit_sent_choice) {
                    Dialog::AppendDialogCallbackJournalEntry(
                        tick,
                        static_cast<uint32_t>(message_id),
                        false,
                        "sent_choice",
                        selected_id,
                        context_dialog_id,
                        context_agent_id,
                        true,
                        context_dialog_id != 0,
                        std::nullopt,
                        std::nullopt,
                        sent_text
                    );
                }
            } break;
            default:
                break;
        }

    }

    void Dialog::AppendDialogEventLog(
        GW::UI::UIMessage msgid,
        bool incoming,
        bool is_frame_message,
        uint32_t frame_id,
        const void* wparam,
        size_t wparam_size,
        const void* lparam,
        size_t lparam_size
    ) {
        DialogEventLog entry{};
        entry.tick = GetTickCount64();
        entry.message_id = static_cast<uint32_t>(msgid);
        entry.incoming = incoming;
        entry.is_frame_message = is_frame_message;
        entry.frame_id = frame_id;
        CopyBytesSafe(wparam, wparam_size, entry.w_bytes);
        CopyBytesSafe(lparam, lparam_size, entry.l_bytes);
        std::scoped_lock lock(dialog_mutex);
        try {
            dialog_event_logs.push_back(std::move(entry));
            if (dialog_event_logs.size() > kMaxDialogEventLogs) {
                const size_t overflow = dialog_event_logs.size() - kMaxDialogEventLogs;
                dialog_event_logs.erase(dialog_event_logs.begin(),
                    dialog_event_logs.begin() + overflow);
            }
            auto& dir_logs = incoming ? dialog_event_logs_received : dialog_event_logs_sent;
            dir_logs.push_back(dialog_event_logs.back());
            if (dir_logs.size() > kMaxDialogEventLogs) {
                const size_t overflow = dir_logs.size() - kMaxDialogEventLogs;
                dir_logs.erase(dir_logs.begin(), dir_logs.begin() + overflow);
            }
        } catch (...) {
        }
    }

    void Dialog::AppendDialogCallbackJournalEntry(
        uint64_t tick,
        uint32_t message_id,
        bool incoming,
        const char* event_type,
        uint32_t dialog_id,
        uint32_t context_dialog_id,
        uint32_t agent_id,
        bool dialog_id_authoritative,
        bool context_dialog_id_inferred,
        std::optional<uint32_t> map_id,
        std::optional<uint32_t> model_id,
        const std::string& text
    ) {
        DialogCallbackJournalEntry entry{};
        entry.tick = tick ? tick : GetTickCount64();
        entry.message_id = message_id;
        entry.incoming = incoming;
        entry.dialog_id = dialog_id;
        entry.context_dialog_id = context_dialog_id;
        entry.agent_id = agent_id;
        entry.map_id = map_id.has_value() ? *map_id : GetCurrentMapIdSafe();
        entry.model_id = model_id.has_value() ? *model_id : GetAgentModelIdSafe(agent_id);
        entry.dialog_id_authoritative = dialog_id_authoritative;
        entry.context_dialog_id_inferred = context_dialog_id_inferred;
        try {
            entry.npc_uid = BuildNpcUid(entry.map_id, entry.model_id, agent_id);
            entry.event_type = event_type ? event_type : "";
            entry.text = text;
        } catch (...) {
            return;
        }

        std::scoped_lock lock(dialog_mutex);
        try {
            dialog_callback_journal.push_back(entry);
            if (dialog_callback_journal.size() > kMaxDialogCallbackJournal) {
                const size_t overflow = dialog_callback_journal.size() - kMaxDialogCallbackJournal;
                dialog_callback_journal.erase(
                    dialog_callback_journal.begin(),
                    dialog_callback_journal.begin() + overflow);
            }

            auto& dir_logs = incoming ? dialog_callback_journal_received : dialog_callback_journal_sent;
            dir_logs.push_back(std::move(entry));
            if (dir_logs.size() > kMaxDialogCallbackJournal) {
                const size_t overflow = dir_logs.size() - kMaxDialogCallbackJournal;
                dir_logs.erase(dir_logs.begin(), dir_logs.begin() + overflow);
            }
        } catch (...) {
        }
    }

    std::vector<DialogEventLog> Dialog::GetDialogEventLogs() {
        std::scoped_lock lock(dialog_mutex);
        return dialog_event_logs;
    }

    std::vector<DialogEventLog> Dialog::GetDialogEventLogsReceived() {
        std::scoped_lock lock(dialog_mutex);
        return dialog_event_logs_received;
    }

    std::vector<DialogEventLog> Dialog::GetDialogEventLogsSent() {
        std::scoped_lock lock(dialog_mutex);
        return dialog_event_logs_sent;
    }

    void Dialog::ClearDialogEventLogs() {
        std::scoped_lock lock(dialog_mutex);
        dialog_event_logs.clear();
        dialog_event_logs_received.clear();
        dialog_event_logs_sent.clear();
    }

    void Dialog::ClearDialogEventLogsReceived() {
        std::scoped_lock lock(dialog_mutex);
        dialog_event_logs_received.clear();
    }

    void Dialog::ClearDialogEventLogsSent() {
        std::scoped_lock lock(dialog_mutex);
        dialog_event_logs_sent.clear();
    }

    std::vector<DialogCallbackJournalEntry> Dialog::GetDialogCallbackJournal() {
        std::vector<DialogCallbackJournalEntry> entries;
        {
            std::scoped_lock lock(dialog_mutex);
            entries = dialog_callback_journal;
        }
        return SortDialogCallbackJournalEntries(entries);
    }

    std::vector<DialogCallbackJournalEntry> Dialog::GetDialogCallbackJournalReceived() {
        std::vector<DialogCallbackJournalEntry> entries;
        {
            std::scoped_lock lock(dialog_mutex);
            entries = dialog_callback_journal_received;
        }
        return SortDialogCallbackJournalEntries(entries);
    }

    std::vector<DialogCallbackJournalEntry> Dialog::GetDialogCallbackJournalSent() {
        std::vector<DialogCallbackJournalEntry> entries;
        {
            std::scoped_lock lock(dialog_mutex);
            entries = dialog_callback_journal_sent;
        }
        return SortDialogCallbackJournalEntries(entries);
    }

    void Dialog::ClearDialogCallbackJournal() {
        std::scoped_lock lock(dialog_mutex);
        dialog_callback_journal.clear();
        dialog_callback_journal_received.clear();
        dialog_callback_journal_sent.clear();
    }

    void Dialog::ClearDialogCallbackJournalReceived() {
        std::scoped_lock lock(dialog_mutex);
        dialog_callback_journal_received.clear();
    }

    void Dialog::ClearDialogCallbackJournalSent() {
        std::scoped_lock lock(dialog_mutex);
        dialog_callback_journal_sent.clear();
    }

    void Dialog::ClearDialogCallbackJournalFiltered(
        std::optional<std::string> npc_uid,
        std::optional<bool> incoming,
        std::optional<uint32_t> message_id,
        std::optional<std::string> event_type
    ) {
        std::scoped_lock lock(dialog_mutex);
        if (!npc_uid.has_value() && !incoming.has_value() && !message_id.has_value() && !event_type.has_value()) {
            dialog_callback_journal.clear();
            dialog_callback_journal_received.clear();
            dialog_callback_journal_sent.clear();
            return;
        }

        const bool has_event_type = event_type.has_value() && !event_type->empty();
        const std::string event_type_filter = has_event_type ? *event_type : "";
        auto matches = [&](const DialogCallbackJournalEntry& entry) -> bool {
            if (npc_uid.has_value() && entry.npc_uid != *npc_uid) {
                return false;
            }
            if (incoming.has_value() && entry.incoming != *incoming) {
                return false;
            }
            if (message_id.has_value() && entry.message_id != *message_id) {
                return false;
            }
            if (has_event_type && entry.event_type != event_type_filter) {
                return false;
            }
            return true;
        };

        dialog_callback_journal.erase(
            std::remove_if(dialog_callback_journal.begin(), dialog_callback_journal.end(), matches),
            dialog_callback_journal.end());

        dialog_callback_journal_received.clear();
        dialog_callback_journal_sent.clear();
        dialog_callback_journal_received.reserve(dialog_callback_journal.size());
        dialog_callback_journal_sent.reserve(dialog_callback_journal.size());
        for (const auto& entry : dialog_callback_journal) {
            if (entry.incoming) {
                dialog_callback_journal_received.push_back(entry);
            }
            else {
                dialog_callback_journal_sent.push_back(entry);
            }
        }
    }

void __cdecl Dialog::OnDialogBodyDecoded(void* param, const wchar_t* s) {
    auto* req = static_cast<DialogBodyDecodeRequest*>(param);
    if (!req) {
        return;
    }
    const DialogMapStateSnapshot map_state = GetDialogMapStateSafe();
    ObserveMapChange(map_state.map_id, map_state.map_ready, false);
    wchar_t* decoded_copy = DupWideStringSafe(s);
    std::wstring decoded_w;
    if (decoded_copy) {
        try {
            decoded_w.assign(decoded_copy);
        } catch (...) {
        }
    }
    const std::string decoded_text = decoded_copy ? WideToUtf8Safe(decoded_copy) : std::string{};
    bool append_journal = false;
    {
        std::scoped_lock lock(dialog_mutex);
        if (pending_async_decode_count > 0) {
            --pending_async_decode_count;
        }
        if (!dialog_shutdown_requested &&
            !dialog_callbacks_suspended &&
            map_state.map_ready &&
            req->decode_epoch == decode_epoch) {
            append_journal = true;
            if (active_dialog_cache.agent_id == req->agent_id &&
                active_dialog_body_decode_nonce == req->decode_nonce) {
                try {
                    active_dialog_cache.message = decoded_w;
                } catch (...) {
                }
            }
        }
    }
    dialog_async_decode_drained.notify_all();
    delete[] decoded_copy;
    if (append_journal) {
        Dialog::AppendDialogCallbackJournalEntry(
            req->tick,
            req->message_id,
            true,
            "recv_body",
            0,
            req->context_dialog_id,
            req->agent_id,
            false,
            req->context_dialog_id != 0,
            req->map_id,
            req->model_id,
            decoded_text
        );
    }
    ReleaseDialogBodyDecodeRequest(req);
}

void __cdecl Dialog::OnDialogButtonDecoded(void* param, const wchar_t* s) {
    auto* req = static_cast<DialogButtonDecodeRequest*>(param);
    if (!req) {
        return;
    }
    const DialogMapStateSnapshot map_state = GetDialogMapStateSafe();
    ObserveMapChange(map_state.map_id, map_state.map_ready, false);
    wchar_t* decoded_copy = DupWideStringSafe(s);
    const std::string decoded_label = decoded_copy ? WideToUtf8Safe(decoded_copy) : std::string{};
    bool append_journal = false;
    {
        std::scoped_lock lock(dialog_mutex);
        if (pending_async_decode_count > 0) {
            --pending_async_decode_count;
        }
        if (!dialog_shutdown_requested &&
            !dialog_callbacks_suspended &&
            map_state.map_ready &&
            req->decode_epoch == decode_epoch) {
            try {
                decoded_button_label_cache[req->dialog_id] = decoded_label;
                if (decoded_button_label_cache.size() > kMaxDecodedButtonLabelCache) {
                    decoded_button_label_cache.erase(decoded_button_label_cache.begin());
                }
                decoded_button_label_pending.erase(req->dialog_id);
                append_journal = true;
            } catch (...) {
                decoded_button_label_pending.erase(req->dialog_id);
            }
        }
    }
    dialog_async_decode_drained.notify_all();
    delete[] decoded_copy;
    if (append_journal) {
        Dialog::AppendDialogCallbackJournalEntry(
            req->tick,
            req->message_id,
            true,
            "recv_choice",
            req->dialog_id,
            req->context_dialog_id,
            req->agent_id,
            true,
            req->context_dialog_id != 0,
            req->map_id,
            req->model_id,
            decoded_label
        );
    }
    ReleaseDialogButtonDecodeRequest(req);
}

ActiveDialogInfo Dialog::ReadActiveDialog() {
    std::scoped_lock lock(dialog_mutex);
    return active_dialog_cache;
}
