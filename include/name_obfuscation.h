#pragma once
#ifndef NAME_OBFUSCATION_H
#define NAME_OBFUSCATION_H

#include "Headers.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// NameObfuscation
//
// Rewrites player names in incoming StoC packets BEFORE the game stores them,
// so all downstream rendering (nametags, party frame, target pane) shows the
// fake name. Also blanks the guild tag while active.
//
// Lifecycle:
//   Init()     -- registers StoC packet callbacks once on DLL init
//   Shutdown() -- removes callbacks and clears the alias table
// ---------------------------------------------------------------------------

class NameObfuscation {
public:
    static NameObfuscation& Instance() {
        static NameObfuscation instance;
        return instance;
    }

    void Init();
    void Shutdown();

    void Enable()  { enabled_ = true; }
    void Disable() { enabled_ = false; }
    bool IsEnabled() const { return enabled_; }

    void SetAlias(const std::wstring& real_name, const std::wstring& fake_name);
    void RemoveAlias(const std::wstring& real_name);
    void ClearAliases();
    size_t AliasCount();

    // packet-hook side
    bool LookupAlias(const wchar_t* real_name, std::wstring& fake_out);

private:
    NameObfuscation() = default;
    ~NameObfuscation() = default;
    NameObfuscation(const NameObfuscation&) = delete;
    NameObfuscation& operator=(const NameObfuscation&) = delete;

    std::atomic<bool> enabled_{false};
    std::mutex mutex_;
    std::unordered_map<std::wstring, std::wstring> aliases_;

    GW::HookEntry player_join_hook_{};
    GW::HookEntry guild_general_hook_{};
    bool hooks_registered_ = false;
};

#endif // NAME_OBFUSCATION_H
