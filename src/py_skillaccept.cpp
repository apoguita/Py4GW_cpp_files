#include "py_skillaccept.h"

#include <Windows.h>

#include <algorithm>

using namespace pybind11;

namespace {
    constexpr uintptr_t kGwImageBase = 0x00400000;
    constexpr uintptr_t kSkillBarSetSlotData = 0x007F6220;
    constexpr uintptr_t kSkillBarPendingSkillAddRef = 0x007F7E60;
    constexpr uintptr_t kSkillBarPendingSkillRelease = 0x007F7FF0;

    using SkillBar_SetSlotData_fn = void(__stdcall*)(uint32_t key, uint32_t slot, uint32_t skill_id, uint32_t copy_id);
    using SkillBar_PendingSkill_fn = void(__stdcall*)(uint32_t owner_id, uint32_t skill_id, uint32_t copy_id);

    SkillBar_PendingSkill_fn pending_skill_addref_ret = nullptr;
    SkillBar_PendingSkill_fn pending_skill_release_ret = nullptr;

    uintptr_t ToRuntimeAddress(uintptr_t va) {
        static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if (!base) {
            return va;
        }
        return base + (va - kGwImageBase);
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

    bool SafeCallSkillBarSetSlotData(
        SkillBar_SetSlotData_fn fn,
        uint32_t key,
        uint32_t slot,
        uint32_t skill_id,
        uint32_t copy_id) {
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

    bool QueuePendingSkillReplace(
        uint32_t owner_id,
        uint32_t slot,
        uint32_t skill_id,
        uint32_t copy_id) {
        SkillBar_SetSlotData_fn set_slot_fn =
            reinterpret_cast<SkillBar_SetSlotData_fn>(ToRuntimeAddress(kSkillBarSetSlotData));
        if (!set_slot_fn) {
            return false;
        }

        GW::GameThread::Enqueue([owner_id, slot, skill_id, copy_id, set_slot_fn]() {
            const uint32_t resolved_owner_id = owner_id ? owner_id : GW::Agents::GetControlledCharacterId();
            if (!resolved_owner_id) {
                return;
            }
            SafeCallSkillBarSetSlotData(set_slot_fn, resolved_owner_id, slot, skill_id, copy_id);
        });
        return true;
    }

    void AppendPendingSkillFrameEvent(
        std::vector<PendingSkillFrameEvent>& events,
        uint32_t source,
        GW::UI::UIMessage msg,
        void* wparam,
        void* lparam,
        uint32_t w0,
        uint32_t w1,
        uint32_t w2,
        uint32_t w3,
        uint32_t w4) {
        PendingSkillFrameEvent evt{};
        evt.source = source;
        evt.message_id = static_cast<uint32_t>(msg);
        evt.wparam_ptr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam));
        evt.lparam_ptr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lparam));
        evt.w0 = w0;
        evt.w1 = w1;
        evt.w2 = w2;
        evt.w3 = w3;
        evt.w4 = w4;
        events.push_back(evt);
        if (events.size() > 32) {
            events.erase(events.begin(), events.begin() + (events.size() - 32));
        }
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
}

PYBIND11_EMBEDDED_MODULE(PySkillAccept, m) {
    BindPendingSkillInfo(m);
    BindPendingSkillDebugEvent(m);
    BindPendingSkillFrameEvent(m);
    py::class_<SkillAccept>(m, "PySkillAccept")
        .def(py::init<>())
        .def_static("get_pending_skills", &SkillAccept::GetPendingSkills, py::arg("agent_id") = 0)
        .def_static("get_pending_skill_debug_event", &SkillAccept::GetPendingSkillDebugEvent)
        .def_static("get_pending_skill_frame_events", &SkillAccept::GetPendingSkillFrameEvents)
        .def_static("accept_offered_skill", &SkillAccept::AcceptOfferedSkill, py::arg("skill_id"))
        .def_static(
            "accept_offered_skill_replace",
            &SkillAccept::AcceptOfferedSkillReplace,
            py::arg("skill_id"),
            py::arg("slot_index"),
            py::arg("copy_id") = std::nullopt)
        .def_static(
            "apply_pending_skill_replace",
            &SkillAccept::ApplyPendingSkillReplace,
            py::arg("skill_id"),
            py::arg("slot_index"),
            py::arg("copy_id") = std::nullopt,
            py::arg("agent_id") = 0)
        .def_static("clear_cache", &SkillAccept::ClearCache)
        .def_static("initialize", &SkillAccept::Initialize)
        .def_static("terminate", &SkillAccept::Terminate);
}

