#include "py_agent_tag_color.h"

#include <MinHook.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {
    // GetConsiderColor RESOLVER (EXE FUN_007f02e0), __thiscall. This is what the
    // GAME'S name-tag path (CCharAgent::GetTextData -> GetConsiderColor(&color,
    // this, 1)) and the consider/target ring call DIRECTLY with the view pointer.
    // (The id-addressable wrapper AvCharGetConsiderColor / FUN_007d9cf0 is only a
    // C-API entry other code uses; the game does NOT route name tags through it,
    // so hooking the wrapper does nothing visually — the resolver is the target.)
    //
    // Emulated as __fastcall so a free-function detour matches the thiscall ABI:
    // `this` arrives in ECX (fastcall arg 1), EDX is unused (fastcall arg 2), and
    // the two stack args (out, flag) follow. The color is a 4-byte ARGB written
    // THROUGH `out`; `flag` 1 = text (name-tag) color. RET 8 <-> fastcall cleans 8.
    using ResolverFn = uint32_t*(__fastcall*)(void* view, void* edx, uint32_t* out, int flag);
    // ManagerFindChar (EXE FUN_007fc920), __cdecl: agent_id -> CCharAgent view or null.
    using FindCharFn = void*(__cdecl*)(uint32_t agent_id);

    ResolverFn g_resolver = nullptr;           // hooked function (-> detour)
    ResolverFn g_resolver_original = nullptr;  // trampoline -> original behavior
    FindCharFn g_find_char = nullptr;          // id -> view (reads + null guard)

    int ResolveAgentAllegiance(uint32_t agent_id) {
        GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
        if (!agent)
            return 0;
        GW::AgentLiving* living = agent->GetAsAgentLiving();
        if (!living)
            return 0;
        return static_cast<int>(living->allegiance);
    }

    uint32_t* __fastcall Detour_GetConsiderColor(void* view, void* edx, uint32_t* out, int flag) {
        // Let the game resolve the default color first (writes through `out`).
        uint32_t* result = g_resolver_original ? g_resolver_original(view, edx, out, flag) : out;
        if (out && view) {
            // The agent id lives at view+0x2C on the CCharAgent view object.
            const uint32_t agent_id = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(view) + 0x2C);
            *out = AgentTagColor::Instance().ApplyOverride(agent_id, *out);
        }
        return result;
    }
}  // namespace

AgentTagColor& AgentTagColor::Instance() {
    static AgentTagColor instance;
    return instance;
}

void AgentTagColor::Initialize() {
    if (initialized_.exchange(true))
        return;

    {
        std::lock_guard<std::mutex> lock(rules_mutex_);
        RebuildSnapshotLocked();
    }

    // Build-portable resolution: anchor on the "agent" assertion at AvApi.cpp
    // line 0x1e9 inside AvCharGetConsiderColor, then walk to the function start.
    // Anchor on the id-addressable wrapper AvCharGetConsiderColor (FUN_007d9cf0)
    // via its "agent" assertion, then derive the two functions we need from its
    // body: the ManagerFindChar CALL at +0x07 and the GetConsiderColor resolver
    // CALL at +0x31. We HOOK the resolver (what the game's name-tag path calls);
    // ManagerFindChar services reads. Offsets are stable (fixed instruction
    // lengths); validate the CALL sites before trusting them.
    const uintptr_t assertion = GW::Scanner::FindAssertion("AvApi.cpp", "agent", 0x1e9, 0);
    if (!assertion)
        return;

    const uintptr_t wrapper = GW::Scanner::ToFunctionStart(assertion, 0xFFF);
    if (!wrapper)
        return;

    if (*reinterpret_cast<uint8_t*>(wrapper + 0x07) != 0xE8 ||
        *reinterpret_cast<uint8_t*>(wrapper + 0x31) != 0xE8)
        return;

    g_find_char = reinterpret_cast<FindCharFn>(GW::Scanner::FunctionFromNearCall(wrapper + 0x07));
    g_resolver = reinterpret_cast<ResolverFn>(GW::Scanner::FunctionFromNearCall(wrapper + 0x31));
    if (!g_resolver)
        return;

    const int rc = GW::HookBase::CreateHook(
        reinterpret_cast<void**>(&g_resolver),
        reinterpret_cast<void*>(&Detour_GetConsiderColor),
        reinterpret_cast<void**>(&g_resolver_original));
    if (rc != MH_OK)
        return;

    GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_resolver));
    hook_installed_.store(true);
}

