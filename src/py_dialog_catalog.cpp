#include "py_dialog_catalog.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>

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
        bool resolved = false;
    };

    DialogTableAddrs g_dialog_tables;

    uintptr_t ToRuntimeAddress(uintptr_t va) {
        static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if (!base) {
            return va;
        }
        return base + (va - kGwImageBase);
    }

    bool IsDialogCatalogMapReadySafe() {
        __try {
            const auto instance_type = GW::Map::GetInstanceType();
            return GW::Map::GetIsMapLoaded() &&
                !GW::Map::GetIsObserving() &&
                instance_type != GW::Constants::InstanceType::Loading;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
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

    std::wstring Utf8ToWideSafe(const std::string& text) {
        if (text.empty()) {
            return {};
        }
        const int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (len <= 0) {
            return {};
        }
        try {
            std::wstring out(static_cast<size_t>(len), L'\0');
            const int written = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), len);
            if (written <= 0) {
                return {};
            }
            out.resize(static_cast<size_t>(written - 1));
            return out;
        } catch (...) {
            return {};
        }
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

    bool TryReadU32NoLog(uintptr_t address, uint32_t& out) {
        __try {
            out = *reinterpret_cast<uint32_t*>(address);
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

    void* SafeCallDialogLoader_GetText(DialogMemory::DialogLoader_GetText_fn fn, uint32_t dialog_id) {
        void* result = nullptr;
        __try {
            result = fn(dialog_id);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            result = nullptr;
        }
        return result;
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

    void ReleaseDialogDecodeRequest(DialogDecodeRequest* req) {
        if (!req) {
            return;
        }
        delete[] req->encoded;
        delete req;
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

    bool GetModuleInfo(uintptr_t& base) {
        HMODULE module = GetModuleHandleW(nullptr);
        if (!module) {
            base = 0;
            return false;
        }
        base = reinterpret_cast<uintptr_t>(module);
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
            return false;
        }
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) {
            return false;
        }
        return true;
    }

    SectionRange GetSectionRange(const char* name) {
        SectionRange out{};
        uintptr_t base = 0;
        if (!GetModuleInfo(base)) {
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

    DialogTableAddrs BuildStaticDialogTables(const SectionRange& text) {
        DialogTableAddrs tables{};
        tables.flags_base = ToRuntimeAddress(DialogMemory::FLAGS_BASE);
        tables.frame_type_base = ToRuntimeAddress(DialogMemory::FRAME_TYPE_BASE);
        tables.event_handler_base = ToRuntimeAddress(DialogMemory::EVENT_HANDLER_BASE);
        tables.content_id_base = ToRuntimeAddress(DialogMemory::CONTENT_ID_BASE);
        tables.property_id_base = ToRuntimeAddress(DialogMemory::PROPERTY_ID_BASE);

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
        }
        return tables;
    }

    DialogTableAddrs BuildResolvedDialogTables(
        const SectionRange& rdata,
        const SectionRange& text) {
        DialogTableAddrs tables{};
        tables.flags_base = ResolveFlagsBase(rdata, text);
        if (tables.flags_base) {
            tables.event_handler_base = tables.flags_base - 0x8;
            tables.frame_type_base = tables.flags_base - 0x4;
            tables.content_id_base = tables.flags_base + 0x4;
            tables.property_id_base = tables.flags_base + 0x8;
        }

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
        }
        return tables;
    }

    DialogTableAddrs& GetDialogTables() {
        if (g_dialog_tables.resolved) {
            return g_dialog_tables;
        }
        g_dialog_tables.resolved = true;

        const SectionRange rdata = GetSectionRange(".rdata");
        const SectionRange text = GetSectionRange(".text");

        g_dialog_tables = BuildStaticDialogTables(text);
        if (!g_dialog_tables.flags_base) {
            DialogTableAddrs resolved = BuildResolvedDialogTables(rdata, text);
            if (!g_dialog_tables.flags_base) {
                g_dialog_tables.flags_base = resolved.flags_base;
                g_dialog_tables.frame_type_base = resolved.frame_type_base;
                g_dialog_tables.event_handler_base = resolved.event_handler_base;
                g_dialog_tables.content_id_base = resolved.content_id_base;
                g_dialog_tables.property_id_base = resolved.property_id_base;
            }
        }
        g_dialog_tables.resolved = true;

        if (!g_dialog_tables.flags_base) {
            Logger::LogStaticInfo("[DialogCatalog] Dialog table resolution incomplete. Some dialog metadata may be unavailable.");
        }

        return g_dialog_tables;
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
    bool logged_wait = false;
    while (pending_async_decode_count != 0) {
        const bool drained = catalog_async_decode_drained.wait_for(
            lock,
            kDialogAsyncDrainTimeout,
            [] { return DialogCatalog::pending_async_decode_count == 0; });
        if (drained) {
            break;
        }
        if (!logged_wait) {
            Logger::LogStaticInfo("[DialogCatalog] Async dialog decodes did not drain within the initial shutdown timeout; continuing to wait fail-closed.");
            logged_wait = true;
        }
    }
    decoded_text_cache.clear();
    decoded_text_pending.clear();
    lock.unlock();
}

void DialogCatalog::ClearCache() {
    std::scoped_lock lock(catalog_mutex);
    decoded_text_cache.clear();
    decoded_text_pending.clear();
    ++decode_epoch;
    g_dialog_tables = {};
}

bool DialogCatalog::IsDialogAvailable(uint32_t dialog_id) {
    if (!IsDialogCatalogMapReadySafe()) {
        return false;
    }
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return false;
    }
    return (ReadDialogFlags(dialog_id) & 0x1) != 0;
}

DialogInfo DialogCatalog::GetDialogInfo(uint32_t dialog_id) {
    DialogInfo info = {dialog_id, 0, 0, 0, 0, 0, L"", 0};
    if (!IsDialogCatalogMapReadySafe()) {
        return info;
    }
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return info;
    }

    info.flags = ReadDialogFlags(dialog_id);
    info.frame_type = ReadDialogFrameType(dialog_id);
    info.event_handler = ReadDialogEventHandler(dialog_id);
    info.content_id = ReadDialogContentId(dialog_id);
    info.property_id = ReadDialogPropertyId(dialog_id);
    info.content = Utf8ToWideSafe(GetDialogTextDecoded(dialog_id));
    return info;
}

std::vector<DialogInfo> DialogCatalog::EnumerateAvailableDialogs() {
    std::vector<DialogInfo> dialogs;
    if (!IsDialogCatalogMapReadySafe()) {
        return dialogs;
    }
    dialogs.reserve(DialogMemory::MAX_DIALOG_ID + 1);
    for (uint32_t dialog_id = 0; dialog_id <= DialogMemory::MAX_DIALOG_ID; ++dialog_id) {
        if (IsDialogAvailable(dialog_id)) {
            dialogs.push_back(GetDialogInfo(dialog_id));
        }
    }
    return dialogs;
}

std::string DialogCatalog::GetDialogTextDecoded(uint32_t dialog_id) {
    if (!IsDialogCatalogMapReadySafe()) {
        return {};
    }
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
    if (!IsDialogCatalogMapReadySafe()) {
        return false;
    }
    std::scoped_lock lock(catalog_mutex);
    auto it = decoded_text_pending.find(dialog_id);
    return it != decoded_text_pending.end() && it->second;
}

std::vector<DialogTextDecodedInfo> DialogCatalog::GetDecodedDialogTextStatus() {
    if (!IsDialogCatalogMapReadySafe()) {
        return {};
    }
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
    if (!IsDialogCatalogMapReadySafe()) {
        return false;
    }
    std::scoped_lock lock(catalog_mutex);
    auto it = decoded_text_cache.find(dialog_id);
    if (it == decoded_text_cache.end()) {
        return false;
    }
    out = it->second;
    return true;
}

void DialogCatalog::QueueDialogTextDecode(uint32_t dialog_id) {
    if (!IsDialogCatalogMapReadySafe()) {
        return;
    }
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
        try {
            decoded_text_pending[dialog_id] = true;
        } catch (...) {
            decoded_text_pending.erase(dialog_id);
            return;
        }
        request_epoch = decode_epoch;
    }

    DialogMemory::DialogLoader_GetText_fn dialog_loader_get_text = ResolveDialogLoaderGetText();
    if (!dialog_loader_get_text) {
        std::scoped_lock lock(catalog_mutex);
        if (request_epoch == decode_epoch && !shutdown_requested) {
            try {
                decoded_text_cache[dialog_id] = {};
            } catch (...) {
            }
            decoded_text_pending.erase(dialog_id);
        }
        return;
    }

    auto* encoded_ptr = static_cast<const wchar_t*>(SafeCallDialogLoader_GetText(dialog_loader_get_text, dialog_id));
    if (!encoded_ptr) {
        std::scoped_lock lock(catalog_mutex);
        if (request_epoch == decode_epoch && !shutdown_requested) {
            try {
                decoded_text_cache[dialog_id] = {};
            } catch (...) {
            }
            decoded_text_pending.erase(dialog_id);
        }
        return;
    }

    wchar_t* encoded_copy = DupWideStringSafe(encoded_ptr);
    if (!encoded_copy) {
        std::scoped_lock lock(catalog_mutex);
        if (request_epoch == decode_epoch && !shutdown_requested) {
            try {
                decoded_text_cache[dialog_id] = {};
            } catch (...) {
            }
            decoded_text_pending.erase(dialog_id);
        }
        return;
    }

    if (!SafeIsValidEncStr(encoded_copy)) {
        std::scoped_lock lock(catalog_mutex);
        if (request_epoch == decode_epoch && !shutdown_requested) {
            try {
                decoded_text_cache[dialog_id] = WideToUtf8Safe(encoded_copy);
            } catch (...) {
            }
            decoded_text_pending.erase(dialog_id);
        }
        delete[] encoded_copy;
        return;
    }

    auto* req = new (std::nothrow) DialogDecodeRequest();
    if (!req) {
        {
            std::scoped_lock lock(catalog_mutex);
            if (request_epoch == decode_epoch && !shutdown_requested) {
                decoded_text_pending.erase(dialog_id);
            }
        }
        delete[] encoded_copy;
        return;
    }
    req->dialog_id = dialog_id;
    req->decode_epoch = request_epoch;
    req->encoded = encoded_copy;

    {
        std::scoped_lock lock(catalog_mutex);
        if (shutdown_requested || req->decode_epoch != decode_epoch) {
            ReleaseDialogDecodeRequest(req);
            return;
        }
        ++pending_async_decode_count;
    }
    if (!SafeAsyncDecodeStr(req->encoded, DialogCatalog::OnDialogTextDecoded, req)) {
        {
            std::scoped_lock lock(catalog_mutex);
            if (pending_async_decode_count > 0) {
                --pending_async_decode_count;
            }
            if (request_epoch == decode_epoch && !shutdown_requested) {
                decoded_text_pending.erase(dialog_id);
            }
        }
        catalog_async_decode_drained.notify_all();
        ReleaseDialogDecodeRequest(req);
    }
}

