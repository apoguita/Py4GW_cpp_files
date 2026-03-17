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

struct DialogDecodeRequest {
    uint32_t dialog_id = 0;
    uint64_t decode_epoch = 0;
    wchar_t* encoded = nullptr;
};

struct DialogBodyDecodeRequest {
    uint64_t tick = 0;
    uint32_t message_id = 0;
    uint32_t dialog_id = 0;
    uint32_t agent_id = 0;
    uint32_t context_dialog_id = 0;
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

    constexpr auto kDialogAsyncDrainTimeout = std::chrono::milliseconds(500);

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

    bool ValidateDialogConfigTable(uintptr_t config_table) {
        if (!config_table) {
            return false;
        }
        const size_t count = DialogMemory::MAX_DIALOG_ID + 1;
        size_t non_zero = 0;
        for (size_t i = 0; i < count; ++i) {
            uint8_t val = 0;
            if (!TryReadU8NoLog(config_table + i, val)) {
                return false;
            }
            if (val > 7) {
                return false;
            }
            if (val != 0) {
                non_zero++;
            }
        }
        return non_zero > 0;
    }

    bool ValidateDialogMetadataBases(
        uintptr_t flags_base,
        uintptr_t frame_type_base,
        uintptr_t event_handler_base,
        uintptr_t content_id_base,
        uintptr_t property_id_base,
        const SectionRange& text) {
        if (!flags_base || !frame_type_base || !event_handler_base || !content_id_base || !property_id_base || !text.valid()) {
            return false;
        }
        const size_t count = DialogMemory::MAX_DIALOG_ID + 1;
        size_t enabled = 0;
        for (size_t i = 0; i < count; ++i) {
            const uintptr_t offset = i * DialogMemory::FLAGS_STRIDE;
            uint32_t flags = 0;
            uint32_t handler = 0;
            uint32_t frame_type = 0;
            uint32_t content_id = 0;
            uint32_t property_id = 0;
            if (!TryReadU32NoLog(flags_base + offset, flags) ||
                !TryReadU32NoLog(event_handler_base + offset, handler) ||
                !TryReadU32NoLog(frame_type_base + offset, frame_type) ||
                !TryReadU32NoLog(content_id_base + i * DialogMemory::CONTENT_STRIDE, content_id) ||
                !TryReadU32NoLog(property_id_base + i * DialogMemory::PROPERTY_STRIDE, property_id)) {
                return false;
            }
            if (flags > 0xFFFF) {
                return false;
            }
            if (handler != 0 && (handler < text.start || handler >= text.end)) {
                return false;
            }
            if (flags & 0x1) {
                enabled++;
            }
        }
        return enabled > 0;
    }

    bool ValidateDialogPropertyTable(uintptr_t property_table, uintptr_t module_base, size_t module_size) {
        if (!property_table || !module_base || !module_size) {
            return false;
        }
        const size_t count = DialogMemory::MAX_DIALOG_ID + 1;
        size_t non_zero = 0;
        for (size_t i = 0; i < count; ++i) {
            uint32_t ptr = 0;
            if (!TryReadU32NoLog(property_table + i * 8, ptr)) {
                return false;
            }
            if (ptr == 0) {
                continue;
            }
            if (ptr < module_base || ptr >= (module_base + module_size)) {
                return false;
            }
            non_zero++;
        }
        return non_zero > 0;
    }

