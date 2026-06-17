#include "py_name_obfuscator.h"

#include <algorithm>
#include <cstring>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {
    void OnPlayerJoinInstancePacket(GW::HookStatus*, GW::Packet::StoC::PlayerJoinInstance* pak) {
        NameObfuscator::Instance().OnPlayerJoinInstance(pak);
    }
}

NameObfuscator& NameObfuscator::Instance() {
    static NameObfuscator instance;
    return instance;
}

void NameObfuscator::Initialize() {
    if (initialized_.exchange(true)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(alias_mutex_);
        RebuildAliasSnapshotLocked();
    }

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayerJoinInstance>(
        &player_join_hook_,
        OnPlayerJoinInstancePacket,
        -0x8000);

    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    diagnostics_.initialized = true;
    diagnostics_.player_join_hook_registered = true;
}

void NameObfuscator::Terminate() {
    if (!initialized_.exchange(false)) {
        return;
    }

    GW::StoC::RemoveCallbacks(&player_join_hook_);
    enabled_ = false;

    {
        std::lock_guard<std::mutex> lock(observed_mutex_);
        observed_players_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(alias_mutex_);
        aliases_.clear();
        alias_snapshot_.reset();
    }
    {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_ = {};
    }
}

void NameObfuscator::Enable() {
    enabled_ = true;
}

void NameObfuscator::Disable() {
    enabled_ = false;
}

bool NameObfuscator::IsEnabled() const {
    return enabled_.load();
}

bool NameObfuscator::IsMapReady() const {
    const auto instance_type = GW::Map::GetInstanceType();
    return GW::Map::GetIsMapLoaded()
        && !GW::Map::GetIsObserving()
        && instance_type != GW::Constants::InstanceType::Loading;
}

void NameObfuscator::SetAlias(const std::wstring& real_name, const std::wstring& fake_name) {
    std::lock_guard<std::mutex> lock(alias_mutex_);
    aliases_[real_name] = fake_name;
    RebuildAliasSnapshotLocked();
}

bool NameObfuscator::RemoveAlias(const std::wstring& real_name) {
    std::lock_guard<std::mutex> lock(alias_mutex_);
    const auto erased = aliases_.erase(real_name) != 0;
    if (erased) {
        RebuildAliasSnapshotLocked();
    }
    return erased;
}

void NameObfuscator::ClearAliases() {
    std::lock_guard<std::mutex> lock(alias_mutex_);
    aliases_.clear();
    RebuildAliasSnapshotLocked();
}

size_t NameObfuscator::AliasCount() const {
    std::lock_guard<std::mutex> lock(alias_mutex_);
    return aliases_.size();
}

std::map<std::wstring, std::wstring> NameObfuscator::GetAliases() const {
    std::lock_guard<std::mutex> lock(alias_mutex_);
    return aliases_;
}

void NameObfuscator::ClearObservedCache() {
    std::lock_guard<std::mutex> lock(observed_mutex_);
    observed_players_.clear();
}

size_t NameObfuscator::ObservedCount() const {
    std::lock_guard<std::mutex> lock(observed_mutex_);
    return observed_players_.size();
}

std::vector<NameObfuscator::ObservedPlayer> NameObfuscator::GetObservedPlayers() const {
    std::lock_guard<std::mutex> lock(observed_mutex_);
    return observed_players_;
}

NameObfuscator::Diagnostics NameObfuscator::GetDiagnostics() const {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    Diagnostics copy = diagnostics_;
    copy.enabled = enabled_.load();
    copy.current_map_ready = IsMapReady();
    copy.initialized = initialized_.load();
    return copy;
}

void NameObfuscator::ResetDiagnostics() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    const bool initialized = initialized_.load();
    const bool enabled = enabled_.load();
    const bool current_map_ready = IsMapReady();
    diagnostics_ = {};
    diagnostics_.initialized = initialized;
    diagnostics_.player_join_hook_registered = initialized;
    diagnostics_.enabled = enabled;
    diagnostics_.current_map_ready = current_map_ready;
}

bool NameObfuscator::LookupAlias(const std::wstring& real_name, std::wstring& fake_out) const {
    std::shared_ptr<const AliasSnapshot> snapshot;
    {
        std::lock_guard<std::mutex> lock(alias_mutex_);
        snapshot = alias_snapshot_;
    }
    if (!snapshot) {
        return false;
    }
    const auto it = std::find_if(snapshot->begin(), snapshot->end(), [&real_name](const AliasEntry& entry) {
        return entry.real_name == real_name;
    });
    if (it == snapshot->end()) {
        return false;
    }
    fake_out = it->fake_name;
    return true;
}

void NameObfuscator::OnPlayerJoinInstance(GW::Packet::StoC::PlayerJoinInstance* pak) {
    if (!pak) {
        return;
    }

    const bool map_ready = IsMapReady();
    {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.initialized = initialized_.load();
        diagnostics_.player_join_hook_registered = initialized_.load();
        diagnostics_.enabled = enabled_.load();
        diagnostics_.current_map_ready = map_ready;
        diagnostics_.player_packets_seen++;
    }

    if (!pak->player_name[0]) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.player_packets_empty_name++;
        return;
    }

    if (!enabled_.load()) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.player_packets_disabled++;
        return;
    }

    if (!map_ready) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.player_packets_map_not_ready++;
    }

    const std::wstring real_name = pak->player_name;
    std::wstring display_name = real_name;
    bool aliased = false;

    std::wstring fake_name;
    if (LookupAlias(real_name, fake_name)) {
        aliased = true;
        display_name = fake_name;
        const size_t max_chars = std::size(pak->player_name) - 1;
        const size_t to_copy = std::min(fake_name.size(), max_chars);
        std::memcpy(pak->player_name, fake_name.data(), to_copy * sizeof(wchar_t));
        pak->player_name[to_copy] = L'\0';

        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.alias_hits++;
    }

    RecordObservedPlayer({pak->player_number, pak->agent_id, real_name, display_name, aliased});
}

