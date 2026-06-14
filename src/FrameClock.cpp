#include "FrameClock.h"
#include <windows.h>

namespace frame_clock {
    extern uint64_t g_frame_id_timestamp;  // defined in Py4GW.cpp

    uint64_t GetFrameTimestamp() {
        return g_frame_id_timestamp ? g_frame_id_timestamp : GetTickCount64();
    }
}