    DialogTableAddrs BuildStaticDialogTables(const SectionRange& text, uintptr_t module_base, size_t module_size) {
        DialogTableAddrs tables{};
        tables.flags_base = ToRuntimeAddress(DialogMemory::FLAGS_BASE);
        tables.frame_type_base = ToRuntimeAddress(DialogMemory::FRAME_TYPE_BASE);
        tables.event_handler_base = ToRuntimeAddress(DialogMemory::EVENT_HANDLER_BASE);
        tables.content_id_base = ToRuntimeAddress(DialogMemory::CONTENT_ID_BASE);
        tables.property_id_base = ToRuntimeAddress(DialogMemory::PROPERTY_ID_BASE);
        tables.button_ids_base = ToRuntimeAddress(DialogMemory::BUTTON_IDS_BASE);
        tables.config_table = ToRuntimeAddress(DialogMemory::CONFIG_TABLE);
        tables.property_table = ToRuntimeAddress(DialogMemory::PROPERTY_TABLE);

        if (!ValidateDialogMetadataBases(
                tables.flags_base,
                tables.frame_type_base,
                tables.event_handler_base,
                tables.content_id_base,
                tables.property_id_base,
                text)) {
            tables.flags_base = 0;
            tables.frame_type_base = 0;
            tables.event_handler_base = 0;
            tables.content_id_base = 0;
            tables.property_id_base = 0;
            tables.button_ids_base = 0;
        }
        if (!ValidateDialogConfigTable(tables.config_table)) {
            tables.config_table = 0;
        }
        if (!ValidateDialogPropertyTable(tables.property_table, module_base, module_size)) {
            tables.property_table = 0;
        }
        return tables;
    }

    DialogTableAddrs BuildResolvedDialogTables(
        const SectionRange& rdata,
        const SectionRange& data,
        const SectionRange& text,
        uintptr_t module_base,
        size_t module_size) {
        DialogTableAddrs tables{};
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

        if (!ValidateDialogMetadataBases(
                tables.flags_base,
                tables.frame_type_base,
                tables.event_handler_base,
                tables.content_id_base,
                tables.property_id_base,
                text)) {
            tables.flags_base = 0;
            tables.frame_type_base = 0;
            tables.event_handler_base = 0;
            tables.content_id_base = 0;
            tables.property_id_base = 0;
            tables.button_ids_base = 0;
        }
        if (!ValidateDialogConfigTable(tables.config_table)) {
            tables.config_table = 0;
        }
        if (!ValidateDialogPropertyTable(tables.property_table, module_base, module_size)) {
            tables.property_table = 0;
        }
        return tables;
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

        tables = BuildStaticDialogTables(text, module_base, module_size);
        if (!tables.flags_base || !tables.config_table || !tables.property_table) {
            DialogTableAddrs resolved = BuildResolvedDialogTables(rdata, data, text, module_base, module_size);
            if (!tables.flags_base) {
                tables.flags_base = resolved.flags_base;
                tables.frame_type_base = resolved.frame_type_base;
                tables.event_handler_base = resolved.event_handler_base;
                tables.content_id_base = resolved.content_id_base;
                tables.property_id_base = resolved.property_id_base;
                tables.button_ids_base = resolved.button_ids_base;
            }
            if (!tables.config_table) {
                tables.config_table = resolved.config_table;
            }
            if (!tables.property_table) {
                tables.property_table = resolved.property_table;
            }
        }
        tables.resolved = true;

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
uint64_t Dialog::active_dialog_body_decode_nonce = 0;

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
    ClearCache();
    std::unique_lock lock(dialog_mutex);
    const bool drained = dialog_async_decode_drained.wait_for(
        lock,
        kDialogAsyncDrainTimeout,
        [] { return Dialog::pending_async_decode_count == 0; });
    lock.unlock();
    if (!drained) {
        Logger::LogStaticInfo("[Dialog] Timed out waiting for async dialog decodes to drain during shutdown.");
    }
    DialogCatalog::Terminate();
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
        std::scoped_lock lock(dialog_mutex);
        active_dialog_cache = {0, 0, L""};
        active_dialog_buttons.clear();
        last_dialog_id = 0;
        last_selected_dialog_id = 0;
        pending_body_context_dialog_id = 0;
        pending_body_context_agent_id = 0;
        decoded_text_cache.clear();
        decoded_text_pending.clear();
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
        DialogCatalog::ClearCache();
    }

    void Dialog::QueueDialogTextDecode(uint32_t dialog_id) {
        static_cast<void>(DialogCatalog::GetDialogTextDecoded(dialog_id));
    }