void AgentTagColor::Terminate() {
    if (!initialized_.exchange(false))
        return;

    if (hook_installed_.exchange(false) && g_resolver) {
        GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_resolver));
        GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_resolver));
    }
    g_resolver = nullptr;
    g_resolver_original = nullptr;
    g_find_char = nullptr;
    enabled_.store(false);

    std::lock_guard<std::mutex> lock(rules_mutex_);
    agent_rules_.clear();
    allegiance_rules_.clear();
    snapshot_.reset();
}

void AgentTagColor::Enable() {
    enabled_.store(true);
}

void AgentTagColor::Disable() {
    enabled_.store(false);
}

bool AgentTagColor::IsEnabled() const {
    return enabled_.load();
}

bool AgentTagColor::IsHookInstalled() const {
    return hook_installed_.load();
}

void AgentTagColor::SetAgentColor(uint32_t agent_id, uint32_t argb) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    agent_rules_[agent_id] = argb;
    RebuildSnapshotLocked();
}

bool AgentTagColor::RemoveAgentColor(uint32_t agent_id) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    const bool erased = agent_rules_.erase(agent_id) != 0;
    if (erased)
        RebuildSnapshotLocked();
    return erased;
}

void AgentTagColor::SetAllegianceColor(int allegiance, uint32_t argb) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    allegiance_rules_[allegiance] = argb;
    RebuildSnapshotLocked();
}

bool AgentTagColor::RemoveAllegianceColor(int allegiance) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    const bool erased = allegiance_rules_.erase(allegiance) != 0;
    if (erased)
        RebuildSnapshotLocked();
    return erased;
}

void AgentTagColor::ClearRules() {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    agent_rules_.clear();
    allegiance_rules_.clear();
    RebuildSnapshotLocked();
}

std::map<uint32_t, uint32_t> AgentTagColor::GetAgentRules() const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    return agent_rules_;
}

std::map<int, uint32_t> AgentTagColor::GetAllegianceRules() const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    return allegiance_rules_;
}

uint32_t AgentTagColor::ReadConsiderColor(uint32_t agent_id) {
    if (!g_find_char || !g_resolver_original)
        return 0;
    // Resolve the view ourselves. ManagerFindChar returns null for non-living /
    // non-char agents (items/gadgets/signposts) — the same check the game's
    // wrapper does, which avoids its crashing "charAgent" assertion. Then call
    // the ORIGINAL resolver (trampoline) directly with the view so the read
    // reflects the game DEFAULT, unaffected by any installed override.
    void* view = g_find_char(agent_id);
    if (!view)
        return 0;
    uint32_t color = 0;
    g_resolver_original(view, nullptr, &color, 1);  // flag 1 = name-tag text color
    return color;
}

AgentTagColor::Diagnostics AgentTagColor::GetDiagnostics() const {
    Diagnostics diag;
    diag.initialized = initialized_.load();
    diag.hook_installed = hook_installed_.load();
    diag.enabled = enabled_.load();
    diag.resolver_calls_seen = diag_resolver_calls_.load();
    diag.agent_rule_hits = diag_agent_hits_.load();
    diag.allegiance_rule_hits = diag_allegiance_hits_.load();
    diag.last_agent_id = diag_last_agent_.load();
    diag.last_color = diag_last_color_.load();
    return diag;
}

void AgentTagColor::ResetDiagnostics() {
    diag_resolver_calls_.store(0);
    diag_agent_hits_.store(0);
    diag_allegiance_hits_.store(0);
    diag_last_agent_.store(0);
    diag_last_color_.store(0);
}