uint32_t DialogCatalog::ReadDialogFlags(uint32_t dialog_id) {
    if (!IsDialogCatalogMapReadySafe()) {
        return 0;
    }
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
    if (!IsDialogCatalogMapReadySafe()) {
        return 0;
    }
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
    if (!IsDialogCatalogMapReadySafe()) {
        return 0;
    }
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
    if (!IsDialogCatalogMapReadySafe()) {
        return 0;
    }
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
    if (!IsDialogCatalogMapReadySafe()) {
        return 0;
    }
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

void __cdecl DialogCatalog::OnDialogTextDecoded(void* param, const wchar_t* s) {
    auto* req = static_cast<DialogDecodeRequest*>(param);
    if (!req) {
        return;
    }
    wchar_t* decoded_copy = DupWideStringSafe(s);
    const std::string decoded_text = decoded_copy ? WideToUtf8Safe(decoded_copy) : std::string{};
    {
        std::scoped_lock lock(catalog_mutex);
        if (pending_async_decode_count > 0) {
            --pending_async_decode_count;
        }
        if (!shutdown_requested && req->decode_epoch == decode_epoch) {
            try {
                decoded_text_cache[req->dialog_id] = decoded_text;
                decoded_text_pending.erase(req->dialog_id);
            } catch (...) {
                decoded_text_pending.erase(req->dialog_id);
            }
        }
    }
    catalog_async_decode_drained.notify_all();
    delete[] decoded_copy;
    ReleaseDialogDecodeRequest(req);
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
