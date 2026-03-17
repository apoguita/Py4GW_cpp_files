#pragma once
#include "Headers.h"
#include "py_dialog.h"
#include "PyPointers.h"

#include <Windows.h>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <algorithm>
#include <GWCA/Logger/Logger.h>

using namespace pybind11;

namespace {
    constexpr uintptr_t kGwImageBase = 0x00400000;

    uintptr_t ToRuntimeAddress(uintptr_t va) {
        static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if (!base) {
            return va;
        }
        return base + (va - kGwImageBase);
    }

    void LogMemoryReadFailure(const char* label, uintptr_t address) {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "[Dialog] Failed to read %s at 0x%08X", label, static_cast<uint32_t>(address));
        Logger::LogStaticInfo(buffer);
    }

    std::string WideToUtf8Safe(const wchar_t* wstr) {
        if (!wstr) {
            return {};
        }
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) {
            return {};
        }
        std::string out(static_cast<size_t>(len - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), len, nullptr, nullptr);
        return out;
    }

    std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) {
            return {};
        }
        int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        if (len <= 0) {
            return {};
        }
        std::wstring out(static_cast<size_t>(len - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, out.data(), len);
        return out;
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

    bool TryReadU32(uintptr_t address, uint32_t& out, const char* label) {
        __try {
            out = *reinterpret_cast<uint32_t*>(address);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            out = 0;
            LogMemoryReadFailure(label, address);
            return false;
        }
    }

    bool TryReadU8(uintptr_t address, uint8_t& out, const char* label) {
        __try {
            out = *reinterpret_cast<uint8_t*>(address);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            out = 0;
            LogMemoryReadFailure(label, address);
            return false;
        }
    }

    bool TryReadPtr(uintptr_t address, void*& out, const char* label) {
        __try {
            out = *reinterpret_cast<void**>(address);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            out = nullptr;
            LogMemoryReadFailure(label, address);
            return false;
        }
    }

    bool TryReadU32NoLog(uintptr_t address, uint32_t& out) {
        __try {
            out = *reinterpret_cast<uint32_t*>(address);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            out = 0;
            return false;
        }
    }

    bool TryReadU8NoLog(uintptr_t address, uint8_t& out) {
        __try {
            out = *reinterpret_cast<uint8_t*>(address);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            out = 0;
            return false;
        }
    }

    constexpr uintptr_t kSkillBarSetSlotData = 0x007F6220;
    constexpr uintptr_t kSkillBarPendingSkillAddRef = 0x007F7E60;
    constexpr uintptr_t kSkillBarPendingSkillRelease = 0x007F7FF0;

    using SkillBar_SetSlotData_fn = void(__stdcall*)(uint32_t key, uint32_t slot, uint32_t skill_id, uint32_t copy_id);
    using SkillBar_PendingSkill_fn = void(__stdcall*)(uint32_t owner_id, uint32_t skill_id, uint32_t copy_id);
    using SkillBarTable_FindEntry_fn = uint32_t(__fastcall*)(uint32_t key, uint32_t* out_index);

    bool SafeCallSkillBarSetSlotData(SkillBar_SetSlotData_fn fn, uint32_t key, uint32_t slot, uint32_t skill_id, uint32_t copy_id) {
        if (!fn) {
            return false;
        }
        __try {
            fn(key, slot, skill_id, copy_id);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    SkillBar_PendingSkill_fn pending_skill_addref_ret = nullptr;
    SkillBar_PendingSkill_fn pending_skill_release_ret = nullptr;

    uint32_t SafeCallSkillBarTableFindEntry(SkillBarTable_FindEntry_fn fn, uint32_t key, uint32_t* out_index) {
        if (!fn || !out_index) {
            return 0;
        }
        __try {
            return fn(key, out_index);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

struct DialogDecodeRequest {
    uint32_t dialog_id = 0;
    wchar_t* encoded = nullptr;
};

struct DialogBodyDecodeRequest {
    uint64_t tick = 0;
    uint32_t message_id = 0;
    uint32_t dialog_id = 0;
    uint32_t agent_id = 0;
    uint32_t context_dialog_id = 0;
    uint64_t decode_nonce = 0;
    wchar_t* encoded = nullptr;
};

struct DialogButtonDecodeRequest {
    uint64_t tick = 0;
    uint32_t message_id = 0;
    uint32_t dialog_id = 0;
    uint32_t context_dialog_id = 0;
    uint32_t agent_id = 0;
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
            auto* buf = new wchar_t[len + 1];
            std::wmemcpy(buf, src, len + 1);
            return buf;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
}

    void* SafeCallDialogLoader_GetText(DialogMemory::DialogLoader_GetText_fn fn, uint32_t dialog_id) {
        void* result = nullptr;
        __try {
            result = fn(dialog_id);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            result = nullptr;
        }
        return result;
    }

    DialogMemory::DialogLoader_GetText_fn ResolveDialogLoaderGetText() {
        static DialogMemory::DialogLoader_GetText_fn cached = nullptr;
        if (cached) {
            return cached;
        }

        cached = reinterpret_cast<DialogMemory::DialogLoader_GetText_fn>(
            ToRuntimeAddress(DialogMemory::DIALOG_LOADER_GETTEXT));

        return cached;
    }

    struct SectionRange {
        uintptr_t start = 0;
        uintptr_t end = 0;
        bool valid() const { return start && end && end > start; }
    };

    bool GetModuleInfo(uintptr_t& base, size_t& size) {
        HMODULE module = GetModuleHandleW(nullptr);
        if (!module) {
            base = 0;
            size = 0;
            return false;
        }
        base = reinterpret_cast<uintptr_t>(module);
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
            size = 0;
            return false;
        }
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) {
            size = 0;
            return false;
        }
        size = nt->OptionalHeader.SizeOfImage;
        return true;
    }

    SectionRange GetSectionRange(const char* name) {
        SectionRange out{};
        uintptr_t base = 0;
        size_t size = 0;
        if (!GetModuleInfo(base, size)) {
            return out;
        }
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        auto* sections = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
            const auto& sec = sections[i];
            char sec_name[9] = {};
            std::memcpy(sec_name, sec.Name, 8);
            if (std::strncmp(sec_name, name, 8) == 0) {
                uintptr_t start = base + sec.VirtualAddress;
                uintptr_t end = start + std::max(sec.Misc.VirtualSize, sec.SizeOfRawData);
                out.start = start;
                out.end = end;
                return out;
            }
        }
        return out;
    }

    struct DialogTableAddrs {
        uintptr_t flags_base = 0;
        uintptr_t frame_type_base = 0;
        uintptr_t event_handler_base = 0;
        uintptr_t content_id_base = 0;
        uintptr_t property_id_base = 0;
        uintptr_t button_ids_base = 0;
        uintptr_t config_table = 0;
        uintptr_t property_table = 0;
        bool resolved = false;
    };

    uintptr_t ResolveConfigTable(const SectionRange& rdata) {
        if (!rdata.valid()) {
            return 0;
        }
        const size_t count = DialogMemory::MAX_DIALOG_ID + 1;
        for (uintptr_t addr = rdata.start; addr + count <= rdata.end; ++addr) {
            size_t non_zero = 0;
            bool ok = true;
            for (size_t i = 0; i < count; ++i) {
                uint8_t val = 0;
                if (!TryReadU8NoLog(addr + i, val)) {
                    ok = false;
                    break;
                }
                if (val > 7) {
                    ok = false;
                    break;
                }
                if (val != 0) {
                    non_zero++;
                }
            }
            if (ok && non_zero > 0) {
                return addr;
            }
        }
        return 0;
    }

    uintptr_t ResolveFlagsBase(const SectionRange& rdata, const SectionRange& text) {
        if (!rdata.valid() || !text.valid()) {
            return 0;
        }
        const size_t count = DialogMemory::MAX_DIALOG_ID + 1;
        const size_t stride = DialogMemory::FLAGS_STRIDE;
        const uintptr_t start = rdata.start + 8; // allow event handler base at -8
        const uintptr_t end = rdata.end - (count * stride);
        for (uintptr_t addr = start; addr + count * stride <= end; addr += 4) {
            bool ok = true;
            size_t enabled = 0;
            for (size_t i = 0; i < count; ++i) {
                uint32_t flags = 0;
                if (!TryReadU32NoLog(addr + i * stride, flags)) {
                    ok = false;
                    break;
                }
                if (flags > 0xFFFF) {
                    ok = false;
                    break;
                }
                if (flags & 0x1) {
                    enabled++;
                }
                uint32_t handler = 0;
                if (!TryReadU32NoLog(addr - 8 + i * stride, handler)) {
                    ok = false;
                    break;
                }
                if (handler != 0 && (handler < text.start || handler >= text.end)) {
                    ok = false;
                    break;
                }
            }
            if (ok && enabled > 0) {
                return addr;
            }
        }
        return 0;
    }

    uintptr_t ResolvePropertyTable(const SectionRange& data, uintptr_t module_base, size_t module_size) {
        if (!data.valid() || !module_base || !module_size) {
            return 0;
        }
        const size_t count = DialogMemory::MAX_DIALOG_ID + 1;
        const uintptr_t end = data.end - (count * 8);
        for (uintptr_t addr = data.start; addr + count * 8 <= end; addr += 4) {
            bool ok = true;
            size_t non_zero = 0;
            for (size_t i = 0; i < count; ++i) {
                uint32_t ptr = 0;
                if (!TryReadU32NoLog(addr + i * 8, ptr)) {
                    ok = false;
                    break;
                }
                if (ptr == 0) {
                    continue;
                }
                if (ptr < module_base || ptr >= (module_base + module_size)) {
                    ok = false;
                    break;
                }
                non_zero++;
            }
            if (ok && non_zero > 0) {
                return addr;
            }
        }
        return 0;
    }

    DialogTableAddrs& GetDialogTables() {
        static DialogTableAddrs tables;
        if (tables.resolved) {
            return tables;
        }
        tables.resolved = true;

        uintptr_t module_base = 0;
        size_t module_size = 0;
        GetModuleInfo(module_base, module_size);

        SectionRange rdata = GetSectionRange(".rdata");
        SectionRange data = GetSectionRange(".data");
        SectionRange text = GetSectionRange(".text");

        tables.config_table = ResolveConfigTable(rdata);
        tables.flags_base = ResolveFlagsBase(rdata, text);
        if (tables.flags_base) {
            tables.event_handler_base = tables.flags_base - 0x8;
            tables.frame_type_base = tables.flags_base - 0x4;
            tables.content_id_base = tables.flags_base + 0x4;
            tables.property_id_base = tables.flags_base + 0x8;
            tables.button_ids_base = tables.flags_base + 0x10;
        }
        tables.property_table = ResolvePropertyTable(data, module_base, module_size);

        if (!tables.flags_base || !tables.config_table) {
            Logger::LogStaticInfo("[Dialog] Dialog table resolution incomplete. Some dialog metadata may be unavailable.");
        }

        return tables;
    }

    constexpr size_t kMaxDialogEventLogs = 512;
    constexpr size_t kMaxDialogCallbackJournal = 1000;

    void CopyBytesSafe(const void* src, size_t size, std::vector<uint8_t>& out) {
        out.clear();
        if (!src || size == 0) {
            return;
        }
        __try {
            out.resize(size);
            std::memcpy(out.data(), src, size);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            out.clear();
        }
    }

    // defined below as Dialog::AppendDialogEventLog

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
        .def_readwrite("agent_id", &ActiveDialogInfo::agent_id)
        .def_readwrite("message", &ActiveDialogInfo::message);
}

void BindDialogButtonInfo(py::module_& m) {
    py::class_<DialogButtonInfo>(m, "DialogButtonInfo")
        .def(py::init<>())
        .def_readwrite("dialog_id", &DialogButtonInfo::dialog_id)
        .def_readwrite("skill_id", &DialogButtonInfo::skill_id)
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

void BindPendingSkillInfo(py::module_& m) {
    py::class_<PendingSkillInfo>(m, "PendingSkillInfo")
        .def(py::init<>())
        .def_readwrite("skill_id", &PendingSkillInfo::skill_id)
        .def_readwrite("copy_id", &PendingSkillInfo::copy_id)
        .def_readwrite("ref_count", &PendingSkillInfo::ref_count);
}

void BindPendingSkillDebugEvent(py::module_& m) {
    py::class_<PendingSkillDebugEvent>(m, "PendingSkillDebugEvent")
        .def(py::init<>())
        .def_readwrite("owner_id", &PendingSkillDebugEvent::owner_id)
        .def_readwrite("skill_id", &PendingSkillDebugEvent::skill_id)
        .def_readwrite("copy_id", &PendingSkillDebugEvent::copy_id)
        .def_readwrite("added", &PendingSkillDebugEvent::added);
}

void BindPendingSkillFrameEvent(py::module_& m) {
    py::class_<PendingSkillFrameEvent>(m, "PendingSkillFrameEvent")
        .def(py::init<>())
        .def_readwrite("source", &PendingSkillFrameEvent::source)
        .def_readwrite("message_id", &PendingSkillFrameEvent::message_id)
        .def_readwrite("wparam_ptr", &PendingSkillFrameEvent::wparam_ptr)
        .def_readwrite("lparam_ptr", &PendingSkillFrameEvent::lparam_ptr)
        .def_readwrite("w0", &PendingSkillFrameEvent::w0)
        .def_readwrite("w1", &PendingSkillFrameEvent::w1)
        .def_readwrite("w2", &PendingSkillFrameEvent::w2)
        .def_readwrite("w3", &PendingSkillFrameEvent::w3)
        .def_readwrite("w4", &PendingSkillFrameEvent::w4);
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
        .def_readwrite("npc_uid", &DialogCallbackJournalEntry::npc_uid)
        .def_readwrite("event_type", &DialogCallbackJournalEntry::event_type)
        .def_readwrite("text", &DialogCallbackJournalEntry::text);
}

PYBIND11_EMBEDDED_MODULE(PyDialog, m) {
    BindDialogInfo(m);
    BindActiveDialogInfo(m);
    BindDialogButtonInfo(m);
    BindDialogTextDecodedInfo(m);
    BindPendingSkillInfo(m);
    BindPendingSkillDebugEvent(m);
    BindPendingSkillFrameEvent(m);
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
        .def_static("get_pending_skills", &Dialog::GetPendingSkills, py::arg("agent_id") = 0)
        .def_static("get_pending_skill_debug_event", &Dialog::GetPendingSkillDebugEvent)
        .def_static("get_pending_skill_frame_events", &Dialog::GetPendingSkillFrameEvents)
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
        .def_static("accept_offered_skill", &Dialog::AcceptOfferedSkill, py::arg("skill_id"))
        .def_static("accept_offered_skill_replace", &Dialog::AcceptOfferedSkillReplace,
            py::arg("skill_id"), py::arg("slot_index"), py::arg("copy_id") = std::nullopt)
        .def_static("clear_cache", &Dialog::ClearCache)
        .def_static("initialize", &Dialog::Initialize)
        .def_static("terminate", &Dialog::Terminate);
}

// Initialize static members
std::unordered_map<uint32_t, std::string> Dialog::decoded_text_cache;
std::unordered_map<uint32_t, bool> Dialog::decoded_text_pending;
std::mutex Dialog::dialog_mutex;
ActiveDialogInfo Dialog::active_dialog_cache = {0, 0, L""};
std::vector<DialogButtonInfo> Dialog::active_dialog_buttons;
std::unordered_map<uint32_t, std::string> Dialog::decoded_button_label_cache;
std::unordered_map<uint32_t, bool> Dialog::decoded_button_label_pending;
uint32_t Dialog::last_dialog_id = 0;
bool Dialog::dialog_hook_registered = false;
GW::HookEntry Dialog::dialog_ui_message_entry_body;
GW::HookEntry Dialog::dialog_ui_message_entry_button;
GW::HookEntry Dialog::dialog_ui_message_entry_send_agent;
GW::HookEntry Dialog::dialog_ui_message_entry_send_gadget;
uint32_t Dialog::last_selected_dialog_id = 0;
std::vector<DialogEventLog> Dialog::dialog_event_logs;
std::vector<DialogEventLog> Dialog::dialog_event_logs_received;
std::vector<DialogEventLog> Dialog::dialog_event_logs_sent;
std::vector<DialogCallbackJournalEntry> Dialog::dialog_callback_journal;
std::vector<DialogCallbackJournalEntry> Dialog::dialog_callback_journal_received;
std::vector<DialogCallbackJournalEntry> Dialog::dialog_callback_journal_sent;
std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> Dialog::pending_skill_cache;
PendingSkillDebugEvent Dialog::pending_skill_debug_event = {0, 0, 0, 0};
std::vector<PendingSkillFrameEvent> Dialog::pending_skill_frame_events;
std::array<GW::HookEntry, 10> Dialog::pending_skill_frame_entries;
std::array<GW::HookEntry, 10> Dialog::pending_skill_ui_entries;
uint64_t Dialog::active_dialog_body_decode_nonce = 0;

void Dialog::Initialize() {
    {
        // Clear all caches on initialization
        std::scoped_lock lock(dialog_mutex);
        active_dialog_cache = {0, 0, L""};
        active_dialog_buttons.clear();
        last_dialog_id = 0;
        pending_skill_cache.clear();
        active_dialog_body_decode_nonce = 0;
    }
    RegisterDialogUiHooks();
}

void Dialog::Terminate() {
    UnregisterDialogUiHooks();
    // Clean up caches on shutdown
    ClearCache();
}

    // ================= Synchronous Methods (Direct Memory Access) =================

    bool Dialog::IsDialogAvailable(uint32_t dialog_id) {
        // Validate dialog ID range (0x00-0x39 = 58 dialogs max)
        if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
            return false;
        }

        uint32_t flags = ReadDialogFlags(dialog_id);
        // Bit 0 indicates if dialog is enabled
        return (flags & 0x1) != 0;
    }

    DialogInfo Dialog::GetDialogInfo(uint32_t dialog_id) {
        DialogInfo info = {dialog_id, 0, 0, 0, 0, 0, L"", 0};

        if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
            return info;
        }

        // Read all metadata
        info.flags = ReadDialogFlags(dialog_id);
        info.frame_type = ReadDialogFrameType(dialog_id);
        info.event_handler = ReadDialogEventHandler(dialog_id);
        info.content_id = ReadDialogContentId(dialog_id);
        info.property_id = ReadDialogPropertyId(dialog_id);

        return info;
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
                label = GetDialogTextDecoded(btn.dialog_id);
                pending = IsDialogTextDecodePending(btn.dialog_id);
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
        const uint32_t frame_id = GW::UI::GetFrameIDByHash(kNpcDialogHash);
        if (!frame_id) {
            return false;
        }
        const GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
        if (!frame) {
            return false;
        }
        return frame->IsCreated() && frame->IsVisible();
    }

    bool Dialog::IsDialogDisplayed(uint32_t dialog_id) {
        std::scoped_lock lock(dialog_mutex);
        return dialog_id != 0 && active_dialog_cache.dialog_id == dialog_id;
    }

    std::vector<DialogInfo> Dialog::EnumerateAvailableDialogs() {
        std::vector<DialogInfo> dialogs;
        dialogs.reserve(DialogMemory::MAX_DIALOG_ID + 1);
        for (uint32_t dialog_id = 0; dialog_id <= DialogMemory::MAX_DIALOG_ID; ++dialog_id) {
            if (IsDialogAvailable(dialog_id)) {
                dialogs.push_back(GetDialogInfo(dialog_id));
            }
        }
        return dialogs;
    }

