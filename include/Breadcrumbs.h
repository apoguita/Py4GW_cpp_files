#pragma once
// Crash breadcrumbs: per-thread last-Python-frame POD + lock-free MPSC ring.
// Read by the SEH filter, so EVERYTHING here is allocation-free, lock-free, and Python-API-free.
//
// Why a breadcrumb instead of a live Python frame walk:
//   At native-crash time the GIL/interpreter may be inconsistent (the faulting callback
//   held the GIL, Py4GW.cpp:1146). We therefore CANNOT call any Py* API from the filter.
//   Instead the Python side snapshots its current frame into per-thread static storage
//   (under the GIL, on the executing thread) during normal execution; the filter reads
//   only that static copy on the FAULTING thread.

#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace bc {

// ---- last Python frame (per-thread) ----------------------------------------
// Fixed-size POD; strings are memcpy'd at capture time (NOT PyUnicode_AsUTF8 ptrs,
// whose lifetime/heap is exactly what a crash corrupts).
struct LastPyFrame {
    char file[260];
    char func[128];
    int  line;
};

// One slot per thread. Written by whichever thread runs the Python callback (game thread AND
// update thread); read by the filter on the faulting thread. POD `{}` => constant zero-init,
// so there is NO thread_local dynamic-init guard for the crash-time reader to trip over.
inline thread_local LastPyFrame t_last_py_frame{};

// Capture: called from the pybind binding with the GIL held. Copies into TLS (no heap).
inline void set_last_py_frame(const char* file, int line, const char* func) {
    LastPyFrame& f = t_last_py_frame;
    if (file) { strncpy_s(f.file, file, _TRUNCATE); } else { f.file[0] = 0; }
    if (func) { strncpy_s(f.func, func, _TRUNCATE); } else { f.func[0] = 0; }
    f.line = line;
}

// Read: called from the SEH filter. Returns a by-value POD copy of THIS thread's slot.
inline LastPyFrame read_last_py_frame() { return t_last_py_frame; }

// ---- lock-free MPSC breadcrumb ring ----------------------------------------
// Fixed ring of fixed-size messages. MULTIPLE producers (game + update threads call
// breadcrumb()); single consumer (the SEH filter draining at crash time). No locks.
//
// Publish protocol (fixes the missing release/acquire the review flagged): reserve a
// monotonic index, write the message, then RELEASE-store a per-slot seq == index+1. The
// consumer ACQUIRE-loads next, then ACQUIRE-loads each slot's seq and only reads a slot
// whose seq matches the index it expects -> never reads a half-written or recycled slot.
// Residual: two producers landing on the same physical slot (indices kRingSize apart) at the
// same instant can tear one message; tolerated (best-effort, last-few-crumbs-only).
constexpr uint32_t kRingSize = 64;        // power of two for cheap masking
constexpr uint32_t kMsgLen   = 160;

struct Slot {
    std::atomic<uint32_t> seq;            // 0 = empty; otherwise (producer_index + 1)
    char msg[kMsgLen];
};

struct Ring {
    Slot slots[kRingSize];
    std::atomic<uint32_t> next;           // next index to hand out (monotonic)
};

inline Ring g_ring{};

// Producer: append "tag: text" (truncated). Call from normal execution context only.
inline void breadcrumb_copy(const char* tag, const char* text) {
    uint32_t idx = g_ring.next.fetch_add(1, std::memory_order_relaxed);
    Slot& s = g_ring.slots[idx & (kRingSize - 1)];
    _snprintf_s(s.msg, kMsgLen, _TRUNCATE, "%s: %s", tag ? tag : "?", text ? text : "");
    s.seq.store(idx + 1, std::memory_order_release);   // publish AFTER the message is written
}

// Convenience one-arg breadcrumb.
inline void breadcrumb(const char* text) { breadcrumb_copy("bc", text); }

// Consumer (SEH filter). Visit the most-recent up-to-N entries oldest->newest, skipping any
// slot a producer has since recycled or not finished writing.
template <typename Fn>
inline void drain_recent(uint32_t max_entries, Fn&& visit) {
    uint32_t end = g_ring.next.load(std::memory_order_acquire);   // one past the newest index
    uint32_t count = (end < max_entries) ? end : max_entries;
    for (uint32_t k = count; k > 0; --k) {
        uint32_t idx = end - k;                                   // oldest of the window first
        Slot& s = g_ring.slots[idx & (kRingSize - 1)];
        if (s.seq.load(std::memory_order_acquire) == idx + 1)     // slot still holds THIS entry
            visit(s.msg);
    }
}

} // namespace bc