uint32_t AgentTagColor::ApplyOverride(uint32_t agent_id, uint32_t resolved_color) {
    if (!enabled_.load(std::memory_order_relaxed))
        return resolved_color;

    std::shared_ptr<const RuleSnapshot> snap;
    {
        std::lock_guard<std::mutex> lock(rules_mutex_);
        snap = snapshot_;
    }

    diag_resolver_calls_.fetch_add(1, std::memory_order_relaxed);
    diag_last_agent_.store(agent_id, std::memory_order_relaxed);

    uint32_t out = resolved_color;
    if (snap) {
        const auto agent_it = snap->agent_rules.find(agent_id);
        if (agent_it != snap->agent_rules.end()) {
            out = agent_it->second;
            diag_agent_hits_.fetch_add(1, std::memory_order_relaxed);
        }
        else if (!snap->allegiance_rules.empty()) {
            const int allegiance = ResolveAgentAllegiance(agent_id);
            const auto alleg_it = snap->allegiance_rules.find(allegiance);
            if (alleg_it != snap->allegiance_rules.end()) {
                out = alleg_it->second;
                diag_allegiance_hits_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    diag_last_color_.store(out, std::memory_order_relaxed);
    return out;
}

void AgentTagColor::RebuildSnapshotLocked() {
    auto snapshot = std::make_shared<RuleSnapshot>();
    snapshot->agent_rules = agent_rules_;
    snapshot->allegiance_rules = allegiance_rules_;
    snapshot_ = snapshot;
}

PYBIND11_EMBEDDED_MODULE(PyAgentTagColor, m) {
    m.doc() = "Agent name-tag color override. DLL init owns the resolver detour; "
              "Python controls enable/disable and the color rule store. Colors are ARGB 0xAARRGGBB.";

    m.def("enable", []() { AgentTagColor::Instance().Enable(); },
        "Enable color overriding (the resolver detour applies matching rules).");
    m.def("disable", []() { AgentTagColor::Instance().Disable(); },
        "Disable color overriding (game default colors return).");
    m.def("is_enabled", []() { return AgentTagColor::Instance().IsEnabled(); });
    m.def("is_hook_installed", []() { return AgentTagColor::Instance().IsHookInstalled(); });

    m.def("set_agent_color",
        [](uint32_t agent_id, uint32_t argb) { AgentTagColor::Instance().SetAgentColor(agent_id, argb); },
        py::arg("agent_id"), py::arg("argb"),
        "Override one agent's name-tag color (ARGB 0xAARRGGBB). Highest precedence.");
    m.def("remove_agent_color",
        [](uint32_t agent_id) { return AgentTagColor::Instance().RemoveAgentColor(agent_id); },
        py::arg("agent_id"));

    m.def("set_allegiance_color",
        [](int allegiance, uint32_t argb) { AgentTagColor::Instance().SetAllegianceColor(allegiance, argb); },
        py::arg("allegiance"), py::arg("argb"),
        "Override a whole allegiance category (1=Ally..6=NpcMinipet). Per-agent rules win.");
    m.def("remove_allegiance_color",
        [](int allegiance) { return AgentTagColor::Instance().RemoveAllegianceColor(allegiance); },
        py::arg("allegiance"));

    m.def("clear_rules", []() { AgentTagColor::Instance().ClearRules(); },
        "Drop all color overrides (agents revert to game defaults).");

    m.def("get_agent_rules", []() { return AgentTagColor::Instance().GetAgentRules(); });
    m.def("get_allegiance_rules", []() { return AgentTagColor::Instance().GetAllegianceRules(); });

    m.def("read_consider_color",
        [](uint32_t agent_id) { return AgentTagColor::Instance().ReadConsiderColor(agent_id); },
        py::arg("agent_id"),
        "Read the color the game currently computes for an agent (ARGB). Read-only; "
        "uses the original resolver, so it is unaffected by overrides.");

    m.def("get_diagnostics", []() {
        const auto diag = AgentTagColor::Instance().GetDiagnostics();
        py::dict out;
        out["initialized"] = diag.initialized;
        out["hook_installed"] = diag.hook_installed;
        out["enabled"] = diag.enabled;
        out["resolver_calls_seen"] = diag.resolver_calls_seen;
        out["agent_rule_hits"] = diag.agent_rule_hits;
        out["allegiance_rule_hits"] = diag.allegiance_rule_hits;
        out["last_agent_id"] = diag.last_agent_id;
        out["last_color"] = diag.last_color;
        return out;
    });
    m.def("reset_diagnostics", []() { AgentTagColor::Instance().ResetDiagnostics(); });
}
