#pragma once

#include "Headers.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class NameObfuscator {
public:
    struct AliasEntry {
        std::wstring real_name;
        std::wstring fake_name;
    };

    using AliasSnapshot = std::vector<AliasEntry>;

    struct ObservedPlayer {
        uint32_t player_number = 0;
        uint32_t agent_id = 0;
        std::wstring real_name;
        std::wstring display_name;
        bool aliased = false;
    };

    struct Diagnostics {
        bool initialized = false;
        bool player_join_hook_registered = false;
        bool enabled = false;
        bool current_map_ready = false;
        uint32_t player_packets_seen = 0;
        uint32_t player_packets_empty_name = 0;
        uint32_t player_packets_disabled = 0;
        uint32_t player_packets_map_not_ready = 0;
        uint32_t observed_captures = 0;
        uint32_t observed_trylock_skips = 0;
        uint32_t alias_hits = 0;
    };

    static constexpr size_t kMaxObservedPlayers = 256;

    static NameObfuscator& Instance();

    void Initialize();
    void Terminate();

    void Enable();
    void Disable();
    bool IsEnabled() const;
    bool IsMapReady() const;

    void SetAlias(const std::wstring& real_name, const std::wstring& fake_name);
    bool RemoveAlias(const std::wstring& real_name);
    void ClearAliases();
    size_t AliasCount() const;
    std::map<std::wstring, std::wstring> GetAliases() const;

    void ClearObservedCache();
    size_t ObservedCount() const;
    std::vector<ObservedPlayer> GetObservedPlayers() const;

    Diagnostics GetDiagnostics() const;
    void ResetDiagnostics();

    bool LookupAlias(const std::wstring& real_name, std::wstring& fake_out) const;
    void OnPlayerJoinInstance(GW::Packet::StoC::PlayerJoinInstance* pak);

private:
    NameObfuscator() = default;
    NameObfuscator(const NameObfuscator&) = delete;
    NameObfuscator& operator=(const NameObfuscator&) = delete;

    void RebuildAliasSnapshotLocked();
    void RecordObservedPlayer(const ObservedPlayer& player);

    mutable std::mutex alias_mutex_;
    std::map<std::wstring, std::wstring> aliases_;
    std::shared_ptr<const AliasSnapshot> alias_snapshot_;

    mutable std::mutex observed_mutex_;
    std::vector<ObservedPlayer> observed_players_;

    mutable std::mutex diagnostics_mutex_;
    Diagnostics diagnostics_;

    std::atomic<bool> initialized_ = false;
    std::atomic<bool> enabled_ = false;
    GW::HookEntry player_join_hook_;
};