std::mutex SkillAccept::mutex;
bool SkillAccept::hooks_registered = false;
std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> SkillAccept::pending_skill_cache;
PendingSkillDebugEvent SkillAccept::pending_skill_debug_event = {0, 0, 0, 0};
std::vector<PendingSkillFrameEvent> SkillAccept::pending_skill_frame_events;
std::array<GW::HookEntry, 10> SkillAccept::pending_skill_frame_entries;
std::array<GW::HookEntry, 10> SkillAccept::pending_skill_ui_entries;

void SkillAccept::Initialize() {
    ClearCache();
    RegisterHooks();
}

void SkillAccept::Terminate() {
    UnregisterHooks();
    ClearCache();
}

void SkillAccept::ClearCache() {
    std::scoped_lock lock(mutex);
    pending_skill_cache.clear();
    pending_skill_debug_event = {0, 0, 0, 0};
    pending_skill_frame_events.clear();
}

void SkillAccept::RegisterHooks() {
    {
        std::scoped_lock lock(mutex);
        if (hooks_registered) {
            return;
        }
        hooks_registered = true;
    }

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
            SkillAccept::OnPendingSkillFrameMessage,
            0x1);
        GW::UI::RegisterUIMessageCallback(
            &pending_skill_ui_entries[i],
            static_cast<GW::UI::UIMessage>(msg_ids[i]),
            SkillAccept::OnPendingSkillUiMessage,
            0x1);
    }
}

void SkillAccept::UnregisterHooks() {
    {
        std::scoped_lock lock(mutex);
        if (!hooks_registered) {
            return;
        }
        hooks_registered = false;
    }

    for (auto& entry : pending_skill_frame_entries) {
        GW::UI::RemoveFrameUIMessageCallback(&entry);
    }
    for (auto& entry : pending_skill_ui_entries) {
        GW::UI::RemoveUIMessageCallback(&entry, GW::UI::UIMessage::kNone);
    }

    pending_skill_addref_ret = nullptr;
    pending_skill_release_ret = nullptr;
}

bool SkillAccept::AcceptOfferedSkill(uint32_t skill_id) {
    if (skill_id == 0) {
        return false;
    }

    const uint32_t raw_dialog_id = skill_id | 0x0A000000;
    GW::GameThread::Enqueue([raw_dialog_id]() {
        GW::Agents::SendDialog(raw_dialog_id);
    });
    return true;
}

