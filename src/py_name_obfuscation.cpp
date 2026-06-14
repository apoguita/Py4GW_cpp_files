#include "name_obfuscation.h"

#include <cstring>

namespace py = pybind11;

// ---------------------------------------------------------------------------
// Static StoC callbacks
// ---------------------------------------------------------------------------

static void OnPlayerJoinInstance(GW::HookStatus*, GW::Packet::StoC::PlayerJoinInstance* pak) {
    if (!pak || !pak->player_name[0]) return;

    auto& svc = NameObfuscation::Instance();
    if (!svc.IsEnabled()) return;

    std::wstring fake;
    if (svc.LookupAlias(pak->player_name, fake)) {
        // pak->player_name is wchar_t[32]; copy with bounds check.
        const size_t max_chars = 31;
        size_t to_copy = fake.size();
        if (to_copy > max_chars) to_copy = max_chars;
        std::memcpy(pak->player_name, fake.data(), to_copy * sizeof(wchar_t));
        pak->player_name[to_copy] = L'\0';
    }
}

static void OnGuildGeneral(GW::HookStatus*, GW::Packet::StoC::GuildGeneral* pak) {
    if (!pak) return;
    auto& svc = NameObfuscation::Instance();
    if (!svc.IsEnabled()) return;
    if (svc.AliasCount() == 0) return;
    pak->tag[0] = L'\0';
}

// ---------------------------------------------------------------------------
// NameObfuscation impl
// ---------------------------------------------------------------------------

void NameObfuscation::Init() {
    if (hooks_registered_) return;
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayerJoinInstance>(
        &player_join_hook_,
        [](GW::HookStatus* s, GW::Packet::StoC::PlayerJoinInstance* p) { OnPlayerJoinInstance(s, p); },
        -0x8000);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GuildGeneral>(
        &guild_general_hook_,
        [](GW::HookStatus* s, GW::Packet::StoC::GuildGeneral* p) { OnGuildGeneral(s, p); },
        -0x8000);

    hooks_registered_ = true;
}

void NameObfuscation::Shutdown() {
    if (hooks_registered_) {
        GW::StoC::RemoveCallbacks(&player_join_hook_);
        GW::StoC::RemoveCallbacks(&guild_general_hook_);
        hooks_registered_ = false;
    }
    enabled_ = false;
    ClearAliases();
}

void NameObfuscation::SetAlias(const std::wstring& real_name, const std::wstring& fake_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    aliases_[real_name] = fake_name;
}

void NameObfuscation::RemoveAlias(const std::wstring& real_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    aliases_.erase(real_name);
}

void NameObfuscation::ClearAliases() {
    std::lock_guard<std::mutex> lock(mutex_);
    aliases_.clear();
}

size_t NameObfuscation::AliasCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return aliases_.size();
}

bool NameObfuscation::LookupAlias(const wchar_t* real_name, std::wstring& fake_out) {
    if (!real_name) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = aliases_.find(real_name);
    if (it == aliases_.end()) return false;
    fake_out = it->second;
    return true;
}

// ---------------------------------------------------------------------------
// pybind11 module: PyNameObfuscation
// ---------------------------------------------------------------------------

PYBIND11_EMBEDDED_MODULE(PyNameObfuscation, m) {
    m.doc() = "Player name obfuscation: rewrites incoming PlayerJoinInstance "
              "and blanks guild tags so the UI shows fake names.";

    m.def("enable",  []() { NameObfuscation::Instance().Enable(); },
          "Turn the name rewrite on. Takes effect on the next PlayerJoinInstance packet.");
    m.def("disable", []() { NameObfuscation::Instance().Disable(); },
          "Turn the name rewrite off. Names already cached by the game persist until re-zone.");
    m.def("is_enabled", []() { return NameObfuscation::Instance().IsEnabled(); });

    m.def("set_alias",
          [](const std::wstring& real, const std::wstring& fake) {
              NameObfuscation::Instance().SetAlias(real, fake);
          },
          py::arg("real_name"), py::arg("fake_name"),
          "Register or update a real -> fake player-name mapping.");

    m.def("remove_alias",
          [](const std::wstring& real) { NameObfuscation::Instance().RemoveAlias(real); },
          py::arg("real_name"));

    m.def("clear", []() { NameObfuscation::Instance().ClearAliases(); },
          "Drop every alias.");

    m.def("alias_count", []() { return (uint32_t)NameObfuscation::Instance().AliasCount(); });
}
