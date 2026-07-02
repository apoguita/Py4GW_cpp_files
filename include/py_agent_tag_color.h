#pragma once

#include "Headers.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

// AgentTagColor
// ----------------------------------------------------------------------------
// Recolors agent overhead name tags (and the shared consider/target ring) by
// detouring the native color RESOLVER the game itself re-reads on every agent
// update. RE closeout: docs/RE/name_tag_color_reverse_engineering.md.
//
// Hook target: the RESOLVER GetConsiderColor (EXE FUN_007f02e0, __thiscall) —
// what the game's name-tag path (CCharAgent::GetTextData) and the consider ring
// call directly with the CCharAgent view pointer. The color is a 4-byte ARGB
// written THROUGH the out pointer. The detour runs the original first, recovers
// the agent id from view+0x2C, then overwrites *out for agents matching a rule.
// Color is ARGB 0xAARRGGBB (opaque red = 0xFFFF0000).
//
// NOTE: the id-addressable wrapper AvCharGetConsiderColor (FUN_007d9cf0) is NOT
// on the game's render path — hooking it recolors nothing. We only use it as a
// build-portable ANCHOR: FindAssertion("AvApi.cpp","agent",0x1e9) -> ToFunctionStart
// gives the wrapper, whose body CALLs ManagerFindChar (+0x07) and the resolver
// (+0x31); we derive both from there and hook the resolver.
class AgentTagColor {
public:
    struct Diagnostics {
        bool initialized = false;
        bool hook_installed = false;
        bool enabled = false;
        uint32_t resolver_calls_seen = 0;   // resolver calls observed while enabled
        uint32_t agent_rule_hits = 0;       // per-agent overrides applied
        uint32_t allegiance_rule_hits = 0;  // per-allegiance overrides applied
        uint32_t last_agent_id = 0;
        uint32_t last_color = 0;            // last color returned by the detour (ARGB)
    };

    static AgentTagColor& Instance();

    // Resolve + install the resolver detour. Safe to call once at DLL init.
    void Initialize();
    void Terminate();

    void Enable();
    void Disable();
    bool IsEnabled() const;
    bool IsHookInstalled() const;

    // Rule store. Precedence: per-agent > per-allegiance > game default.
    // Colors are ARGB 0xAARRGGBB.
    void SetAgentColor(uint32_t agent_id, uint32_t argb);
    bool RemoveAgentColor(uint32_t agent_id);
    void SetAllegianceColor(int allegiance, uint32_t argb);   // allegiance 1..6
    bool RemoveAllegianceColor(int allegiance);
    void ClearRules();

    std::map<uint32_t, uint32_t> GetAgentRules() const;
    std::map<int, uint32_t> GetAllegianceRules() const;

    // Read-only: the color the game currently computes for `agent_id` (ARGB),
    // by calling the ORIGINAL (un-overridden) resolver. Returns 0 if the
    // resolver was not resolved or the agent is invalid.
    uint32_t ReadConsiderColor(uint32_t agent_id);

    Diagnostics GetDiagnostics() const;
    void ResetDiagnostics();

    // Detour entry point (called from the installed hook). Returns the color to
    // write to *out for this agent (default unchanged when disabled/no rule).
    uint32_t ApplyOverride(uint32_t agent_id, uint32_t resolved_color);

    struct RuleSnapshot {
        std::map<uint32_t, uint32_t> agent_rules;
        std::map<int, uint32_t> allegiance_rules;
    };

private:
    AgentTagColor() = default;
    AgentTagColor(const AgentTagColor&) = delete;
    AgentTagColor& operator=(const AgentTagColor&) = delete;

    void RebuildSnapshotLocked();

    mutable std::mutex rules_mutex_;
    std::map<uint32_t, uint32_t> agent_rules_;
    std::map<int, uint32_t> allegiance_rules_;
    std::shared_ptr<const RuleSnapshot> snapshot_;

    std::atomic<bool> initialized_{false};
    std::atomic<bool> enabled_{false};
    std::atomic<bool> hook_installed_{false};

    std::atomic<uint32_t> diag_resolver_calls_{0};
    std::atomic<uint32_t> diag_agent_hits_{0};
    std::atomic<uint32_t> diag_allegiance_hits_{0};
    std::atomic<uint32_t> diag_last_agent_{0};
    std::atomic<uint32_t> diag_last_color_{0};
};
