#pragma once

#include "Headers.h"

namespace WindowTitleHook {
    bool SetNextCreatedWindowTitle(const std::wstring& title);
    void ClearNextCreatedWindowTitle();
    bool HasNextCreatedWindowTitle();
    void ArmNextCreatedWindowTitle(uint32_t parent_frame_id, uint32_t child_index);
    void CancelArmedWindowTitle(uint32_t parent_frame_id, uint32_t child_index);
    uint32_t GetLastAppliedFrameId();
    std::wstring GetLastAppliedTitle();
    bool IsInstalled();
}
