#include "py_dialog_catalog.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include <GWCA/Logger/Logger.h>

namespace py = pybind11;

namespace {
    constexpr uintptr_t kGwImageBase = 0x00400000;
    constexpr auto kDialogAsyncDrainTimeout = std::chrono::milliseconds(500);

    struct DialogDecodeRequest {
        uint32_t dialog_id = 0;
        uint64_t decode_epoch = 0;
        wchar_t* encoded = nullptr;
    };

    struct SectionRange {
        uintptr_t start = 0;
        uintptr_t end = 0;

        bool valid() const {
            return start && end && end > start;
        }
    };

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

    uintptr_t ToRuntimeAddress(uintptr_t va) {
        static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if (!base) {
            return va;
        }
        return base + (va - kGwImageBase);
    }

    void LogMemoryReadFailure(const char* label, uintptr_t address) {
        char buffer[256];
        std::snprintf(
            buffer,
            sizeof(buffer),
            "[DialogCatalog] Failed to read %s at 0x%08X",
            label,
            static_cast<uint32_t>(address));
        Logger::LogStaticInfo(buffer);
    }

    std::string WideToUtf8Safe(const wchar_t* wstr) {
        if (!wstr) {
            return {};
        }
        const int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) {
            return {};
        }
        std::string out(static_cast<size_t>(len - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), len, nullptr, nullptr);
        return out;
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
            const size_t len = wcslen(src);
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
                const uintptr_t start = base + sec.VirtualAddress;
                const uintptr_t end = start + std::max(sec.Misc.VirtualSize, sec.SizeOfRawData);
                out.start = start;
                out.end = end;
                return out;
            }
        }
        return out;
    }

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
                    ++non_zero;
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
        const uintptr_t start = rdata.start + 8;
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
                    ++enabled;
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
                ++non_zero;
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
                ++non_zero;
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
                ++enabled;
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
            ++non_zero;
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

        const SectionRange rdata = GetSectionRange(".rdata");
        const SectionRange data = GetSectionRange(".data");
        const SectionRange text = GetSectionRange(".text");

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
            Logger::LogStaticInfo("[DialogCatalog] Dialog table resolution incomplete. Some dialog metadata may be unavailable.");
        }

        return tables;
    }

    py::dict ToPythonDialogInfo(const DialogInfo& info) {
        py::dict out;
        out["dialog_id"] = info.dialog_id;
        out["flags"] = info.flags;
        out["frame_type"] = info.frame_type;
        out["event_handler"] = info.event_handler;
        out["content_id"] = info.content_id;
        out["property_id"] = info.property_id;
        out["content"] = info.content;
        out["agent_id"] = info.agent_id;
        return out;
    }

    py::dict ToPythonDialogTextDecodedInfo(const DialogTextDecodedInfo& info) {
        py::dict out;
        out["dialog_id"] = info.dialog_id;
        out["text"] = info.text;
        out["pending"] = info.pending;
        return out;
    }
}

std::unordered_map<uint32_t, std::string> DialogCatalog::decoded_text_cache;
std::unordered_map<uint32_t, bool> DialogCatalog::decoded_text_pending;
std::mutex DialogCatalog::catalog_mutex;
std::condition_variable DialogCatalog::catalog_async_decode_drained;
uint32_t DialogCatalog::pending_async_decode_count = 0;
uint64_t DialogCatalog::decode_epoch = 0;
bool DialogCatalog::shutdown_requested = false;

void DialogCatalog::Initialize() {
    std::scoped_lock lock(catalog_mutex);
    shutdown_requested = false;
}

void DialogCatalog::Terminate() {
    {
        std::scoped_lock lock(catalog_mutex);
        shutdown_requested = true;
        ++decode_epoch;
    }

    std::unique_lock lock(catalog_mutex);
    const bool drained = catalog_async_decode_drained.wait_for(
        lock,
        kDialogAsyncDrainTimeout,
        [] { return DialogCatalog::pending_async_decode_count == 0; });
    decoded_text_cache.clear();
    decoded_text_pending.clear();
    lock.unlock();

    if (!drained) {
        Logger::LogStaticInfo("[DialogCatalog] Timed out waiting for async dialog decodes to drain during shutdown.");
    }
}

void DialogCatalog::ClearCache() {
    std::scoped_lock lock(catalog_mutex);
    decoded_text_cache.clear();
    decoded_text_pending.clear();
    ++decode_epoch;
}

bool DialogCatalog::IsDialogAvailable(uint32_t dialog_id) {
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return false;
    }
    return (ReadDialogFlags(dialog_id) & 0x1) != 0;
}

DialogInfo DialogCatalog::GetDialogInfo(uint32_t dialog_id) {
    DialogInfo info = {dialog_id, 0, 0, 0, 0, 0, L"", 0};
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return info;
    }

    info.flags = ReadDialogFlags(dialog_id);
    info.frame_type = ReadDialogFrameType(dialog_id);
    info.event_handler = ReadDialogEventHandler(dialog_id);
    info.content_id = ReadDialogContentId(dialog_id);
    info.property_id = ReadDialogPropertyId(dialog_id);
    return info;
}