std::string Dialog::GetDialogTextDecoded(uint32_t dialog_id) {
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return {};
    }
    {
        std::scoped_lock lock(dialog_mutex);
        auto it = decoded_text_cache.find(dialog_id);
        if (it != decoded_text_cache.end()) {
            return it->second;
        }
        auto pending = decoded_text_pending.find(dialog_id);
        if (pending != decoded_text_pending.end() && pending->second) {
            return {};
        }
    }
    QueueDialogTextDecode(dialog_id);
    return {};
}

    bool Dialog::IsDialogTextDecodePending(uint32_t dialog_id) {
        std::scoped_lock lock(dialog_mutex);
        auto it = decoded_text_pending.find(dialog_id);
        if (it == decoded_text_pending.end()) {
            return false;
        }
        return it->second;
    }

    std::vector<DialogTextDecodedInfo> Dialog::GetDecodedDialogTextStatus() {
        std::scoped_lock lock(dialog_mutex);
        std::vector<DialogTextDecodedInfo> out;
        out.reserve(decoded_text_cache.size() + decoded_text_pending.size());
        for (const auto& kv : decoded_text_cache) {
            DialogTextDecodedInfo info{};
            info.dialog_id = kv.first;
            info.text = kv.second;
            info.pending = false;
            out.push_back(std::move(info));
        }
        for (const auto& kv : decoded_text_pending) {
            if (!kv.second) {
                continue;
            }
            if (decoded_text_cache.find(kv.first) != decoded_text_cache.end()) {
                continue;
            }
            DialogTextDecodedInfo info{};
            info.dialog_id = kv.first;
            info.text.clear();
            info.pending = true;
            out.push_back(std::move(info));
        }
        return out;
    }

    void Dialog::ClearCache() {
        std::scoped_lock lock(dialog_mutex);
        active_dialog_cache = {0, 0, L""};
        active_dialog_buttons.clear();
        last_dialog_id = 0;
        last_selected_dialog_id = 0;
        decoded_text_cache.clear();
        decoded_text_pending.clear();
        decoded_button_label_cache.clear();
        decoded_button_label_pending.clear();
        pending_skill_cache.clear();
        pending_skill_debug_event = {0, 0, 0, 0};
        dialog_event_logs.clear();
        dialog_event_logs_received.clear();
        dialog_event_logs_sent.clear();
        dialog_callback_journal.clear();
        dialog_callback_journal_received.clear();
        dialog_callback_journal_sent.clear();
        active_dialog_body_decode_nonce = 0;
    }

    void Dialog::QueueDialogTextDecode(uint32_t dialog_id) {
        if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
            return;
        }
        {
            std::scoped_lock lock(dialog_mutex);
            if (decoded_text_cache.find(dialog_id) != decoded_text_cache.end()) {
                return;
            }
            auto pending = decoded_text_pending.find(dialog_id);
            if (pending != decoded_text_pending.end() && pending->second) {
                return;
            }
            decoded_text_pending[dialog_id] = true;
        }

        DialogMemory::DialogLoader_GetText_fn DialogLoader_GetText = ResolveDialogLoaderGetText();
        if (!DialogLoader_GetText) {
            std::scoped_lock lock(dialog_mutex);
            decoded_text_cache[dialog_id] = {};
            decoded_text_pending[dialog_id] = false;
            return;
        }

        auto* encoded_ptr = static_cast<const wchar_t*>(SafeCallDialogLoader_GetText(DialogLoader_GetText, dialog_id));
        if (!encoded_ptr) {
            std::scoped_lock lock(dialog_mutex);
            decoded_text_cache[dialog_id] = {};
            decoded_text_pending[dialog_id] = false;
            return;
        }

        wchar_t* encoded_copy = DupWideStringSafe(encoded_ptr);
        if (!encoded_copy) {
            std::scoped_lock lock(dialog_mutex);
            decoded_text_cache[dialog_id] = {};
            decoded_text_pending[dialog_id] = false;
            return;
        }

        if (!SafeIsValidEncStr(encoded_copy)) {
            std::scoped_lock lock(dialog_mutex);
            decoded_text_cache[dialog_id] = WideToUtf8Safe(encoded_copy);
            decoded_text_pending[dialog_id] = false;
            delete[] encoded_copy;
            return;
        }

        auto* req = new DialogDecodeRequest();
        req->dialog_id = dialog_id;
        req->encoded = encoded_copy;

        GW::UI::AsyncDecodeStr(req->encoded, Dialog::OnDialogTextDecoded, req);
    }

    // ================= Internal Memory Access Functions =================

    uint32_t Dialog::ReadDialogFlags(uint32_t dialog_id) {
        if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
            return 0;
        }

        uintptr_t flags_base = ToRuntimeAddress(DialogMemory::FLAGS_BASE);
        uint32_t offset = dialog_id * DialogMemory::FLAGS_STRIDE;
        uint32_t address = static_cast<uint32_t>(flags_base + offset);
        uint32_t flags = 0;
        if (!TryReadU32(address, flags, "FLAGS")) {
            return 0;
        }
        return flags;
    }

    uint32_t Dialog::ReadDialogFrameType(uint32_t dialog_id) {
        if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
            return 0;
        }

        uintptr_t frame_base = ToRuntimeAddress(DialogMemory::FRAME_TYPE_BASE);
        uint32_t offset = dialog_id * DialogMemory::FLAGS_STRIDE;
        uint32_t address = static_cast<uint32_t>(frame_base + offset);
        uint32_t frame_type = 0;
        if (!TryReadU32(address, frame_type, "FRAME_TYPE")) {
            return 0;
        }
        return frame_type;
    }

    uint32_t Dialog::ReadDialogEventHandler(uint32_t dialog_id) {
        if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
            return 0;
        }

        uintptr_t handler_base = ToRuntimeAddress(DialogMemory::EVENT_HANDLER_BASE);
        uint32_t offset = dialog_id * DialogMemory::FLAGS_STRIDE;
        uint32_t address = static_cast<uint32_t>(handler_base + offset);
        uint32_t handler = 0;
        if (!TryReadU32(address, handler, "EVENT_HANDLER")) {
            return 0;
        }
        return handler;
    }

    uint32_t Dialog::ReadDialogContentId(uint32_t dialog_id) {
        if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
            return 0;
        }

        uintptr_t content_base = ToRuntimeAddress(DialogMemory::CONTENT_ID_BASE);
        uint32_t offset = dialog_id * DialogMemory::CONTENT_STRIDE;
        uint32_t address = static_cast<uint32_t>(content_base + offset);
        uint32_t content_id = 0;
        if (!TryReadU32(address, content_id, "CONTENT_ID")) {
            return 0;
        }
        return content_id;
    }

    uint32_t Dialog::ReadDialogPropertyId(uint32_t dialog_id) {
        if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
            return 0;
        }

        uintptr_t property_base = ToRuntimeAddress(DialogMemory::PROPERTY_ID_BASE);
        uint32_t offset = dialog_id * DialogMemory::PROPERTY_STRIDE;
        uint32_t address = static_cast<uint32_t>(property_base + offset);
        uint32_t property_id = 0;
        if (!TryReadU32(address, property_id, "PROPERTY_ID")) {
            return 0;
        }
        return property_id;
    }

    uint8_t Dialog::ReadDialogConfigType(uint32_t dialog_id) {
        if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
            return 0;
        }

        uintptr_t config_base = ToRuntimeAddress(DialogMemory::CONFIG_TABLE);
        uint32_t address = static_cast<uint32_t>(config_base + dialog_id);
        uint8_t config_type = 0;
        if (!TryReadU8(address, config_type, "CONFIG_TYPE")) {
            return 0;
        }
        return config_type;
    }

    void* Dialog::ReadDialogProperties(uint32_t dialog_id) {
        if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
            return nullptr;
        }

        uintptr_t property_table = ToRuntimeAddress(DialogMemory::PROPERTY_TABLE);
        uint32_t offset = dialog_id * 8;
        uint32_t address = static_cast<uint32_t>(property_table + offset);
        void* properties = nullptr;
        if (!TryReadPtr(address, properties, "PROPERTIES")) {
            return nullptr;
        }
        return properties;
    }

    void Dialog::RegisterDialogUiHooks() {
        {
            std::scoped_lock lock(dialog_mutex);
            if (dialog_hook_registered) {
                return;
            }
            dialog_hook_registered = true;
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
        RegisterPendingSkillHooks();
    }

    void Dialog::UnregisterDialogUiHooks() {
        {
            std::scoped_lock lock(dialog_mutex);
            if (!dialog_hook_registered) {
                return;
            }
            dialog_hook_registered = false;
        }
        GW::UI::RemoveUIMessageCallback(&dialog_ui_message_entry_body, GW::UI::UIMessage::kDialogBody);
        GW::UI::RemoveUIMessageCallback(&dialog_ui_message_entry_button, GW::UI::UIMessage::kDialogButton);
        GW::UI::RemoveUIMessageCallback(&dialog_ui_message_entry_send_agent, GW::UI::UIMessage::kSendAgentDialog);
        GW::UI::RemoveUIMessageCallback(&dialog_ui_message_entry_send_gadget, GW::UI::UIMessage::kSendGadgetDialog);
        UnregisterPendingSkillHooks();
    }

    void Dialog::RegisterPendingSkillHooks() {
        const std::array<uint32_t, 10> msg_ids = {
            0x10000058u,
            0x10000059u,
            0x1000005au,
            0x1000005bu,
            0x1000005cu,
            0x1000005du,
            0x1000005eu,
            0x10000141u,
            0x10000190u,
            0x1000019fu,
        };
        for (size_t i = 0; i < msg_ids.size(); ++i) {
            GW::UI::RegisterFrameUIMessageCallback(
                &pending_skill_frame_entries[i],
                static_cast<GW::UI::UIMessage>(msg_ids[i]),
                Dialog::OnPendingSkillFrameMessage,
                0x1);
            GW::UI::RegisterUIMessageCallback(
                &pending_skill_ui_entries[i],
                static_cast<GW::UI::UIMessage>(msg_ids[i]),
                Dialog::OnPendingSkillUiMessage,
                0x1);
        }
    }

    void Dialog::UnregisterPendingSkillHooks() {
        for (auto& entry : pending_skill_frame_entries) {
            GW::UI::RemoveFrameUIMessageCallback(&entry);
        }
        for (auto& entry : pending_skill_ui_entries) {
            GW::UI::RemoveUIMessageCallback(&entry, GW::UI::UIMessage::kNone);
        }
        pending_skill_addref_ret = nullptr;
        pending_skill_release_ret = nullptr;
    }

    bool Dialog::AcceptOfferedSkill(uint32_t skill_id) {
        if (skill_id == 0) {
            return false;
        }
        const uint32_t raw_dialog_id = skill_id | 0x0A000000;
        GW::GameThread::Enqueue([raw_dialog_id]() {
            GW::Agents::SendDialog(raw_dialog_id);
        });
        return true;
    }

    bool Dialog::AcceptOfferedSkillReplace(uint32_t skill_id, uint32_t slot_index, std::optional<uint32_t> copy_id) {
        if (skill_id == 0 || slot_index > 7) {
            return false;
        }

        uint32_t resolved_copy_id = 0;
        bool have_copy_id = false;
        if (copy_id.has_value()) {
            resolved_copy_id = copy_id.value();
            have_copy_id = true;
        }
        else {
            have_copy_id = TryGetPendingCopyId(0, skill_id, resolved_copy_id);
        }

        const uint32_t raw_dialog_id = skill_id | 0x0A000000;
        const uint32_t slot = slot_index;
        const uint32_t skill = skill_id;
        const uint32_t copy = resolved_copy_id;
        const bool do_replace = have_copy_id;

        GW::GameThread::Enqueue([raw_dialog_id, slot, skill, copy, do_replace]() {
            GW::Agents::SendDialog(raw_dialog_id);
            if (!do_replace) {
                return;
            }
            SkillBar_SetSlotData_fn set_slot_fn =
                reinterpret_cast<SkillBar_SetSlotData_fn>(ToRuntimeAddress(kSkillBarSetSlotData));
            if (!set_slot_fn) {
                return;
            }
            uint32_t agent_id = GW::Agents::GetControlledCharacterId();
            if (!agent_id) {
                return;
            }
            SafeCallSkillBarSetSlotData(set_slot_fn, agent_id, slot, skill, copy);
        });

        return do_replace;
    }

    std::vector<PendingSkillInfo> Dialog::GetPendingSkills(uint32_t agent_id) {
        return ReadPendingSkills(agent_id);
    }

    PendingSkillDebugEvent Dialog::GetPendingSkillDebugEvent() {
        std::scoped_lock lock(dialog_mutex);
        return pending_skill_debug_event;
    }

    std::vector<PendingSkillFrameEvent> Dialog::GetPendingSkillFrameEvents() {
        std::scoped_lock lock(dialog_mutex);
        return pending_skill_frame_events;
    }

    void __stdcall Dialog::OnPendingSkillAddRef(uint32_t owner_id, uint32_t skill_id, uint32_t copy_id) {
        if (pending_skill_addref_ret) {
            pending_skill_addref_ret(owner_id, skill_id, copy_id);
        }
        const uint32_t key = (skill_id << 16) | (copy_id & 0xFFFF);
        std::scoped_lock lock(dialog_mutex);
        pending_skill_cache[owner_id][key] += 1;
        pending_skill_debug_event = {owner_id, skill_id, copy_id, 1};
    }

    void __stdcall Dialog::OnPendingSkillRelease(uint32_t owner_id, uint32_t skill_id, uint32_t copy_id) {
        if (pending_skill_release_ret) {
            pending_skill_release_ret(owner_id, skill_id, copy_id);
        }
        const uint32_t key = (skill_id << 16) | (copy_id & 0xFFFF);
        std::scoped_lock lock(dialog_mutex);
        auto owner_it = pending_skill_cache.find(owner_id);
        if (owner_it == pending_skill_cache.end()) {
            return;
        }
        auto& entries = owner_it->second;
        auto it = entries.find(key);
        if (it == entries.end()) {
            return;
        }
        if (it->second > 1) {
            it->second -= 1;
        }
        else {
            entries.erase(it);
            if (entries.empty()) {
                pending_skill_cache.erase(owner_it);
            }
        }
        pending_skill_debug_event = {owner_id, skill_id, copy_id, 0};
    }

    void Dialog::OnPendingSkillFrameMessage(GW::HookStatus*, const GW::UI::Frame*, GW::UI::UIMessage msg, void* wparam, void* lparam) {
        if (!wparam) {
            return;
        }
        uint32_t w0 = 0;
        uint32_t w1 = 0;
        uint32_t w2 = 0;
        uint32_t w3 = 0;
        uint32_t w4 = 0;
        const uintptr_t wparam_addr = reinterpret_cast<uintptr_t>(wparam);
        const bool ok0 = TryReadU32NoLog(wparam_addr, w0);
        const bool ok1 = TryReadU32NoLog(wparam_addr + 4, w1);
        const bool ok2 = TryReadU32NoLog(wparam_addr + 8, w2);
        const bool ok3 = TryReadU32NoLog(wparam_addr + 12, w3);
        const bool ok4 = TryReadU32NoLog(wparam_addr + 16, w4);
        if (!ok0 && !ok1 && !ok2 && !ok3 && !ok4) {
            w0 = static_cast<uint32_t>(wparam_addr);
            w1 = 0;
            w2 = 0;
            w3 = 0;
            w4 = 0;
        }

        std::scoped_lock lock(dialog_mutex);
        PendingSkillFrameEvent evt{};
        evt.source = 0;
        evt.message_id = static_cast<uint32_t>(msg);
        evt.wparam_ptr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam));
        evt.lparam_ptr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lparam));
        evt.w0 = w0;
        evt.w1 = w1;
        evt.w2 = w2;
        evt.w3 = w3;
        evt.w4 = w4;
        pending_skill_frame_events.push_back(evt);
        if (pending_skill_frame_events.size() > 32) {
            pending_skill_frame_events.erase(pending_skill_frame_events.begin(),
                pending_skill_frame_events.begin() + (pending_skill_frame_events.size() - 32));
        }

        if (w0 != 0 && w1 != 0) {
            const uint32_t key = (w1 << 16) | (w2 & 0xFFFF);
            pending_skill_cache[w0][key] = 1;
            pending_skill_debug_event = {w0, w1, w2, 1};
        }
    }

    void Dialog::OnPendingSkillUiMessage(GW::HookStatus*, GW::UI::UIMessage msg, void* wparam, void* lparam) {
        if (!wparam) {
            return;
        }
        uint32_t w0 = 0;
        uint32_t w1 = 0;
        uint32_t w2 = 0;
        uint32_t w3 = 0;
        uint32_t w4 = 0;
        const uintptr_t wparam_addr = reinterpret_cast<uintptr_t>(wparam);
        const bool ok0 = TryReadU32NoLog(wparam_addr, w0);
        const bool ok1 = TryReadU32NoLog(wparam_addr + 4, w1);
        const bool ok2 = TryReadU32NoLog(wparam_addr + 8, w2);
        const bool ok3 = TryReadU32NoLog(wparam_addr + 12, w3);
        const bool ok4 = TryReadU32NoLog(wparam_addr + 16, w4);
        if (!ok0 && !ok1 && !ok2 && !ok3 && !ok4) {
            w0 = static_cast<uint32_t>(wparam_addr);
            w1 = 0;
            w2 = 0;
            w3 = 0;
            w4 = 0;
        }

        std::scoped_lock lock(dialog_mutex);
        PendingSkillFrameEvent evt{};
        evt.source = 1;
        evt.message_id = static_cast<uint32_t>(msg);
        evt.wparam_ptr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam));
        evt.lparam_ptr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lparam));
        evt.w0 = w0;
        evt.w1 = w1;
        evt.w2 = w2;
        evt.w3 = w3;
        evt.w4 = w4;
        pending_skill_frame_events.push_back(evt);
        if (pending_skill_frame_events.size() > 32) {
            pending_skill_frame_events.erase(pending_skill_frame_events.begin(),
                pending_skill_frame_events.begin() + (pending_skill_frame_events.size() - 32));
        }

        if (w0 != 0 && w1 != 0) {
            const uint32_t key = (w1 << 16) | (w2 & 0xFFFF);
            pending_skill_cache[w0][key] = 1;
            pending_skill_debug_event = {w0, w1, w2, 1};
        }
    }

    void Dialog::OnDialogUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
        if (!wparam) {
            return;
        }

        switch (message_id) {
            case GW::UI::UIMessage::kDialogButton: {
                const auto* info = static_cast<GW::UI::DialogButtonInfo*>(wparam);
                if (!info) {
                    return;
                }
                const uint64_t tick = GetTickCount64();
                uint32_t context_dialog_id = 0;
                uint32_t context_agent_id = 0;
                {
                    std::scoped_lock lock(dialog_mutex);
                    context_dialog_id = active_dialog_cache.dialog_id;
                    context_agent_id = active_dialog_cache.agent_id;
                }
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
                            auto* req = new DialogButtonDecodeRequest();
                            req->tick = tick;
                            req->message_id = static_cast<uint32_t>(message_id);
                            req->dialog_id = info->dialog_id;
                            req->context_dialog_id = context_dialog_id;
                            req->agent_id = context_agent_id;
                            req->encoded = encoded_copy;
                            {
                                std::scoped_lock lock(dialog_mutex);
                                decoded_button_label_pending[info->dialog_id] = true;
                            }
                            GW::UI::AsyncDecodeStr(req->encoded, Dialog::OnDialogButtonDecoded, req);
                            label_pending = true;
                        }
                    }
                }

                {
                    std::scoped_lock lock(dialog_mutex);
                    if (!label_utf8.empty()) {
                        decoded_button_label_cache[info->dialog_id] = label_utf8;
                        decoded_button_label_pending[info->dialog_id] = false;
                    }
                    DialogButtonInfo button{};
                    button.dialog_id = info->dialog_id;
                    button.skill_id = info->skill_id;
                    button.button_icon = info->button_icon;
                    button.message = label_utf8;
                    button.message_decoded = label_utf8;
                    button.message_decode_pending = label_pending;
                    active_dialog_buttons.push_back(std::move(button));
                }
                if (!label_pending) {
                    Dialog::AppendDialogCallbackJournalEntry(
                        tick,
                        static_cast<uint32_t>(message_id),
                        true,
                        "recv_choice",
                        info->dialog_id,
                        context_dialog_id,
                        context_agent_id,
                        label_utf8
                    );
                }
            } break;
            case GW::UI::UIMessage::kDialogBody: {
                const auto* info = static_cast<GW::UI::DialogBodyInfo*>(wparam);
                if (!info) {
                    return;
                }
                const uint64_t tick = GetTickCount64();

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
                uint64_t decode_nonce = 0;
                {
                    std::scoped_lock lock(dialog_mutex);
                    active_dialog_cache.agent_id = info->agent_id;
                    context_dialog_id = last_selected_dialog_id;
                    active_dialog_cache.dialog_id = context_dialog_id;
                    active_dialog_cache.message.clear();
                    active_dialog_buttons.clear();
                    decode_nonce = ++active_dialog_body_decode_nonce;
                }

                bool append_immediate = true;
                std::string immediate_text;

                if (info->message_enc) {
                    wchar_t* encoded_copy = DupWideStringSafe(info->message_enc);
                    if (encoded_copy) {
                        if (!SafeIsValidEncStr(encoded_copy)) {
                            const std::wstring plain_text(encoded_copy);
                            immediate_text = WideToUtf8Safe(encoded_copy);
                            delete[] encoded_copy;
                            {
                                std::scoped_lock lock(dialog_mutex);
                                if (active_dialog_cache.agent_id == info->agent_id &&
                                    active_dialog_body_decode_nonce == decode_nonce) {
                                    active_dialog_cache.message = plain_text;
                                }
                            }
                        }
                        else {
                            auto* req = new DialogBodyDecodeRequest();
                            req->tick = tick;
                            req->message_id = static_cast<uint32_t>(message_id);
                            req->dialog_id = context_dialog_id;
                            req->agent_id = info->agent_id;
                            req->context_dialog_id = context_dialog_id;
                            req->decode_nonce = decode_nonce;
                            req->encoded = encoded_copy;
                            GW::UI::AsyncDecodeStr(req->encoded, Dialog::OnDialogBodyDecoded, req);
                            append_immediate = false;
                        }
                    }
                }
                if (append_immediate) {
                    Dialog::AppendDialogCallbackJournalEntry(
                        tick,
                        static_cast<uint32_t>(message_id),
                        true,
                        "recv_body",
                        context_dialog_id,
                        context_dialog_id,
                        info->agent_id,
                        immediate_text
                    );
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
                {
                    std::scoped_lock lock(dialog_mutex);
                    context_dialog_id = active_dialog_cache.dialog_id;
                    context_agent_id = active_dialog_cache.agent_id;
                    auto label_it = decoded_button_label_cache.find(selected_id);
                    if (label_it != decoded_button_label_cache.end()) {
                        sent_text = label_it->second;
                    }
                    else {
                        auto text_it = decoded_text_cache.find(selected_id);
                        if (text_it != decoded_text_cache.end()) {
                            sent_text = text_it->second;
                        }
                    }
                    last_selected_dialog_id = selected_id;
                    last_dialog_id = selected_id;
                    active_dialog_cache.dialog_id = selected_id;
                }
                Dialog::AppendDialogCallbackJournalEntry(
                    tick,
                    static_cast<uint32_t>(message_id),
                    false,
                    "sent_choice",
                    selected_id,
                    context_dialog_id,
                    context_agent_id,
                    sent_text
                );
            } break;
            default:
                break;
        }

    }

    std::vector<PendingSkillInfo> Dialog::ReadPendingSkills(uint32_t agent_id) {
        std::vector<PendingSkillInfo> out;

        std::scoped_lock lock(dialog_mutex);
        if (agent_id == 0) {
            for (const auto& owner_entry : pending_skill_cache) {
                for (const auto& kv : owner_entry.second) {
                    const uint32_t key = kv.first;
                    PendingSkillInfo info{};
                    info.skill_id = key >> 16;
                    info.copy_id = key & 0xFFFF;
                    info.ref_count = kv.second;
                    out.push_back(info);
                }
            }
            return out;
        }

        auto owner_it = pending_skill_cache.find(agent_id);
        if (owner_it != pending_skill_cache.end() && !owner_it->second.empty()) {
            out.reserve(owner_it->second.size());
            for (const auto& kv : owner_it->second) {
                const uint32_t key = kv.first;
                PendingSkillInfo info{};
                info.skill_id = key >> 16;
                info.copy_id = key & 0xFFFF;
                info.ref_count = kv.second;
                out.push_back(info);
            }
        }
        return out;
    }

    bool Dialog::TryGetPendingCopyId(uint32_t agent_id, uint32_t skill_id, uint32_t& out_copy_id) {
        out_copy_id = 0;
        if (skill_id == 0) {
            return false;
        }
        auto pending = ReadPendingSkills(agent_id);
        for (const auto& entry : pending) {
            if (entry.skill_id == skill_id) {
                out_copy_id = entry.copy_id;
                return out_copy_id != 0;
            }
        }
        return false;
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
    }

    void Dialog::AppendDialogCallbackJournalEntry(
        uint64_t tick,
        uint32_t message_id,
        bool incoming,
        const char* event_type,
        uint32_t dialog_id,
        uint32_t context_dialog_id,
        uint32_t agent_id,
        const std::string& text
    ) {
        DialogCallbackJournalEntry entry{};
        entry.tick = tick ? tick : GetTickCount64();
        entry.message_id = message_id;
        entry.incoming = incoming;
        entry.dialog_id = dialog_id;
        entry.context_dialog_id = context_dialog_id;
        entry.agent_id = agent_id;
        entry.map_id = GetCurrentMapIdSafe();
        entry.model_id = GetAgentModelIdSafe(agent_id);
        entry.npc_uid = BuildNpcUid(entry.map_id, entry.model_id, agent_id);
        entry.event_type = event_type ? event_type : "";
        entry.text = text;

        std::scoped_lock lock(dialog_mutex);
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
        std::scoped_lock lock(dialog_mutex);
        return dialog_callback_journal;
    }

    std::vector<DialogCallbackJournalEntry> Dialog::GetDialogCallbackJournalReceived() {
        std::scoped_lock lock(dialog_mutex);
        return dialog_callback_journal_received;
    }

    std::vector<DialogCallbackJournalEntry> Dialog::GetDialogCallbackJournalSent() {
        std::scoped_lock lock(dialog_mutex);
        return dialog_callback_journal_sent;
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

void __cdecl Dialog::OnDialogTextDecoded(void* param, const wchar_t* s) {
    auto* req = static_cast<DialogDecodeRequest*>(param);
    if (!req) {
        return;
    }
        {
            std::scoped_lock lock(dialog_mutex);
            decoded_text_cache[req->dialog_id] = WideToUtf8Safe(s);
            decoded_text_pending[req->dialog_id] = false;
        }
    delete[] req->encoded;
    delete req;
}

void __cdecl Dialog::OnDialogBodyDecoded(void* param, const wchar_t* s) {
    auto* req = static_cast<DialogBodyDecodeRequest*>(param);
    if (!req) {
        return;
    }
    const std::string decoded_text = WideToUtf8Safe(s);
    {
        std::scoped_lock lock(dialog_mutex);
        if (active_dialog_cache.agent_id == req->agent_id &&
            active_dialog_body_decode_nonce == req->decode_nonce) {
            active_dialog_cache.message = s ? s : L"";
        }
    }
    Dialog::AppendDialogCallbackJournalEntry(
        req->tick,
        req->message_id,
        true,
        "recv_body",
        req->dialog_id,
        req->context_dialog_id,
        req->agent_id,
        decoded_text
    );
    delete[] req->encoded;
    delete req;
}

void __cdecl Dialog::OnDialogButtonDecoded(void* param, const wchar_t* s) {
    auto* req = static_cast<DialogButtonDecodeRequest*>(param);
    if (!req) {
        return;
    }
    const std::string decoded_label = WideToUtf8Safe(s);
    {
        std::scoped_lock lock(dialog_mutex);
        decoded_button_label_cache[req->dialog_id] = decoded_label;
        decoded_button_label_pending[req->dialog_id] = false;
    }
    Dialog::AppendDialogCallbackJournalEntry(
        req->tick,
        req->message_id,
        true,
        "recv_choice",
        req->dialog_id,
        req->context_dialog_id,
        req->agent_id,
        decoded_label
    );
    delete[] req->encoded;
    delete req;
}

ActiveDialogInfo Dialog::ReadActiveDialog() {
    std::scoped_lock lock(dialog_mutex);
    return active_dialog_cache;
}
