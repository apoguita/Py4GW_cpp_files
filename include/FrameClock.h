#pragma once
#include <cstdint>

namespace frame_clock {
    // Returns the per-frame monotonically-increasing timestamp set at the
    // top of Py4GW::Draw() before SHMem publish. Use this instead of
    // GetTickCount64() in any code that needs to bucket events by frame.
    uint64_t GetFrameTimestamp();
}