    // ================= Internal Memory Access Functions =================

    uint32_t Dialog::ReadDialogFlags(uint32_t dialog_id) {
        return DialogCatalog::ReadDialogFlags(dialog_id);
    }

    uint32_t Dialog::ReadDialogFrameType(uint32_t dialog_id) {
        return DialogCatalog::ReadDialogFrameType(dialog_id);
    }

    uint32_t Dialog::ReadDialogEventHandler(uint32_t dialog_id) {
        return DialogCatalog::ReadDialogEventHandler(dialog_id);
    }

    uint32_t Dialog::ReadDialogContentId(uint32_t dialog_id) {
        return DialogCatalog::ReadDialogContentId(dialog_id);
    }

    uint32_t Dialog::ReadDialogPropertyId(uint32_t dialog_id) {
        return DialogCatalog::ReadDialogPropertyId(dialog_id);
    }

    uint8_t Dialog::ReadDialogConfigType(uint32_t dialog_id) {
        return DialogCatalog::ReadDialogConfigType(dialog_id);
    }

    void* Dialog::ReadDialogProperties(uint32_t dialog_id) {
        return DialogCatalog::ReadDialogProperties(dialog_id);
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
    }

    void Dialog::OnDialogUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
        if (!wparam) {
            return;
        }
        {
            std::scoped_lock lock(dialog_mutex);
            if (dialog_shutdown_requested) {
                return;
            }
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
                uint64_t request_epoch = 0;
                {
                    std::scoped_lock lock(dialog_mutex);
                    context_dialog_id = active_dialog_cache.dialog_id;
                    context_agent_id = active_dialog_cache.agent_id;
                    request_epoch = decode_epoch;
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
                            bool queue_async_label = false;
                            req->tick = tick;
                            req->message_id = static_cast<uint32_t>(message_id);
                            req->dialog_id = info->dialog_id;
                            req->context_dialog_id = context_dialog_id;
                            req->agent_id = context_agent_id;
                            req->decode_epoch = request_epoch;
                            req->encoded = encoded_copy;
                            {
                                std::scoped_lock lock(dialog_mutex);
                                if (dialog_shutdown_requested || req->decode_epoch != decode_epoch) {
                                    delete[] req->encoded;
                                    delete req;
                                } else {
                                    decoded_button_label_pending[info->dialog_id] = true;
                                    ++pending_async_decode_count;
                                    queue_async_label = true;
                                }
                            }
                            if (queue_async_label) {
                                GW::UI::AsyncDecodeStr(req->encoded, Dialog::OnDialogButtonDecoded, req);
                                label_pending = true;
                            }
                        }
                    }
                }

                {
                    std::scoped_lock lock(dialog_mutex);
                    if (request_epoch == decode_epoch && !dialog_shutdown_requested && !label_utf8.empty()) {
                        decoded_button_label_cache[info->dialog_id] = label_utf8;
                        decoded_button_label_pending[info->dialog_id] = false;
                    }
                    if (request_epoch == decode_epoch && !dialog_shutdown_requested) {
                        DialogButtonInfo button{};
                        button.dialog_id = info->dialog_id;
                        button.button_icon = info->button_icon;
                        button.message = label_utf8;
                        button.message_decoded = label_utf8;
                        button.message_decode_pending = label_pending;
                        active_dialog_buttons.push_back(std::move(button));
                    }
                }
                if (!label_pending) {
                    bool append_journal = false;
                    {
                        std::scoped_lock lock(dialog_mutex);
                        append_journal = request_epoch == decode_epoch && !dialog_shutdown_requested;
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
                            label_utf8
                        );
                    }
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
                uint64_t request_epoch = 0;
                uint64_t decode_nonce = 0;
                {
                    std::scoped_lock lock(dialog_mutex);
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
                    active_dialog_cache.dialog_id = context_dialog_id;
                    active_dialog_cache.message.clear();
                    active_dialog_buttons.clear();
                    request_epoch = decode_epoch;
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
                            bool queue_async_body = false;
                            req->tick = tick;
                            req->message_id = static_cast<uint32_t>(message_id);
                            req->dialog_id = context_dialog_id;
                            req->agent_id = info->agent_id;
                            req->context_dialog_id = context_dialog_id;
                            req->decode_epoch = request_epoch;
                            req->decode_nonce = decode_nonce;
                            req->encoded = encoded_copy;
                            {
                                std::scoped_lock lock(dialog_mutex);
                                if (dialog_shutdown_requested || req->decode_epoch != decode_epoch) {
                                    delete[] req->encoded;
                                    delete req;
                                } else {
                                    ++pending_async_decode_count;
                                    queue_async_body = true;
                                }
                            }
                            if (queue_async_body) {
                                GW::UI::AsyncDecodeStr(req->encoded, Dialog::OnDialogBodyDecoded, req);
                                append_immediate = false;
                            }
                        }
                    }
                }
                if (append_immediate) {
                    bool append_journal = false;
                    {
                        std::scoped_lock lock(dialog_mutex);
                        append_journal = request_epoch == decode_epoch && !dialog_shutdown_requested;
                    }
                    if (append_journal) {
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
                    else if (!DialogCatalog::TryGetCachedDialogTextDecoded(selected_id, sent_text)) {
                        sent_text.clear();
                    }
                    last_selected_dialog_id = selected_id;
                    last_dialog_id = selected_id;
                    pending_body_context_dialog_id = selected_id;
                    pending_body_context_agent_id = context_agent_id;
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
        if (pending_async_decode_count > 0) {
            --pending_async_decode_count;
        }
        if (!dialog_shutdown_requested && req->decode_epoch == decode_epoch) {
            decoded_text_cache[req->dialog_id] = WideToUtf8Safe(s);
            decoded_text_pending[req->dialog_id] = false;
        }
    }
    dialog_async_decode_drained.notify_all();
    delete[] req->encoded;
    delete req;
}