void NameObfuscator::RebuildAliasSnapshotLocked() {
    auto snapshot = std::make_shared<AliasSnapshot>();
    snapshot->reserve(aliases_.size());
    for (const auto& [real_name, fake_name] : aliases_) {
        snapshot->push_back({real_name, fake_name});
    }
    alias_snapshot_ = snapshot;
}

void NameObfuscator::RecordObservedPlayer(const ObservedPlayer& player) {
    std::unique_lock<std::mutex> lock(observed_mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        std::lock_guard<std::mutex> diag_lock(diagnostics_mutex_);
        diagnostics_.observed_trylock_skips++;
        return;
    }

    auto it = std::find_if(observed_players_.begin(), observed_players_.end(), [&player](const ObservedPlayer& existing) {
        return existing.player_number == player.player_number || existing.agent_id == player.agent_id;
    });

    if (it != observed_players_.end()) {
        *it = player;
    }
    else {
        observed_players_.push_back(player);
        if (observed_players_.size() > kMaxObservedPlayers) {
            observed_players_.erase(observed_players_.begin());
        }
    }

    std::lock_guard<std::mutex> diag_lock(diagnostics_mutex_);
    diagnostics_.observed_captures++;
}

PYBIND11_EMBEDDED_MODULE(PyNameObfuscator, m) {
    m.doc() = "Player name obfuscator runtime control. DLL initialization owns hooks; Python controls enable/disable, aliases, and caches.";

    py::class_<NameObfuscator::ObservedPlayer>(m, "ObservedPlayer")
        .def_readonly("player_number", &NameObfuscator::ObservedPlayer::player_number)
        .def_readonly("agent_id", &NameObfuscator::ObservedPlayer::agent_id)
        .def_readonly("real_name", &NameObfuscator::ObservedPlayer::real_name)
        .def_readonly("display_name", &NameObfuscator::ObservedPlayer::display_name)
        .def_readonly("aliased", &NameObfuscator::ObservedPlayer::aliased);

    m.def("enable", []() { NameObfuscator::Instance().Enable(); },
        "Turn name obfuscation on. Hooks are owned by DLL initialization.");
    m.def("disable", []() { NameObfuscator::Instance().Disable(); },
        "Turn name obfuscation off. Names already cached by the game persist until re-zone.");
    m.def("is_enabled", []() { return NameObfuscator::Instance().IsEnabled(); });
    m.def("is_map_ready", []() { return NameObfuscator::Instance().IsMapReady(); });

    m.def("set_alias",
        [](const std::wstring& real, const std::wstring& fake) {
            NameObfuscator::Instance().SetAlias(real, fake);
        },
        py::arg("real_name"), py::arg("fake_name"),
        "Register or update a real -> fake player-name mapping.");

    m.def("remove_alias",
        [](const std::wstring& real) { return NameObfuscator::Instance().RemoveAlias(real); },
        py::arg("real_name"));

    m.def("clear_aliases", []() { NameObfuscator::Instance().ClearAliases(); },
        "Drop every alias.");
    m.def("clear", []() { NameObfuscator::Instance().ClearAliases(); },
        "Alias for clear_aliases().");

    m.def("alias_count", []() { return (uint32_t)NameObfuscator::Instance().AliasCount(); });
    m.def("get_aliases", []() { return NameObfuscator::Instance().GetAliases(); });

    m.def("clear_observed_cache", []() { NameObfuscator::Instance().ClearObservedCache(); },
        "Drop the map-scoped observed player cache.");
    m.def("observed_count", []() { return (uint32_t)NameObfuscator::Instance().ObservedCount(); });
    m.def("get_observed_players", []() { return NameObfuscator::Instance().GetObservedPlayers(); });
    m.def("get_diagnostics", []() {
        const auto diag = NameObfuscator::Instance().GetDiagnostics();
        py::dict out;
        out["initialized"] = diag.initialized;
        out["player_join_hook_registered"] = diag.player_join_hook_registered;
        out["enabled"] = diag.enabled;
        out["current_map_ready"] = diag.current_map_ready;
        out["player_packets_seen"] = diag.player_packets_seen;
        out["player_packets_empty_name"] = diag.player_packets_empty_name;
        out["player_packets_disabled"] = diag.player_packets_disabled;
        out["player_packets_map_not_ready"] = diag.player_packets_map_not_ready;
        out["observed_captures"] = diag.observed_captures;
        out["observed_trylock_skips"] = diag.observed_trylock_skips;
        out["alias_hits"] = diag.alias_hits;
        return out;
    });
    m.def("reset_diagnostics", []() { NameObfuscator::Instance().ResetDiagnostics(); });
}