std::vector<DialogInfo> DialogCatalog::EnumerateAvailableDialogs() {
    std::vector<DialogInfo> dialogs;
    dialogs.reserve(DialogMemory::MAX_DIALOG_ID + 1);
    for (uint32_t dialog_id = 0; dialog_id <= DialogMemory::MAX_DIALOG_ID; ++dialog_id) {
        if (IsDialogAvailable(dialog_id)) {
            dialogs.push_back(GetDialogInfo(dialog_id));
        }
    }
    return dialogs;
}

std::string DialogCatalog::GetDialogTextDecoded(uint32_t dialog_id) {
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return {};
    }
    {
        std::scoped_lock lock(catalog_mutex);
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

bool DialogCatalog::IsDialogTextDecodePending(uint32_t dialog_id) {
    std::scoped_lock lock(catalog_mutex);
    auto it = decoded_text_pending.find(dialog_id);
    return it != decoded_text_pending.end() && it->second;
}

std::vector<DialogTextDecodedInfo> DialogCatalog::GetDecodedDialogTextStatus() {
    std::scoped_lock lock(catalog_mutex);
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
        if (!kv.second || decoded_text_cache.find(kv.first) != decoded_text_cache.end()) {
            continue;
        }
        DialogTextDecodedInfo info{};
        info.dialog_id = kv.first;
        info.pending = true;
        out.push_back(std::move(info));
    }
    return out;
}

bool DialogCatalog::TryGetCachedDialogTextDecoded(uint32_t dialog_id, std::string& out) {
    std::scoped_lock lock(catalog_mutex);
    auto it = decoded_text_cache.find(dialog_id);
    if (it == decoded_text_cache.end()) {
        return false;
    }
    out = it->second;
    return true;
}

void DialogCatalog::QueueDialogTextDecode(uint32_t dialog_id) {
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return;
    }

    uint64_t request_epoch = 0;
    {
        std::scoped_lock lock(catalog_mutex);
        if (shutdown_requested) {
            return;
        }
        if (decoded_text_cache.find(dialog_id) != decoded_text_cache.end()) {
            return;
        }
        auto pending = decoded_text_pending.find(dialog_id);
        if (pending != decoded_text_pending.end() && pending->second) {
            return;
        }
        decoded_text_pending[dialog_id] = true;
        request_epoch = decode_epoch;
    }

    DialogMemory::DialogLoader_GetText_fn dialog_loader_get_text = ResolveDialogLoaderGetText();
    if (!dialog_loader_get_text) {
        std::scoped_lock lock(catalog_mutex);
        if (request_epoch == decode_epoch && !shutdown_requested) {
            decoded_text_cache[dialog_id] = {};
            decoded_text_pending[dialog_id] = false;
        }
        return;
    }

    auto* encoded_ptr = static_cast<const wchar_t*>(SafeCallDialogLoader_GetText(dialog_loader_get_text, dialog_id));
    if (!encoded_ptr) {
        std::scoped_lock lock(catalog_mutex);
        if (request_epoch == decode_epoch && !shutdown_requested) {
            decoded_text_cache[dialog_id] = {};
            decoded_text_pending[dialog_id] = false;
        }
        return;
    }

    wchar_t* encoded_copy = DupWideStringSafe(encoded_ptr);
    if (!encoded_copy) {
        std::scoped_lock lock(catalog_mutex);
        if (request_epoch == decode_epoch && !shutdown_requested) {
            decoded_text_cache[dialog_id] = {};
            decoded_text_pending[dialog_id] = false;
        }
        return;
    }

    if (!SafeIsValidEncStr(encoded_copy)) {
        std::scoped_lock lock(catalog_mutex);
        if (request_epoch == decode_epoch && !shutdown_requested) {
            decoded_text_cache[dialog_id] = WideToUtf8Safe(encoded_copy);
            decoded_text_pending[dialog_id] = false;
        }
        delete[] encoded_copy;
        return;
    }

    auto* req = new DialogDecodeRequest();
    req->dialog_id = dialog_id;
    req->decode_epoch = request_epoch;
    req->encoded = encoded_copy;

    {
        std::scoped_lock lock(catalog_mutex);
        if (shutdown_requested || req->decode_epoch != decode_epoch) {
            delete[] req->encoded;
            delete req;
            return;
        }
        ++pending_async_decode_count;
    }
    GW::UI::AsyncDecodeStr(req->encoded, DialogCatalog::OnDialogTextDecoded, req);
}

uint32_t DialogCatalog::ReadDialogFlags(uint32_t dialog_id) {
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return 0;
    }
    const auto& tables = GetDialogTables();
    if (!tables.flags_base) {
        return 0;
    }
    const uintptr_t address = tables.flags_base + (dialog_id * DialogMemory::FLAGS_STRIDE);
    uint32_t flags = 0;
    if (!TryReadU32(address, flags, "FLAGS")) {
        return 0;
    }
    return flags;
}

