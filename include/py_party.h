#pragma once
#include "Headers.h"



class Hero {
private:
    HeroType hero_id;
    std::string hero_name;
    Profession hero_profession;

    static const std::unordered_map<int, std::pair<std::string, int>> hero_data;

public:

    Hero(int hero_id);
    Hero(const std::string& hero_name);

    int GetId() const;
    std::string GetName() const;
    Profession GetProfession() const;

    bool operator==(const Hero& other) const {
        return hero_id == other.hero_id;
    }

    bool operator!=(const Hero& other) const {
        return hero_id != other.hero_id;
    }

    FlaggingState GetFlaggingState();
    bool SetFlaggingState(FlaggingState set_state);
	bool IsHeroFlagged(int hero);
	bool FlagHero(uint32_t idx);
	void UnflagAllHeroes();
};




class PartyTick {
private:
    bool ticked_ = false;  // Using snake_case for private variable
public:
    PartyTick(bool ticked) : ticked_(ticked) {}

    bool IsTicked() const {  // Following PascalCase
        return ticked_;
    }

    void SetTicked(bool ticked) {  // Following PascalCase
        ticked_ = ticked;
        GW::PartyMgr::Tick(ticked_);
    }

    bool ToggleTicked() {  // Following PascalCase
        ticked_ = !ticked_;
        GW::PartyMgr::Tick(ticked_);
        return ticked_;
    }

    void SetTickToggle(bool enable) {  // Following PascalCase
        GW::PartyMgr::SetTickToggle(enable);
    }
};

class PlayerPartyMember {
public:
    int login_number = 0;         // login number
    int called_target_id = 0;  // Changed to snake_case
    bool is_connected = false; // Changed to snake_case
    bool is_ticked = false;    // Changed to snake_case

    PlayerPartyMember(int player_id, int target_id, bool connected, bool ticked)
        : login_number(player_id), called_target_id(target_id), is_connected(connected), is_ticked(ticked) {}
};


class HeroPartyMember {
public:
    int agent_id = 0;            // Changed to snake_case
    int owner_player_id = 0;     // Changed to snake_case
    Hero hero_id = 0;   // Changed to snake_case
    int level = 0;               // Changed to snake_case
    Profession primary = 0;
    Profession secondary = 0;

    HeroPartyMember(int agent_id, int owner_id, int hero_id, int level)
        : agent_id(agent_id), owner_player_id(owner_id), hero_id(hero_id), level(level) {  // snake_case for variables
        GW::HeroInfo* hero_info = GW::PartyMgr::GetHeroInfo(hero_id);  // Changed heroinfo to hero_info
        if (!hero_info) return;

        primary = hero_info->primary;
        secondary = hero_info->secondary;
    }
};

class PetInfo {
public:
    uint32_t agent_id;
    uint32_t owner_agent_id;
    std::string pet_name;
    uint32_t model_file_id1;
    uint32_t model_file_id2;
    int behavior;
    uint32_t locked_target_id;

    PetInfo(uint32_t owner_agentid) {
        GW::PetInfo* pet = GW::PartyMgr::GetPetInfo(owner_agentid);
        if (!pet) return;

        agent_id = pet->agent_id;
        owner_agent_id = pet->owner_agent_id;
        wchar_t* p_name = pet->pet_name;
        std::wstring wide_name(p_name);
        auto name = std::string(wide_name.begin(), wide_name.end());
        pet_name = name;
        model_file_id1 = pet->model_file_id1;
        model_file_id2 = pet->model_file_id2;
        behavior = static_cast<int>(pet->behavior);
        locked_target_id = pet->locked_target_id;
    }
};


class HenchmanPartyMember {
public:
    int agent_id = 0;            
    Profession profession = 0;   
    int level = 0;               

    HenchmanPartyMember(int agent_id, int prof, int level)
        : agent_id(agent_id), profession(prof), level(level) {}  // snake_case for variables
};


class PyParty {
public:
    int party_id = 0;
    std::vector<PlayerPartyMember> players;
    std::vector<HeroPartyMember> heroes;
    std::vector<HenchmanPartyMember> henchmen;

    bool is_in_hard_mode = false;  // Changed to snake_case
    bool is_hard_mode_unlocked = false;  // Changed to snake_case
    int party_size = 0;  // Changed to snake_case
    int party_player_count = 0;  // Changed to snake_case
    int party_hero_count = 0;  // Changed to snake_case
    int party_henchman_count = 0;  // Changed to snake_case
    bool is_party_defeated = false;  // Changed to snake_case
    bool is_party_loaded = false;  // Changed to snake_case
    bool is_party_leader = false;  // Changed to snake_case

    PartyTick tick = false;  // Changed to snake_case

    PyParty();
    void GetContext();
    bool ReturnToOutpost();
    bool SetHardMode(bool flag);
    bool RespondToPartyRequest(int party_id, bool accept);
    bool AddHero(int hero_id);
    bool KickHero(int hero_id);
    bool KickAllHeroes();
    bool AddHenchman(int henchman_id);
    bool KickHenchman(int henchman_id);
    bool KickPlayer(int player_id);
    bool InvitePlayer(int player_id);
    bool LeaveParty();
    bool FlagHero(int agent_id, float x, float y);
    bool FlagAllHeroes(float x, float y);
    bool UnflagHero(int agent_id);
    bool UnflagAllHeroes();
    bool IsHeroFlagged(int hero);
	bool IsAllFlagged();
	float GetAllFlagX();
	float GetAllFlagY();
    int GetHeroAgentID(int hero_index);
    int GetAgentHeroID(int agent_id);
    int GetAgentIDByLoginNumber(int login_number);
    std::string GetPlayerNameByLoginNumber(int login_number);
    bool SearchParty(uint32_t search_type, std::string advertisement);
    bool SearchPartyCancel();
    bool SearchPartyReply(bool accept);
    void SetHeroBehavior(int hero_index, int behavior);
    void SetPetBehaviour(int behaviour, int lock_target_id);
    
