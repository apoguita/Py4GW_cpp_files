#pragma once

#include "py_dialog.h"

#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class DialogCatalog {
public:
    static void Initialize();
    static void Terminate();
    static void ClearCache();

    static bool IsDialogAvailable(uint32_t dialog_id);
    static DialogInfo GetDialogInfo(uint32_t dialog_id);
    static std::vector<DialogInfo> EnumerateAvailableDialogs();

    static std::string GetDialogTextDecoded(uint32_t dialog_id);
    static bool IsDialogTextDecodePending(uint32_t dialog_id);
    static std::vector<DialogTextDecodedInfo> GetDecodedDialogTextStatus();
    static bool TryGetCachedDialogTextDecoded(uint32_t dialog_id, std::string& out);

    static uint32_t ReadDialogFlags(uint32_t dialog_id);
    static uint32_t ReadDialogFrameType(uint32_t dialog_id);
    static uint32_t ReadDialogEventHandler(uint32_t dialog_id);
    static uint32_t ReadDialogContentId(uint32_t dialog_id);
    static uint32_t ReadDialogPropertyId(uint32_t dialog_id);
    static uint8_t ReadDialogConfigType(uint32_t dialog_id);
    static void* ReadDialogProperties(uint32_t dialog_id);

private:
    static void QueueDialogTextDecode(uint32_t dialog_id);
    static void __cdecl OnDialogTextDecoded(void* param, const wchar_t* s);

    static std::unordered_map<uint32_t, std::string> decoded_text_cache;
    static std::unordered_map<uint32_t, bool> decoded_text_pending;
    static std::mutex catalog_mutex;
    static std::condition_variable catalog_async_decode_drained;
    static uint32_t pending_async_decode_count;
    static uint64_t decode_epoch;
    static bool shutdown_requested;
};