uint32_t DialogCatalog::ReadDialogFrameType(uint32_t dialog_id) {
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return 0;
    }
    const auto& tables = GetDialogTables();
    if (!tables.frame_type_base) {
        return 0;
    }
    const uintptr_t address = tables.frame_type_base + (dialog_id * DialogMemory::FLAGS_STRIDE);
    uint32_t frame_type = 0;
    if (!TryReadU32(address, frame_type, "FRAME_TYPE")) {
        return 0;
    }
    return frame_type;
}

uint32_t DialogCatalog::ReadDialogEventHandler(uint32_t dialog_id) {
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return 0;
    }
    const auto& tables = GetDialogTables();
    if (!tables.event_handler_base) {
        return 0;
    }
    const uintptr_t address = tables.event_handler_base + (dialog_id * DialogMemory::FLAGS_STRIDE);
    uint32_t handler = 0;
    if (!TryReadU32(address, handler, "EVENT_HANDLER")) {
        return 0;
    }
    return handler;
}

uint32_t DialogCatalog::ReadDialogContentId(uint32_t dialog_id) {
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return 0;
    }
    const auto& tables = GetDialogTables();
    if (!tables.content_id_base) {
        return 0;
    }
    const uintptr_t address = tables.content_id_base + (dialog_id * DialogMemory::CONTENT_STRIDE);
    uint32_t content_id = 0;
    if (!TryReadU32(address, content_id, "CONTENT_ID")) {
        return 0;
    }
    return content_id;
}

uint32_t DialogCatalog::ReadDialogPropertyId(uint32_t dialog_id) {
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return 0;
    }
    const auto& tables = GetDialogTables();
    if (!tables.property_id_base) {
        return 0;
    }
    const uintptr_t address = tables.property_id_base + (dialog_id * DialogMemory::PROPERTY_STRIDE);
    uint32_t property_id = 0;
    if (!TryReadU32(address, property_id, "PROPERTY_ID")) {
        return 0;
    }
    return property_id;
}

uint8_t DialogCatalog::ReadDialogConfigType(uint32_t dialog_id) {
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return 0;
    }
    const auto& tables = GetDialogTables();
    if (!tables.config_table) {
        return 0;
    }
    const uintptr_t address = tables.config_table + dialog_id;
    uint8_t config_type = 0;
    if (!TryReadU8(address, config_type, "CONFIG_TYPE")) {
        return 0;
    }
    return config_type;
}

void* DialogCatalog::ReadDialogProperties(uint32_t dialog_id) {
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return nullptr;
    }
    const auto& tables = GetDialogTables();
    if (!tables.property_table) {
        return nullptr;
    }
    const uintptr_t address = tables.property_table + (dialog_id * 8);
    void* properties = nullptr;
    if (!TryReadPtr(address, properties, "PROPERTIES")) {
        return nullptr;
    }
    return properties;
}

void __cdecl DialogCatalog::OnDialogTextDecoded(void* param, const wchar_t* s) {
    auto* req = static_cast<DialogDecodeRequest*>(param);
    if (!req) {
        return;
    }
    {
        std::scoped_lock lock(catalog_mutex);
        if (pending_async_decode_count > 0) {
            --pending_async_decode_count;
        }
        if (!shutdown_requested && req->decode_epoch == decode_epoch) {
            decoded_text_cache[req->dialog_id] = WideToUtf8Safe(s);
            decoded_text_pending[req->dialog_id] = false;
        }
    }
    catalog_async_decode_drained.notify_all();
    delete[] req->encoded;
    delete req;
}

PYBIND11_EMBEDDED_MODULE(PyDialogCatalog, m) {
    py::class_<DialogCatalog>(m, "PyDialogCatalog")
        .def(py::init<>())
        .def_static("is_dialog_available", &DialogCatalog::IsDialogAvailable, py::arg("dialog_id"))
        .def_static("get_dialog_info", [](uint32_t dialog_id) {
            return ToPythonDialogInfo(DialogCatalog::GetDialogInfo(dialog_id));
        }, py::arg("dialog_id"))
        .def_static("enumerate_available_dialogs", []() {
            py::list out;
            for (const auto& info : DialogCatalog::EnumerateAvailableDialogs()) {
                out.append(ToPythonDialogInfo(info));
            }
            return out;
        })
        .def_static("get_dialog_text_decoded", &DialogCatalog::GetDialogTextDecoded, py::arg("dialog_id"))
        .def_static("is_dialog_text_decode_pending", &DialogCatalog::IsDialogTextDecodePending, py::arg("dialog_id"))
        .def_static("get_dialog_text_decode_status", []() {
            py::list out;
            for (const auto& info : DialogCatalog::GetDecodedDialogTextStatus()) {
                out.append(ToPythonDialogTextDecodedInfo(info));
            }
            return out;
        })
        .def_static("clear_cache", &DialogCatalog::ClearCache);
}
