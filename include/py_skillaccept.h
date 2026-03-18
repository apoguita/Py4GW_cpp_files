#pragma once

#include "Headers.h"

#include <array>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

struct PendingSkillInfo {
    uint32_t skill_id = 0;
    uint32_t copy_id = 0;
    uint32_t ref_count = 0;
};

struct PendingSkillDebugEvent {
    uint32_t owner_id = 0;
    uint32_t skill_id = 0;
    uint32_t copy_id = 0;
    bool added = false;
};

struct PendingSkillFrameEvent {
    uint32_t source = 0;
    uint32_t message_id = 0;
    uint32_t wparam_ptr = 0;
    uint32_t lparam_ptr = 0;
    uint32_t w0 = 0;
    uint32_t w1 = 0;
    uint32_t w2 = 0;
    uint32_t w3 = 0;
    uint32_t w4 = 0;
};

class SkillAccept {
public:
    SkillAccept() = default;

    static void Initialize();
    static void Terminate();
    static void ClearCache();

    static bool AcceptOfferedSkill(uint32_t skill_id);
    static bool AcceptOfferedSkillReplace(
        uint32_t skill_id,
        uint32_t slot_index,
        std::optional<uint32_t> copy_id = std::nullopt);

    static std::vector<PendingSkillInfo> GetPendingSkills(uint32_t agent_id = 0);
    static PendingSkillDebugEvent GetPendingSkillDebugEvent();
    static std::vector<PendingSkillFrameEvent> GetPendingSkillFrameEvents();

private:
    static void RegisterHooks();
    static void UnregisterHooks();

    static std::vector<PendingSkillInfo> ReadPendingSkills(uint32_t agent_id);
    static bool TryGetPendingCopyId(uint32_t agent_id, uint32_t skill_id, uint32_t& out_copy_id);

    static void OnPendingSkillFrameMessage(
        GW::HookStatus*,
        const GW::UI::Frame*,
        GW::UI::UIMessage msg,
        void* wparam,
        void* lparam);
    static void OnPendingSkillUiMessage(
        GW::HookStatus*,
        GW::UI::UIMessage msg,
        void* wparam,
        void* lparam);
    static void __stdcall OnPendingSkillAddRef(uint32_t owner_id, uint32_t skill_id, uint32_t copy_id);
    static void __stdcall OnPendingSkillRelease(uint32_t owner_id, uint32_t skill_id, uint32_t copy_id);

    static std::mutex mutex;
    static bool hooks_registered;
    static std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> pending_skill_cache;
    static PendingSkillDebugEvent pending_skill_debug_event;
    static std::vector<PendingSkillFrameEvent> pending_skill_frame_events;
    static std::array<GW::HookEntry, 10> pending_skill_frame_entries;
    static std::array<GW::HookEntry, 10> pending_skill_ui_entries;
};