bool SkillAccept::AcceptOfferedSkillReplace(
    uint32_t skill_id,
    uint32_t slot_index,
    std::optional<uint32_t> copy_id) {
    if (skill_id == 0 || slot_index > 7) {
        return false;
    }

    uint32_t resolved_copy_id = 0;
    bool have_copy_id = false;
    if (copy_id.has_value()) {
        resolved_copy_id = copy_id.value();
        have_copy_id = true;
    } else {
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

bool SkillAccept::ApplyPendingSkillReplace(
    uint32_t skill_id,
    uint32_t slot_index,
    std::optional<uint32_t> copy_id,
    uint32_t agent_id) {
    if (skill_id == 0 || slot_index > 7) {
        return false;
    }

    uint32_t resolved_copy_id = 0;
    if (copy_id.has_value()) {
        resolved_copy_id = copy_id.value();
    } else if (!TryGetPendingCopyId(agent_id, skill_id, resolved_copy_id)) {
        return false;
    }

    if (resolved_copy_id == 0) {
        return false;
    }

    return QueuePendingSkillReplace(agent_id, slot_index, skill_id, resolved_copy_id);
}

std::vector<PendingSkillInfo> SkillAccept::GetPendingSkills(uint32_t agent_id) {
    return ReadPendingSkills(agent_id);
}

PendingSkillDebugEvent SkillAccept::GetPendingSkillDebugEvent() {
    std::scoped_lock lock(mutex);
    return pending_skill_debug_event;
}

std::vector<PendingSkillFrameEvent> SkillAccept::GetPendingSkillFrameEvents() {
    std::scoped_lock lock(mutex);
    return pending_skill_frame_events;
}

void __stdcall SkillAccept::OnPendingSkillAddRef(uint32_t owner_id, uint32_t skill_id, uint32_t copy_id) {
    if (pending_skill_addref_ret) {
        pending_skill_addref_ret(owner_id, skill_id, copy_id);
    }

    const uint32_t key = (skill_id << 16) | (copy_id & 0xFFFF);
    std::scoped_lock lock(mutex);
    pending_skill_cache[owner_id][key] += 1;
    pending_skill_debug_event = {owner_id, skill_id, copy_id, true};
}

void __stdcall SkillAccept::OnPendingSkillRelease(uint32_t owner_id, uint32_t skill_id, uint32_t copy_id) {
    if (pending_skill_release_ret) {
        pending_skill_release_ret(owner_id, skill_id, copy_id);
    }

    const uint32_t key = (skill_id << 16) | (copy_id & 0xFFFF);
    std::scoped_lock lock(mutex);
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
    } else {
        entries.erase(it);
        if (entries.empty()) {
            pending_skill_cache.erase(owner_it);
        }
    }

    pending_skill_debug_event = {owner_id, skill_id, copy_id, false};
}

void SkillAccept::OnPendingSkillFrameMessage(
    GW::HookStatus*,
    const GW::UI::Frame*,
    GW::UI::UIMessage msg,
    void* wparam,
    void* lparam) {
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

    std::scoped_lock lock(mutex);
    AppendPendingSkillFrameEvent(pending_skill_frame_events, 0, msg, wparam, lparam, w0, w1, w2, w3, w4);
    if (w0 != 0 && w1 != 0) {
        const uint32_t key = (w1 << 16) | (w2 & 0xFFFF);
        pending_skill_cache[w0][key] = 1;
        pending_skill_debug_event = {w0, w1, w2, true};
    }
}

void SkillAccept::OnPendingSkillUiMessage(
    GW::HookStatus*,
    GW::UI::UIMessage msg,
    void* wparam,
    void* lparam) {
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

    std::scoped_lock lock(mutex);
    AppendPendingSkillFrameEvent(pending_skill_frame_events, 1, msg, wparam, lparam, w0, w1, w2, w3, w4);
    if (w0 != 0 && w1 != 0) {
        const uint32_t key = (w1 << 16) | (w2 & 0xFFFF);
        pending_skill_cache[w0][key] = 1;
        pending_skill_debug_event = {w0, w1, w2, true};
    }
}

std::vector<PendingSkillInfo> SkillAccept::ReadPendingSkills(uint32_t agent_id) {
    std::vector<PendingSkillInfo> out;

    std::scoped_lock lock(mutex);
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

bool SkillAccept::TryGetPendingCopyId(uint32_t agent_id, uint32_t skill_id, uint32_t& out_copy_id) {
    out_copy_id = 0;
    if (skill_id == 0) {
        return false;
    }

    const auto pending = ReadPendingSkills(agent_id);
    for (const auto& entry : pending) {
        if (entry.skill_id == skill_id) {
            out_copy_id = entry.copy_id;
            return out_copy_id != 0;
        }
    }
    return false;
}