    PetInfo GetPetInfo(uint32_t owner_agent_id);
    bool GetIsPlayerTicked(int player_id);
    void UseHeroSkillInstant(uint32_t hero_id, uint32_t skill_slot, uint32_t target_id);

    
};

namespace GW {
    namespace Effects {
        inline Effect* GetAgentEffectBySkillId(uint32_t agent_id, Constants::SkillID skill_id) {
            auto* effects = GetAgentEffects(agent_id);
            if (!effects) return nullptr;
            for (auto& effect : *effects) {
                if (effect.skill_id == skill_id)
                    return &effect;
            }
            return nullptr;
        }

        inline Buff* GetAgentBuffBySkillId(uint32_t agent_id, Constants::SkillID skill_id) {
            auto* buffs = GetAgentBuffs(agent_id);
            if (!buffs) return nullptr;
            for (auto& buff : *buffs) {
                if (buff.skill_id == skill_id)
                    return &buff;
            }
            return nullptr;
        }
        inline bool EffectExists(uint32_t agent_id, Constants::SkillID skill_id) {
            auto* effects = GetAgentEffects(agent_id);
            if (!effects) return false;
            for (auto& effect : *effects) {
                if (effect.skill_id == skill_id)
                    return true;
            }
            return false;
        }

        inline bool BuffExists(uint32_t agent_id, Constants::SkillID skill_id) {
            auto* buffs = GetAgentBuffs(agent_id);
            if (!buffs) return false;
            for (auto& buff : *buffs) {
                if (buff.skill_id == skill_id)
                    return true;
            }
            return false;
        }
    }
}


class EffectType {
public:
    int skill_id;  // Handle as int for Python
    int attribute_level;
    int effect_id;
    int agent_id;
    float duration;
    DWORD timestamp;
    DWORD time_elapsed;
    DWORD time_remaining;

    // Constructor that takes all properties explicitly, using Python-friendly int for skill_id
    EffectType(int skill_id, int attribute_level, int effect_id, int agent_id, float duration, DWORD timestamp, DWORD time_elapsed, DWORD time_remaining)
        : skill_id(skill_id), attribute_level(attribute_level), effect_id(effect_id), agent_id(agent_id), duration(duration), timestamp(timestamp), time_elapsed(time_elapsed), time_remaining(time_remaining) {}
};

class BuffType {
public:
    int skill_id; 
    int buff_id;
    int target_agent_id;

    // Constructor that takes all properties explicitly, using Python-friendly int for skill_id
    BuffType(int skill_id, int buff_id, int target_agent_id)
        : skill_id(skill_id), buff_id(buff_id), target_agent_id(target_agent_id) {}
};

class AgentEffects {
    int agent_id;
    std::vector<EffectType> Effects_list;
    std::vector<BuffType> Buffs_list;

public:
    // Constructor initializes agent_id and fetches both effects and buffs
    AgentEffects(int agentid) : agent_id(agentid) {
        // Fetch effects for the agent
        auto* effects = GW::Effects::GetAgentEffects(static_cast<uint32_t>(agent_id));
        if (effects) {
            for (const auto& effect : *effects) {
                Effects_list.emplace_back(
                    static_cast<int>(effect.skill_id),
                    effect.attribute_level,
                    effect.effect_id,
                    effect.agent_id,
                    effect.duration,
                    effect.timestamp,
                    effect.GetTimeElapsed(),
                    effect.GetTimeRemaining()
                );
            }
        }

        // Fetch buffs for the agent
        auto* buffs = GW::Effects::GetAgentBuffs(static_cast<uint32_t>(agent_id));
        if (buffs) {
            for (const auto& buff : *buffs) {
                Buffs_list.emplace_back(
                    static_cast<int>(buff.skill_id),
                    buff.buff_id,
                    buff.target_agent_id
                );
            }
        }
    }

    // Fetch effects for the current agent and return as std::vector<EffectType>
    std::vector<EffectType> GetEffects() const {
        return Effects_list;
    }

    // Fetch buffs for the current agent and return as std::vector<BuffType>
    std::vector<BuffType> GetBuffs() const {
        return Buffs_list;
    }

    // Return the count of effects for the current agent
    int GetEffectCount() const {
        return Effects_list.size();
    }

    // Return the count of buffs for the current agent
    int GetBuffCount() const {
        return Buffs_list.size();
    }

    // Check if a specific effect exists for the current agent and skill ID
    bool EffectExists(int skill_id) const {
        for (const auto& effect : Effects_list) {
            if (effect.skill_id == skill_id) {
                return true;
            }
        }
        return false;
    }

    // Check if a specific buff exists for the current agent and skill ID
    bool BuffExists(int skill_id) const {
        for (const auto& buff : Buffs_list) {
            if (buff.skill_id == skill_id) {
                return true;
            }
        }
        return false;
    }

    //drop buff
    void DropBuff(int skill_id) {
        GW::Effects::DropBuff(static_cast<uint32_t>(skill_id));
    }
};