void __cdecl Dialog::OnDialogBodyDecoded(void* param, const wchar_t* s) {
    auto* req = static_cast<DialogBodyDecodeRequest*>(param);
    if (!req) {
        return;
    }
    const std::string decoded_text = WideToUtf8Safe(s);
    bool append_journal = false;
    {
        std::scoped_lock lock(dialog_mutex);
        if (pending_async_decode_count > 0) {
            --pending_async_decode_count;
        }
        if (!dialog_shutdown_requested && req->decode_epoch == decode_epoch) {
            append_journal = true;
            if (active_dialog_cache.agent_id == req->agent_id &&
                active_dialog_body_decode_nonce == req->decode_nonce) {
                active_dialog_cache.message = s ? s : L"";
            }
        }
    }
    dialog_async_decode_drained.notify_all();
    if (append_journal) {
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
    }
    delete[] req->encoded;
    delete req;
}

void __cdecl Dialog::OnDialogButtonDecoded(void* param, const wchar_t* s) {
    auto* req = static_cast<DialogButtonDecodeRequest*>(param);
    if (!req) {
        return;
    }
    const std::string decoded_label = WideToUtf8Safe(s);
    bool append_journal = false;
    {
        std::scoped_lock lock(dialog_mutex);
        if (pending_async_decode_count > 0) {
            --pending_async_decode_count;
        }
        if (!dialog_shutdown_requested && req->decode_epoch == decode_epoch) {
            decoded_button_label_cache[req->dialog_id] = decoded_label;
            decoded_button_label_pending[req->dialog_id] = false;
            append_journal = true;
        }
    }
    dialog_async_decode_drained.notify_all();
    if (append_journal) {
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
    }
    delete[] req->encoded;
    delete req;
}

ActiveDialogInfo Dialog::ReadActiveDialog() {
    std::scoped_lock lock(dialog_mutex);
    return active_dialog_cache;
}
