#pragma once
#include "HeroAI.h"

void HeroAI::initialize() {
	MemMgr.InitializeSharedMemory(MemMgr.MemData);
	MemMgr.GetBasePtr();
	std::string skills_json_path = dllDirectory + "/skills.json";
	CustomSkillData.Init(skills_json_path);
	synchTimer.start();
	GameState.combat.LastActivity.start();
	GameState.combat.IntervalSkillCasting = 0.0f;
	ScatterRest.start();
	Pathingdisplay.start();
	GW::InitPathfinding();
	UpdateGameVars();
    BipFixtimer.start();
	//AI_enabled = true;


    DebugMessage(L"HeroAI Initialized");
}

void HeroAI::cleanup() {
	MemMgr.MutexAquire();
	MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].IsActive = false;
	MemMgr.MutexRelease();
	CloseHandle(MemMgr.hMutex);
}


int HeroAI::GetPartyNumber() {

    if (GameVars.Map.IsPartyLoaded) {
        const auto party_info = GW::PartyMgr::GetPartyInfo();

        if (!party_info) { return 0; }

        for (int i = 0; i < GW::PartyMgr::GetPartyPlayerCount(); i++)
        {
            const auto playerId = GW::Agents::GetAgentIdByLoginNumber(party_info->players[i].login_number);
            if (playerId == GameVars.Target.Self.ID) { return i + 1; }
        }
    }
    return 0;
}


int HeroAI::GetMemPosByID(GW::AgentID agentID) {
    for (int i = 0; i < 8; i++) {
        if ((MemMgr.MemData->MemPlayers[i].IsActive) && (MemMgr.MemData->MemPlayers[i].ID == agentID)) {
            return i;
        }
    }
    return -1;
}



float HeroAI::CalculatePathDistance(float targetX, float targetY, ImDrawList* drawList, bool draw) {
    using namespace GW;

    PathPoint pathArray[30];
    uint32_t pathCount = 30;
    PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);
	GW::GamePos game_pos = GW::GamePos(agent_handler.x, agent_handler.y, agent_handler.z);
    PathPoint start{ game_pos, nullptr };
	Overlay overlay = Overlay();

    GamePos pos;
    pos.x = targetX;
    pos.y = targetY;
    pos.zplane =  game_pos.zplane;
    PathPoint goal{ pos,nullptr };

    if (GetSquareDistance(game_pos, goal.pos) > 4500 * 4500) { return -1; }

    if (!FindPath_Func) { return -2; }

    FindPath_Func(&start, &goal, 4500.0f, pathCount, &pathCount, pathArray);

    float altitude = 0;
    GamePos g1 = pathArray[0].pos;
    GW::Map::QueryAltitude(game_pos, 10, altitude);
    Point3D p0{ game_pos.x, game_pos.y, altitude };
	GW::Vec3f p0_vec3 = { game_pos.x, game_pos.y, altitude };
    Point3D p1{ g1.x, g1.y, altitude };
	GW::Vec3f p1_vec3 = { g1.x, g1.y, altitude };

    if (draw) { overlay.DrawLine3D(p0, p1, IM_COL32(0, 255, 0, 127), 3); }


    float totalDistance = GetDistance(game_pos, pathArray[0].pos);

    for (uint32_t i = 1; i < pathCount; ++i) {
        p0 = p1;
        p1.x = pathArray[i].pos.x;
        p1.y = pathArray[i].pos.y;
        GW::Map::QueryAltitude(g1, 10, altitude);

        if (draw) { overlay.DrawLine3D(p0, p1, IM_COL32(0, 255, 0, 127), 3); }
        totalDistance += GW::GetDistance(p0_vec3, p1_vec3);
    }


    return totalDistance;

}

bool HeroAI::IsPointValid(float x, float y) {
    GW::Vec2f pos{ x,y };
    PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);
    GW::GamePos game_pos = GW::GamePos(agent_handler.x, agent_handler.y, agent_handler.z);

    float distance = GW::GetDistance(game_pos, pos);

    float calcdistance = CalculatePathDistance(pos.x, pos.y, 0, false);

    if ((calcdistance - distance) > 50.0f) { return false; }
    if (calcdistance < distance) { return false; }
    return true;
}


bool HeroAI::HasEffect(const GW::Constants::SkillID skillID, uint32_t agentID)
{

    bool effect = GW::Effects::EffectExists(agentID, skillID);
    bool buff = GW::Effects::BuffExists(agentID, skillID);

    if ((effect) || (buff)) { return true; }

    int Skillptr = 0;

    const auto tempAgent = GW::Agents::GetAgentByID(agentID);
    if (!tempAgent) { return false; }
    const auto tempAgentLiving = tempAgent->GetAsAgentLiving();
    if (!tempAgentLiving) { return false; }

    Skillptr = CustomSkillData.GetPtrBySkillID(skillID);
    if (!Skillptr) { return false; }

    CustomSkillClass::CustomSkillDatatype vSkill = CustomSkillData.GetSkillByPtr(Skillptr);

    if ((vSkill.SkillType == GW::Constants::SkillType::WeaponSpell) && (tempAgentLiving->GetIsWeaponSpelled())) { return true; }

    GW::Skill skill = *GW::SkillbarMgr::GetSkillConstantData(skillID);

    if ((skill.type == GW::Constants::SkillType::WeaponSpell) && (tempAgentLiving->GetIsWeaponSpelled())) { return true; }

    MemMgr.MutexAquire();
    auto PlayerID = GetMemPosByID(agentID);
    if (PlayerID != -1) {
        if (MemMgr.MemData->MemPlayers[PlayerID].BuffList.Exists(skillID)) { MemMgr.MutexRelease(); return true; }
    }
    if (MemMgr.MemData->HeroBuffs.Exists(agentID, skillID)) { MemMgr.MutexRelease(); return true; }
    MemMgr.MutexRelease();

    return false;
}



bool HeroAI::IsMelee(GW::AgentID AgentID) {
    //enum WeaponType { bow = 1, axe, hammer, daggers, scythe, spear, sword, wand = 10, staff1 = 12, staff2 = 14 }; 
	PyAgent agent_handler = PyAgent(AgentID);
    const auto weapon_type = static_cast<WeaponType>(agent_handler.living_agent.weapon_type.Get());
    switch (weapon_type) {
    case axe:
    case hammer:
    case daggers:
    case scythe:
    case sword:
        return true;
        break;
    default:
        return false;
        break;
    }
    return false;

}

bool HeroAI::IsMartial(GW::AgentID AgentID) {
    //enum WeaponType { bow = 1, axe, hammer, daggers, scythe, spear, sword, wand = 10, staff1 = 12, staff2 = 14 }; 
    PyAgent agent_handler = PyAgent(AgentID);
    const auto weapon_type = static_cast<WeaponType>(agent_handler.living_agent.weapon_type.Get());

    switch (weapon_type) {
    case bow:
    case axe:
    case hammer:
    case daggers:
    case scythe:
    case spear:
    case sword:
        return true;
        break;
    default:
        return false;
        break;
    }
    return false;

}

//////////////////// ENEMY TARGETTING ///////////////////////////
uint32_t  HeroAI::TargetNearestEnemy(bool IsBigAggro) {
    
    std::vector<int> enemy_array = PyPlayer().GetEnemyArray();
    
    static int EnemyID = 0;
    static int tStoredEnemyID = 0;

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if ((!GameState.combat.StayAlert.hasElapsed(3000)) || (IsBigAggro)) {
        distance = GW::Constants::Range::Spellcast;
        maxdistance = GW::Constants::Range::Spellcast;
    }

    if (!enemy_array.empty()) {
        for (int EnemyID : enemy_array) {
            if (!EnemyID) { continue; }

            PyAgent enemy_handler = PyAgent(EnemyID);
            if (!enemy_handler.living_agent.is_alive) { continue; }
            GW::GamePos enemy_pos = GW::GamePos(enemy_handler.x, enemy_handler.y, enemy_handler.z);

            PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

            if (std::isnan(enemy_pos.x) || std::isnan(enemy_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, enemy_pos);

            if (dist < distance) {
                distance = dist;
                tStoredEnemyID = EnemyID;
            }
        }
    }

    if (distance != maxdistance) { return tStoredEnemyID; }
    else { 
    return 0; 
    }
    
}

uint32_t  HeroAI::TargetNearestEnemyCaster(bool IsBigAggro) {
    std::vector<int> enemy_array = PyPlayer().GetEnemyArray();

    static int EnemyID = 0;
    static int tStoredEnemyID = 0;

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if ((!GameState.combat.StayAlert.hasElapsed(3000)) || (IsBigAggro)) {
        distance = GW::Constants::Range::Spellcast;
        maxdistance = GW::Constants::Range::Spellcast;
    }

    if (!enemy_array.empty()) {
        for (int EnemyID : enemy_array) {
            if (!EnemyID) { continue; }

            PyAgent enemy_handler = PyAgent(EnemyID);
            if (!enemy_handler.living_agent.is_alive) { continue; }
            GW::GamePos enemy_pos = GW::GamePos(enemy_handler.x, enemy_handler.y, enemy_handler.z);

            if (IsMartial(EnemyID)) { continue; }

            PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

            if (std::isnan(enemy_pos.x) || std::isnan(enemy_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, enemy_pos);

            if (dist < distance) {
                distance = dist;
                tStoredEnemyID = EnemyID;
            }
        }
    }

    if (distance != maxdistance) { return tStoredEnemyID; }
    else { return 0; }
}


uint32_t  HeroAI::TargetNearestEnemyMartial(bool IsBigAggro) {
    std::vector<int> enemy_array = PyPlayer().GetEnemyArray();

    static int EnemyID = 0;
    static int tStoredEnemyID = 0;

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if ((!GameState.combat.StayAlert.hasElapsed(3000)) || (IsBigAggro)) {
        distance = GW::Constants::Range::Spellcast;
        maxdistance = GW::Constants::Range::Spellcast;
    }

    if (!enemy_array.empty()) {
        for (int EnemyID : enemy_array) {
            if (!EnemyID) { continue; }

            PyAgent enemy_handler = PyAgent(EnemyID);
            if (!enemy_handler.living_agent.is_alive) { continue; }
            GW::GamePos enemy_pos = GW::GamePos(enemy_handler.x, enemy_handler.y, enemy_handler.z);

            if (!IsMartial(EnemyID)) { continue; }

            PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

            if (std::isnan(enemy_pos.x) || std::isnan(enemy_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, enemy_pos);

            if (dist < distance) {
                distance = dist;
                tStoredEnemyID = EnemyID;
            }
        }
    }

    if (distance != maxdistance) { return tStoredEnemyID; }
    else { return 0; }
}

uint32_t  HeroAI::TargetNearestEnemyMelee(bool IsBigAggro) {
    std::vector<int> enemy_array = PyPlayer().GetEnemyArray();

    static int EnemyID = 0;
    static int tStoredEnemyID = 0;

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if ((!GameState.combat.StayAlert.hasElapsed(3000)) || (IsBigAggro)) {
        distance = GW::Constants::Range::Spellcast;
        maxdistance = GW::Constants::Range::Spellcast;
    }

    if (!enemy_array.empty()) {
        for (int EnemyID : enemy_array) {
            if (!EnemyID) { continue; }

            PyAgent enemy_handler = PyAgent(EnemyID);
            if (!enemy_handler.living_agent.is_alive) { continue; }
            GW::GamePos enemy_pos = GW::GamePos(enemy_handler.x, enemy_handler.y, enemy_handler.z);

            if (!IsMelee(EnemyID)) { continue; }

            PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

            if (std::isnan(enemy_pos.x) || std::isnan(enemy_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, enemy_pos);

            if (dist < distance) {
                distance = dist;
                tStoredEnemyID = EnemyID;
            }
        }
    }

    if (distance != maxdistance) { return tStoredEnemyID; }
    else { return 0; }
}

uint32_t  HeroAI::TargetNearestEnemyRanged(bool IsBigAggro) {
    std::vector<int> enemy_array = PyPlayer().GetEnemyArray();

    static int EnemyID = 0;
    static int tStoredEnemyID = 0;

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if ((!GameState.combat.StayAlert.hasElapsed(3000)) || (IsBigAggro)) {
        distance = GW::Constants::Range::Spellcast;
        maxdistance = GW::Constants::Range::Spellcast;
    }

    if (!enemy_array.empty()) {
        for (int EnemyID : enemy_array) {
            if (!EnemyID) { continue; }

            PyAgent enemy_handler = PyAgent(EnemyID);
            if (!enemy_handler.living_agent.is_alive) { continue; }
            GW::GamePos enemy_pos = GW::GamePos(enemy_handler.x, enemy_handler.y, enemy_handler.z);

            if (IsMelee(EnemyID)) { continue; }

            PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

            if (std::isnan(enemy_pos.x) || std::isnan(enemy_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, enemy_pos);

            if (dist < distance) {
                distance = dist;
                tStoredEnemyID = EnemyID;
            }
        }
    }

    if (distance != maxdistance) { return tStoredEnemyID; }
    else { return 0; }
}

//////////////////// END ENEMY TARGETTING ///////////////////////////
//////////////////// ALLY TARGETTING ////////////////////////////////


uint32_t  HeroAI::TargetLowestAlly(bool OtherAlly) {
    
	std::vector<int> ally_array = PyPlayer().GetAllyArray();
   
    float distance = GW::Constants::Range::Spellcast;
    int AllyID = 0;
    int tStoredAllyID = 0;
    float StoredLife = 2.0f;

    auto slot = GameState.combat.currentCastingSlot;

    if (ally_array.empty()) { return 0; }

    for (int AllyID : ally_array) {
        if (!AllyID || AllyID <= 0) { continue; }
        if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }
        if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, AllyID)) { continue; }

        PyAgent ally_handler = PyAgent(AllyID);
        if (!ally_handler.living_agent.is_alive) { continue; }
        GW::GamePos ally_pos = GW::GamePos(ally_handler.x, ally_handler.y, ally_handler.z);            

        PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
        GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

        if (std::isnan(ally_pos.x) || std::isnan(ally_pos.y) ||
            std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
            continue;
        }
        const auto dist = GW::GetDistance(player_pos, ally_pos);

        const float vlife = ally_handler.living_agent.hp;
        if (vlife < StoredLife && dist < distance && dist > 0.0f) {
            StoredLife = vlife;
            tStoredAllyID = AllyID;
        }
    }
    

    if (StoredLife != 2.0f && tStoredAllyID > 0) { return tStoredAllyID; }
   
    return 0;
}

uint32_t  HeroAI::TargetLowestAllyEnergy(bool OtherAlly) {
    std::vector<int> ally_array = PyPlayer().GetAllyArray();
    
    float distance = GW::Constants::Range::Spellcast;
    int AllyID = 0;
    int tStoredAllyenergyID = 0;
    float StoredEnergy = 2.0f;

    auto slot = GameState.combat.currentCastingSlot;

    if (!ally_array.empty()) {
        for (int AllyID : ally_array) {

            if (!AllyID) { continue; }
            if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

            if (HasEffect(GW::Constants::SkillID::Blood_is_Power, AllyID)) { continue; }
            if (HasEffect(GW::Constants::SkillID::Blood_Ritual, AllyID)) { continue; }

            PyAgent ally_handler = PyAgent(AllyID);
            if (!ally_handler.living_agent.is_alive) { continue; }
            GW::GamePos ally_pos = GW::GamePos(ally_handler.x, ally_handler.y, ally_handler.z);

            PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);
            
            if (std::isnan(ally_pos.x) || std::isnan(ally_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, ally_pos);

            //ENERGY CHECKS
            int PIndex = GetMemPosByID(AllyID);
            if (PIndex >= 0) {        
                MemMgr.MutexAquire();
                if ((MemMgr.MemData->MemPlayers[PIndex].IsActive) &&
                    (MemMgr.MemData->MemPlayers[PIndex].Energy < StoredEnergy) &&
                    (dist < distance)
                    ) {
                    StoredEnergy = MemMgr.MemData->MemPlayers[PIndex].Energy;
                    tStoredAllyenergyID = AllyID;
                }
                MemMgr.MutexRelease();
            }
        }
    }
    
    if (StoredEnergy != 2.0f) { return tStoredAllyenergyID; }
    
    return 0;

}

uint32_t  HeroAI::TargetLowestAllyCaster(bool OtherAlly) {
    std::vector<int> ally_array = PyPlayer().GetAllyArray();

    float distance = GW::Constants::Range::Spellcast;
    int AllyID = 0;
    int tStoredAllyID = 0;
    float StoredLife = 2.0f;

    auto slot = GameState.combat.currentCastingSlot;

    if (!ally_array.empty()) {
        for (int AllyID : ally_array) {

            if (!AllyID) { continue; }

            PyAgent ally_handler = PyAgent(AllyID);
            if (!ally_handler.living_agent.is_alive) { continue; }
            GW::GamePos ally_pos = GW::GamePos(ally_handler.x, ally_handler.y, ally_handler.z);

            if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, AllyID)) { continue; }

            const float vlife = ally_handler.living_agent.hp;

            if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

            if (IsMartial(AllyID)) { continue; }

            PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

            if (std::isnan(ally_pos.x) || std::isnan(ally_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, ally_pos);

            if ((vlife < StoredLife) && (dist < distance))
            {
                StoredLife = vlife;
                tStoredAllyID = AllyID;
            }
        }
    }
    if (StoredLife != 2.0f) { return tStoredAllyID; }

    return 0;
}

uint32_t  HeroAI::TargetLowestAllyMartial(bool OtherAlly) {
    std::vector<int> ally_array = PyPlayer().GetAllyArray();

    float distance = GW::Constants::Range::Spellcast;
    int AllyID = 0;
    int tStoredAllyID = 0;
    float StoredLife = 2.0f;

    auto slot = GameState.combat.currentCastingSlot;

    if (!ally_array.empty()) {
        for (int AllyID : ally_array) {

            if (!AllyID) { continue; }

            PyAgent ally_handler = PyAgent(AllyID);
            if (!ally_handler.living_agent.is_alive) { continue; }
            GW::GamePos ally_pos = GW::GamePos(ally_handler.x, ally_handler.y, ally_handler.z);

            if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, AllyID)) { continue; }

            const float vlife = ally_handler.living_agent.hp;

            if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

            if (!IsMartial(AllyID)) { continue; }

            PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

            if (std::isnan(ally_pos.x) || std::isnan(ally_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, ally_pos);

            if ((vlife < StoredLife) && (dist < distance))
            {
                StoredLife = vlife;
                tStoredAllyID = AllyID;
            }
        }
    }
    if (StoredLife != 2.0f) { return tStoredAllyID; }

    return 0;
}

uint32_t  HeroAI::TargetLowestAllyMelee(bool OtherAlly) {
    std::vector<int> ally_array = PyPlayer().GetAllyArray();

    float distance = GW::Constants::Range::Spellcast;
    int AllyID = 0;
    int tStoredAllyID = 0;
    float StoredLife = 2.0f;

    auto slot = GameState.combat.currentCastingSlot;

    if (!ally_array.empty()) {
        for (int AllyID : ally_array) {

            if (!AllyID) { continue; }

            PyAgent ally_handler = PyAgent(AllyID);
            if (!ally_handler.living_agent.is_alive) { continue; }
            GW::GamePos ally_pos = GW::GamePos(ally_handler.x, ally_handler.y, ally_handler.z);

            if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, AllyID)) { continue; }

            const float vlife = ally_handler.living_agent.hp;

            if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

            if (!IsMelee(AllyID)) { continue; }

            PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

            if (std::isnan(ally_pos.x) || std::isnan(ally_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, ally_pos);

            if ((vlife < StoredLife) && (dist < distance))
            {
                StoredLife = vlife;
                tStoredAllyID = AllyID;
            }
        }
    }
    if (StoredLife != 2.0f) { return tStoredAllyID; }

    return 0;
}

uint32_t  HeroAI::TargetLowestAllyRanged(bool OtherAlly) {
    std::vector<int> ally_array = PyPlayer().GetAllyArray();

    float distance = GW::Constants::Range::Spellcast;
    int AllyID = 0;
    int tStoredAllyID = 0;
    float StoredLife = 2.0f;

    auto slot = GameState.combat.currentCastingSlot;

    if (!ally_array.empty()) {
        for (int AllyID : ally_array) {

            if (!AllyID) { continue; }

            PyAgent ally_handler = PyAgent(AllyID);
            if (!ally_handler.living_agent.is_alive) { continue; }
            GW::GamePos ally_pos = GW::GamePos(ally_handler.x, ally_handler.y, ally_handler.z);

            if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, AllyID)) { continue; }

            const float vlife = ally_handler.living_agent.hp;

            if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

            if (IsMelee(AllyID)) { continue; }

            PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

            if (std::isnan(ally_pos.x) || std::isnan(ally_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, ally_pos);

            if ((vlife < StoredLife) && (dist < distance))
            {
                StoredLife = vlife;
                tStoredAllyID = AllyID;
            }
        }
    }
    if (StoredLife != 2.0f) { return tStoredAllyID; }

    return 0;
}

//////////////////// END ALLY TARGETTING ///////////////////////////

uint32_t  HeroAI::TargetDeadAllyInAggro() {
    std::vector<int> ally_array = PyPlayer().GetAllyArray();

    float distance = GW::Constants::Range::Spellcast;
    static GW::AgentID AllyID;
    static GW::AgentID tStoredAllyID = 0;
    float StoredLife = 2.0f;

    if (!ally_array.empty()) {
        for (int AllyID : ally_array) {

            if (!AllyID) { continue; }

            PyAgent ally_handler = PyAgent(AllyID);
            if (!ally_handler.living_agent.is_dead) { continue; }
            GW::GamePos ally_pos = GW::GamePos(ally_handler.x, ally_handler.y, ally_handler.z);

            PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(agent_handler.x, agent_handler.y, agent_handler.z);

            if (std::isnan(ally_pos.x) || std::isnan(ally_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, ally_pos);
            if (dist < distance) {
                return AllyID;
            }
        }
    }
    return 0;

}

uint32_t  HeroAI::TargetNearestCorpse() {
    std::vector<int> agent_array = PyPlayer().GetAgentArray();

    float distance = GW::Constants::Range::Spellcast;
    static GW::AgentID AllyID;
    static GW::AgentID tStoredAllyID = 0;
    float StoredLife = 2.0f;


    if (!agent_array.empty()) {
        for (int AllyID : agent_array) {

            if (AllyID <= 0) { continue; }

            PyAgent ally_handler = PyAgent(AllyID);
            if (!ally_handler.living_agent.is_dead) { continue; }
            GW::GamePos ally_pos = GW::GamePos(ally_handler.x, ally_handler.y, ally_handler.z);

            switch (static_cast<GW::Constants::Allegiance>(ally_handler.living_agent.allegiance.Get())) {
            case GW::Constants::Allegiance::Ally_NonAttackable:
            case GW::Constants::Allegiance::Neutral:
            case GW::Constants::Allegiance::Enemy:
            case GW::Constants::Allegiance::Npc_Minipet:
                break;
            default:
                continue;
                break;
            }


            PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(agent_handler.x, agent_handler.y, agent_handler.z);

            if (std::isnan(ally_pos.x) || std::isnan(ally_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, ally_pos);
            if (dist < distance) {
                return AllyID;
            }
        }
    }
    return 0;
}

uint32_t HeroAI::TargetNearestItem()
{
	std::vector<int> item_array = PyPlayer().GetItemArray();
    float distance = GW::Constants::Range::Spellcast;

    static GW::AgentID ItemID;
    static GW::AgentID tStoredItemID = 0;

    if (!item_array.empty()) {
        for (int ItemID : item_array) {

            if (ItemID <= 0) { continue; }

            PyAgent item_handler = PyAgent(ItemID);
            GW::GamePos item_pos = GW::GamePos(item_handler.x, item_handler.y, item_handler.z);

            const auto tAssignedTo = item_handler.item_agent.owner_id;

            if ((tAssignedTo) && (tAssignedTo != GameVars.Target.Self.ID)) { continue; }

            PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(agent_handler.x, agent_handler.y, agent_handler.z);

            if (!IsPointValid(agent_handler.x, agent_handler.y)) { continue; }

            if (std::isnan(item_pos.x) || std::isnan(item_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, item_pos);
            if (dist < distance) {
                distance = dist;
                tStoredItemID = ItemID;
            }
        }
    }
    if (distance != GW::Constants::Range::Spellcast) { return tStoredItemID; }
    else {
        return 0;
    }
}

uint32_t HeroAI::TargetNearestNpc() {

    std::vector<int> enemy_array = PyPlayer().GetNPCMinipetArray();

    static int EnemyID = 0;
    static int tStoredEnemyID = 0;

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;

    if (!enemy_array.empty()) {
        for (int EnemyID : enemy_array) {
            if (!EnemyID) { continue; }

            PyAgent enemy_handler = PyAgent(EnemyID);
            if (!enemy_handler.living_agent.is_alive) { continue; }
            GW::GamePos enemy_pos = GW::GamePos(enemy_handler.x, enemy_handler.y, enemy_handler.z);

            PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

            if (std::isnan(enemy_pos.x) || std::isnan(enemy_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, enemy_pos);

            if (dist < distance) {
                distance = dist;
                tStoredEnemyID = EnemyID;
            }
        }
    }

    if (distance != maxdistance) { return tStoredEnemyID; }
    else {
        return 0;
    }

}

uint32_t HeroAI::TargetNearestSpirit() {

    std::vector<int> enemy_array = PyPlayer().GetSpiritPetArray();

    static int EnemyID = 0;
    static int tStoredEnemyID = 0;

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;

    if (!enemy_array.empty()) {
        for (int EnemyID : enemy_array) {
            if (!EnemyID) { continue; }

            PyAgent enemy_handler = PyAgent(EnemyID);
            if (!enemy_handler.living_agent.is_alive) { continue; }
            GW::GamePos enemy_pos = GW::GamePos(enemy_handler.x, enemy_handler.y, enemy_handler.z);

            PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
            GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

            if (std::isnan(enemy_pos.x) || std::isnan(enemy_pos.y) ||
                std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
                continue;
            }
            const auto dist = GW::GetDistance(player_pos, enemy_pos);

            if (dist < distance) {
                distance = dist;
                tStoredEnemyID = EnemyID;
            }
        }
    }

    if (distance != maxdistance) { return tStoredEnemyID; }
    else {
        return 0;
    }

}

uint32_t  HeroAI::TargetLowestMinion() {
    std::vector<int> ally_array = PyPlayer().GetMinionArray();

    float distance = GW::Constants::Range::Spellcast;
    int AllyID = 0;
    int tStoredAllyID = 0;
    float StoredLife = 2.0f;

    auto slot = GameState.combat.currentCastingSlot;

    if (ally_array.empty()) { return 0; }

    for (int AllyID : ally_array) {
        if (!AllyID || AllyID <= 0) { continue; }
        if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

        PyAgent ally_handler = PyAgent(AllyID);
        if (!ally_handler.living_agent.is_alive) { continue; }
        GW::GamePos ally_pos = GW::GamePos(ally_handler.x, ally_handler.y, ally_handler.z);

        PyAgent player_handler = PyAgent(GameVars.Target.Self.ID);
        GW::GamePos player_pos = GW::GamePos(player_handler.x, player_handler.y, player_handler.z);

        if (std::isnan(ally_pos.x) || std::isnan(ally_pos.y) ||
            std::isnan(player_pos.x) || std::isnan(player_pos.y)) {
            continue;
        }
        const auto dist = GW::GetDistance(player_pos, ally_pos);

        const float vlife = ally_handler.living_agent.hp;
        if (vlife < StoredLife && dist < distance && dist > 0.0f) {
            StoredLife = vlife;
            tStoredAllyID = AllyID;
        }
    }


    if (StoredLife != 2.0f && tStoredAllyID > 0) { return tStoredAllyID; }


    return 0;
}

bool HeroAI::IsHeroFlagged(int hero) {
    if (hero <= GW::PartyMgr::GetPartyHeroCount()) {
        if (hero == 0) {
            const GW::Vec3f& allflag = GW::GetGameContext()->world->all_flag;
            if (allflag.x != 0 && allflag.y != 0 && (!std::isinf(allflag.x) || !std::isinf(allflag.y))) {
                return true;
            }
        }
        else {
            const GW::HeroFlagArray& flags = GW::GetGameContext()->world->hero_flags;
            if (!flags.valid() || static_cast<uint32_t>(hero) > flags.size()) {
                return false;
            }

            const GW::HeroFlag& flag = flags[hero - 1];
            if (!std::isinf(flag.flag.x) || !std::isinf(flag.flag.y)) {
                return true;
            }
        }
    }
    else {
        MemMgr.MutexAquire();
        bool ret = ((MemMgr.MemData->MemPlayers[hero - GW::PartyMgr::GetPartyHeroCount()].IsFlagged) && (MemMgr.MemData->MemPlayers[hero - GW::PartyMgr::GetPartyHeroCount()].IsActive)) ? true : false;
        MemMgr.MutexRelease();
        return ret;

    }


    return false;

}

bool HeroAI::UpdateGameVars() {
	PyMap map_handler = PyMap();

    GameVars.Map.IsLoading = (map_handler.instance_type.Get() == GW::Constants::InstanceType::Loading) ? true : false;
	GameVars.Map.IsExplorable = (map_handler.instance_type.Get() == GW::Constants::InstanceType::Explorable) ? true : false;
	GameVars.Map.IsOutpost = (map_handler.instance_type.Get() == GW::Constants::InstanceType::Outpost) ? true : false;
    GameVars.Map.IsMapReady = map_handler.is_map_ready;

    if (GameVars.Map.IsLoading) { return false; }
    if (!GameVars.Map.IsMapReady) { return false; }

    PyParty party_handler = PyParty();
    GameVars.Map.IsPartyLoaded = party_handler.is_party_loaded;
    if (!GameVars.Map.IsPartyLoaded) { return false; }
    
    if (map_handler.is_in_cinematic) { return false; }

    PyPlayer player_handler = PyPlayer();
    PyAgent player_agent_handler = PyAgent(player_handler.id);

	GameVars.Target.Self.ID = player_handler.id;  
    std::vector<PlayerPartyMember> players = party_handler.players;
    GameVars.Party.LeaderID = party_handler.GetAgentIDByLoginNumber(players[0].login_number);
    GameVars.Party.SelfPartyNumber = GetPartyNumber();

    
    /// CANDIDATES
    
    if (GameVars.Map.IsOutpost)
    {
        Scandidates candidateSelf;

		candidateSelf.player_ID = player_agent_handler.living_agent.player_number;
        candidateSelf.MapID = map_handler.map_id.Get();
		candidateSelf.MapRegion = static_cast<GW::Constants::ServerRegion>(map_handler.server_region.Get());
        candidateSelf.MapDistrict = map_handler.district;
        MemMgr.MutexAquire();
        CandidateUniqueKey = MemMgr.MemData->NewCandidates.Subscribe(candidateSelf);
        MemMgr.MemData->NewCandidates.DoKeepAlive(CandidateUniqueKey);
        MemMgr.MutexRelease();
    }

    //END CANDIDATES
    
    //receive python commands

    MemMgr.MutexAquire();
    for (int i = 0; i < 8; i++) {

        if (PythonVars[i].Looting.command_issued) {
            MemMgr.MemData->GameState[i].state.Looting = PythonVars[i].Looting.bool_parameter;
            PythonVars[i].Looting.command_issued = false;
        }

        if (PythonVars[i].Following.command_issued) {
            MemMgr.MemData->GameState[i].state.Following = PythonVars[i].Following.bool_parameter;
            PythonVars[i].Following.command_issued = false;
        }

        if (PythonVars[i].Combat.command_issued) {
            MemMgr.MemData->GameState[i].state.Combat = PythonVars[i].Combat.bool_parameter;
            PythonVars[i].Combat.command_issued = false;
        }

        if (PythonVars[i].Targetting.command_issued) {
            MemMgr.MemData->GameState[i].state.Targetting = PythonVars[i].Targetting.bool_parameter;
            PythonVars[i].Targetting.command_issued = false;
        }

        if (PythonVars[i].Collision.command_issued) {
            MemMgr.MemData->GameState[i].state.Collision = PythonVars[i].Collision.bool_parameter;
            PythonVars[i].Collision.command_issued = false;
        }

        for (int j = 0; j < 8; j++) {
            if (PythonVars[i].Skills[j].command_issued) {
                MemMgr.MemData->GameState[i].CombatSkillState[j] = PythonVars[i].Skills[j].bool_parameter;
                PythonVars[i].Skills[j].command_issued = false;
            }
        }

        if (PythonVars[i].Flagging.command_issued) {

            MemMgr.MemData->MemPlayers[i].IsFlagged = true;
            MemMgr.MemData->MemPlayers[i].IsAllFlag = (i == 0) ? true : false;
            MemMgr.MemData->MemPlayers[i].FlagPos.x = PythonVars[i].Flagging.parameter_3;
            MemMgr.MemData->MemPlayers[i].FlagPos.y = PythonVars[i].Flagging.parameter_4;

        }

        if (PythonVars[i].UnFlagging.command_issued) {
            MemMgr.MemData->MemPlayers[i].IsFlagged = false;
            MemMgr.MemData->MemPlayers[i].IsAllFlag = false;
            MemMgr.MemData->MemPlayers[i].FlagPos.x = 0;
            MemMgr.MemData->MemPlayers[i].FlagPos.y = 0;
        }

        if (PythonVars[i].Resign.command_issued) {
            MemMgr.MemData->command[i] = rc_Resign;
            PythonVars[i].Resign.command_issued = false;
        }

        if (PythonVars[i].Identify.command_issued) {
            MemMgr.MemData->command[i] = rc_ID;
            PythonVars[i].Identify.command_issued = false;
        }

        if (PythonVars[i].TakeQuest.command_issued) {
            MemMgr.MemData->command[i] = rc_GetQuest;
            PythonVars[i].TakeQuest.command_issued = false;
        }

        if (PythonVars[i].Salvage.command_issued) {
            MemMgr.MemData->command[i] = rc_Salvage;
            PythonVars[i].Salvage.command_issued = false;
        }

    }
    MemMgr.MutexRelease();
    
    
    //Synch Remote Control
    MemMgr.MutexAquire();
    GameState.state.Following = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Following;
    GameState.state.Combat = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Combat;
    GameState.state.Targetting = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Targetting;
    GameState.state.Collision = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Collision;
    GameState.state.Looting = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Looting;
    MemMgr.MutexRelease();

    MemMgr.MutexAquire();
    for (int i = 0; i < 8; i++) {
        GameState.CombatSkillState[i] = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[i];
    }
    MemMgr.MutexRelease();
    MemMgr.MutexAquire();
    RangedRangeValue = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.RangedRangeValue;
    MeleeRangeValue = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MeleeRangeValue;
    Fields.Agent.CollisionRadius = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MemFields.Agent.CollisionRadius;
    Fields.Agent.AttractionStrength = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MemFields.Agent.AttractionStrength;
    Fields.Agent.RepulsionStrength = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MemFields.Agent.RepulsionStrength;

    Fields.Enemy.CollisionRadius = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MemFields.Enemy.CollisionRadius;
    Fields.Enemy.AttractionStrength = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MemFields.Enemy.AttractionStrength;
    Fields.Enemy.RepulsionStrength = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MemFields.Enemy.RepulsionStrength;

    Fields.Spirit.CollisionRadius = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MemFields.Spirit.CollisionRadius;
    Fields.Spirit.AttractionStrength = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MemFields.Spirit.AttractionStrength;
    Fields.Spirit.RepulsionStrength = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MemFields.Spirit.RepulsionStrength;

    if (RangedRangeValue == 0) {
        RangedRangeValue = GW::Constants::Range::Spellcast;
        MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.RangedRangeValue = RangedRangeValue;
    }
    MemMgr.MutexRelease();
    MemMgr.MutexAquire();
    if (MeleeRangeValue == 0) {
        MeleeRangeValue = GW::Constants::Range::Spellcast;
        MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MeleeRangeValue = MeleeRangeValue;
    }

    MemMgr.MutexRelease();

    
    //KEEP ALIVE
    if (++elapsedCycles > 10000) {
        elapsedCycles = 0;
    };
    MemMgr.MutexAquire();
    if ((GameVars.Party.SelfPartyNumber == 1) && (elapsedCycles >= 50)) {
        for (int i = 1; i < 8; i++) {
            if (MemMgr.MemData->MemPlayers[i].IsActive) {
                if (MemMgr.MemData->MemPlayers[i].Keepalivecounter == 0)
                {
                    MemMgr.MemData->MemPlayers[i].IsActive = false;
                }
                else
                {
                    MemMgr.MemData->MemPlayers[i].Keepalivecounter = 0;
                }
            }
        }
        //synchTimer.reset();
        elapsedCycles = 0;
    }
    MemMgr.MutexRelease();
    
    //SELF SUBSCRIBE
    MemMgr.MutexAquire();
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].ID = GameVars.Target.Self.ID;
	MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].Energy = player_agent_handler.living_agent.energy;
	MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].energyRegen = player_agent_handler.living_agent.energy_regen;
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].IsActive = true;
    ++MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].Keepalivecounter;
    if (MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].Keepalivecounter > 10000) {
        MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].Keepalivecounter = 1;
    }; 
    MemMgr.MutexRelease();

    
    ///////// PYTHON MESSAGING
    
    MemMgr.MutexAquire();

    for (int i = 0; i < 8; i++) {

        PythonVars[i].is_active = MemMgr.MemData->MemPlayers[i].IsActive;
        PythonVars[i].Looting.value = MemMgr.MemData->GameState[i].state.Looting;
        PythonVars[i].Following.value = MemMgr.MemData->GameState[i].state.Following;
        PythonVars[i].Combat.value = MemMgr.MemData->GameState[i].state.Combat;
        PythonVars[i].Targetting.value = MemMgr.MemData->GameState[i].state.Targetting;
        PythonVars[i].Collision.value = MemMgr.MemData->GameState[i].state.Collision;
        PythonVars[i].agent_id = MemMgr.MemData->MemPlayers[i].ID;
        PythonVars[i].energy = MemMgr.MemData->MemPlayers[i].Energy;
        PythonVars[i].energy_regen = MemMgr.MemData->MemPlayers[i].energyRegen;

        for (int j = 0; j < 8; j++) {
            PythonVars[i].Skills[j].value = MemMgr.MemData->GameState[i].CombatSkillState[j];
        }
    }

    MemMgr.MutexRelease();
    
    
    //EXPLORABLE ONLY
    

    if (GameVars.Map.IsExplorable) {
        UnflagNeeded = true;
    }
    else {
        if (UnflagNeeded) {
            for (int i = 1; i < 9; i++) { UnFlagAIHero(i); }
        }
		UnflagNeeded = false;
        return false;
    } 

    
    if (!player_agent_handler.living_agent.is_alive) { return false; }

    GW::GamePos player_pos = GW::GamePos(player_agent_handler.x, player_agent_handler.y, player_agent_handler.z);

	PyAgent leader_agent_handler = PyAgent(GameVars.Party.LeaderID);
    GW::GamePos leader_pos = GW::GamePos(leader_agent_handler.x, leader_agent_handler.y, leader_agent_handler.z);

    GameVars.Party.DistanceFromLeader = GW::GetDistance(player_pos, leader_pos);
    if (GameVars.Party.DistanceFromLeader >= GW::Constants::Range::Compass) { return false; }
    
    
    //FLAGS
    if (GameVars.Party.SelfPartyNumber == 1) {

        const GW::Vec3f& allflag = GW::GetGameContext()->world->all_flag;

        if ((IsHeroFlagged(0)) &&
            (allflag.x) &&
            (allflag.x) &&
            (!MemMgr.MemData->MemPlayers[0].IsFlagged)
            ) {
            MemMgr.MutexAquire();
            MemMgr.MemData->MemPlayers[0].IsFlagged = true;
            MemMgr.MemData->MemPlayers[0].FlagPos.x = allflag.x;
            MemMgr.MemData->MemPlayers[0].FlagPos.y = allflag.y;
            MemMgr.MemData->MemPlayers[0].IsAllFlag = true;
            GameVars.Party.Flags[0] = true;
            MemMgr.MutexRelease();
        }
        else {
            if (!MemMgr.MemData->MemPlayers[0].IsAllFlag) {
                MemMgr.MutexAquire();
                MemMgr.MemData->MemPlayers[0].IsFlagged = false;
                MemMgr.MemData->MemPlayers[0].IsAllFlag = false;
                MemMgr.MemData->MemPlayers[0].FlagPos.x = 0;
                MemMgr.MemData->MemPlayers[0].FlagPos.y = 0;
                GameVars.Party.Flags[0] = false;
                MemMgr.MutexRelease();
            }
        }

        for (int i = 0; i < 8; i++) {
            GameVars.Party.Flags[i] = IsHeroFlagged(i);
        }


    }

    
    //////////////// BUFFS ////////////////////

    if (++elapsedBuffCycles > 10000) {
        elapsedBuffCycles = 0;
    };
    if (elapsedBuffCycles >= 100) {

        auto* buffs = GW::Effects::GetPlayerBuffs();

        MemMgr.MutexAquire();
        MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].BuffList.Clear();

        if (buffs) {
            for (auto& buff : *buffs) {
                MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].BuffList.Register(buff.skill_id);
            }
        }

        auto* effects = GW::Effects::GetPlayerEffects();
        if (effects) {
            for (auto& effect : *effects) {
                MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].BuffList.Register(effect.skill_id);
            }
        }
        MemMgr.MutexRelease();

        /////////////// HERO BUFFS ////////////////

        MemMgr.MutexAquire();
        if (GameVars.Party.SelfPartyNumber == 1) {
            MemMgr.MemData->HeroBuffs.Clear();
        }
        MemMgr.MutexRelease();

        elapsedBuffCycles = 0;
    }

    
    //if (elapsedBuffCycles-1 % 5 == 0) //-1 for visual bug
    {
        std::vector<int> ally_array = player_handler.GetAllyArray();

        if (!ally_array.empty()) {
            MemMgr.MutexAquire();

            for (int AllyID : ally_array) {
                if (!AllyID) { continue; }
				if (PyAgent(AllyID).living_agent.is_player) { continue; }

                auto* herobuffs = GW::Effects::GetAgentBuffs(AllyID);

                if (herobuffs) {
                    for (auto& herobuff : *herobuffs) {
                        MemMgr.MemData->HeroBuffs.Register(AllyID, herobuff.skill_id);
                    }
                }

                auto* heroeffects = GW::Effects::GetAgentEffects(AllyID);
                if (heroeffects) {
                    for (auto& heroeffect : *heroeffects) {
                        MemMgr.MemData->HeroBuffs.Register(AllyID, heroeffect.skill_id);
                    }
                }
            }

            MemMgr.MutexRelease();
        }
    }
    //////////////  END HERO BUFFS ///////////// 

    ////////////////END BUFF //////////////////
     
    GameVars.Target.Nearest.ID = 0;
    GameVars.Target.Nearest.ID = TargetNearestEnemy();

    GameVars.Target.NearestCaster.ID = 0;
    GameVars.Target.NearestCaster.ID = TargetNearestEnemyCaster(false);

    GameVars.Target.NearestMartial.ID = 0;
    GameVars.Target.NearestMartial.ID = TargetNearestEnemyMartial(false);

    GameVars.Target.NearestMartialMelee.ID = 0;
    GameVars.Target.NearestMartialMelee.ID = TargetNearestEnemyMelee(false);

    GameVars.Target.NearestMartialRanged.ID = 0;
    GameVars.Target.NearestMartialRanged.ID = TargetNearestEnemyRanged(false);

    GameVars.Party.InAggro = (GameVars.Target.Nearest.ID > 0) ? true : false;

    GameVars.Target.Called.ID = 0;

    GameVars.Target.LowestAlly.ID = 0;

    
    GameVars.Target.LowestOtherAlly.ID = 0;
    GameVars.Target.LowestOtherAlly.ID = TargetLowestAlly(true);
    
    GameVars.Target.LowestEnergyOtherAlly.ID = 0;
    GameVars.Target.LowestEnergyOtherAlly.ID = TargetLowestAllyEnergy(true);
    
    GameVars.Target.LowestAllyCaster.ID = 0;
    GameVars.Target.LowestAllyCaster.ID = TargetLowestAllyCaster(false);

    GameVars.Target.LowestAllyMartial.ID = 0;
    GameVars.Target.LowestAllyMartial.ID = TargetLowestAllyMartial(false);

    GameVars.Target.LowestAllyMartialMelee.ID = 0;
    GameVars.Target.LowestAllyMartialMelee.ID = TargetLowestAllyMelee(false);

    GameVars.Target.LowestAllyMartialRanged.ID = 0;
    GameVars.Target.LowestAllyMartialRanged.ID = TargetLowestAllyRanged(false);

    GameVars.Target.DeadAlly.ID = 0;
    GameVars.Target.DeadAlly.ID = TargetDeadAllyInAggro();

    GameVars.Target.NearestSpirit.ID = 0;
    GameVars.Target.NearestSpirit.ID = TargetNearestSpirit();

    GameVars.Target.LowestMinion.ID = 0;
    GameVars.Target.LowestMinion.ID = TargetLowestMinion();

    GameVars.Target.NearestCorpse.ID = 0;
    GameVars.Target.NearestCorpse.ID = TargetNearestCorpse();
    
    
    GameVars.SkillBar.SkillBar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!GameVars.SkillBar.SkillBar || !GameVars.SkillBar.SkillBar->IsValid()) { return false; }

    for (int i = 0; i < 8; i++) {
        GameVars.SkillBar.Skills[i].Skill = GameVars.SkillBar.SkillBar->skills[i];
        GameVars.SkillBar.Skills[i].Data = *GW::SkillbarMgr::GetSkillConstantData(GameVars.SkillBar.Skills[i].Skill.skill_id);

        int Skillptr = 0;

        Skillptr = CustomSkillData.GetPtrBySkillID(GameVars.SkillBar.Skills[i].Skill.skill_id);

        if (Skillptr != 0) {
            GameVars.SkillBar.Skills[i].CustomData = CustomSkillData.GetSkillByPtr(Skillptr);
            GameVars.SkillBar.Skills[i].HasCustomData = true;
        }
        else { GameVars.SkillBar.Skills[i].HasCustomData = false; }
    }

    
    //interrupt/ remove ench/ REz / heal / cleanse Hex /cleanse condi / buff / benefical / Hex / Dmg / attack
    // skill priority order
    int ptr = 0;
    bool ptr_chk[8] = { false,false,false,false,false,false,false,false };
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Interrupt)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Enchantment_Removal)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Healing)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Hex_Removal)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Condi_Cleanse)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == EnergyBuff)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Resurrection)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Form)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Enchantment)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::EchoRefrain)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::WeaponSpell)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Chant)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Preparation)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Ritual)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Ward)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Hex)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Trap)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Stance)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Shout)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Glyph)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Signet)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }

    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Form)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Enchantment)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::EchoRefrain)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::WeaponSpell)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Chant)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Preparation)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Ritual)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Ward)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Hex)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Trap)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Stance)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Shout)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Glyph)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Signet)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }

    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Buff)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }

    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.combo == 3)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } } //dual attack
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.combo == 2)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } } //off-hand attack
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.combo == 1)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } } //lead attack
    for (int i = 0; i < 8; i++) { if (!ptr_chk[i]) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }


    if (GameState.combat.LastActivity.hasElapsed(GameState.combat.IntervalSkillCasting)) { GameState.combat.IsSkillCasting = false; }
	PlayerPos.x = player_agent_handler.x;
	PlayerPos.y = player_agent_handler.y;


    GridSize = Fields.Agent.CollisionRadius;
    
    return true;
    
}

void HeroAI::DebugMessage(const wchar_t* message) {
    WriteChat(GW::Chat::CHANNEL_GWCA1, message, L"Hero AI");
}



uint32_t HeroAI::GetNextUnidentifiedItem(IdentifyAllType type)
{
    size_t start_slot = 0;
    auto start_bag_id = GW::Constants::Bag::Backpack;

    for (auto bag_id = start_bag_id; bag_id <= GW::Constants::Bag::Equipment_Pack; bag_id++) {
        GW::Bag* bag = GW::Items::GetBag(bag_id);
        size_t slot = start_slot;
        start_slot = 0;
        if (!bag) {
            continue;
        }
        GW::ItemArray& items = bag->items;
        for (size_t i = slot, size = items.size(); i < size; i++) {
            if (!items[i]) continue;

            uint32_t item_id = items[i]->item_id;

            SafeItem safe_item = SafeItem(item_id);
            if (safe_item.IsIdentified) continue;
            if (safe_item.IsRarityGreen) continue;
            if (safe_item.item_type == static_cast<uint32_t>(GW::Constants::ItemType::Minipet)) continue;

            switch (type) {
            case IdentifyAllType::All:
                return safe_item.item_id;
            case IdentifyAllType::Blue:
                if (safe_item.IsRarityBlue) { safe_item.item_id; }
                break;
            case IdentifyAllType::Purple:
                if (safe_item.IsRarityPurple) { safe_item.item_id; }
                break;
            case IdentifyAllType::Gold:
                if (safe_item.IsRarityGold) { safe_item.item_id; }
                break;
            default:
                break;
            }
        }
    }
    return 0;
}

uint32_t HeroAI::GetIDKit()
{
    size_t start_slot = 0;
    auto start_bag_id = GW::Constants::Bag::Backpack;

    for (auto bag_id = start_bag_id; bag_id <= GW::Constants::Bag::Equipment_Pack; bag_id++) {
        GW::Bag* bag = GW::Items::GetBag(bag_id);
        size_t slot = start_slot;
        start_slot = 0;
        if (!bag) {
            continue;
        }
        GW::ItemArray& items = bag->items;
        for (size_t i = slot, size = items.size(); i < size; i++) {
            
            if (!items[i]) {
                continue;
            }
            uint32_t item_id  = static_cast<uint32_t>(items[i]->item_id);
			SafeItem safe_item = SafeItem(item_id);
			if (safe_item.IsIDKit) {
                return safe_item.item_id;
			}   
        }
    }
    return 0;
}

void HeroAI::IdentifyAll(IdentifyAllType type)
{
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"ID Session Started", L"Hero AI");
    int count = 0;
    int unid = GetNextUnidentifiedItem(type);
    int kit = GetIDKit();

    while (unid && kit) {
		Inventory inventory_handler;
		inventory_handler.IdentifyItem(kit, unid);
        count++;
        unid = GetNextUnidentifiedItem(type);
        kit = GetIDKit();
        if (unid && kit) {
            Sleep(100);
        }
    }
    std::string countStr = std::to_string(count);
    std::wstring countWStr = std::wstring(countStr.begin(), countStr.end());
    std::wstring msg = L"ID Session Finished. (" + countWStr + L") items Identified.";

    WriteChat(GW::Chat::CHANNEL_GWCA1, msg.c_str(), L"Hero AI");
}

bool HeroAI::ThereIsSpaceInInventory()
{
    GW::Bag** bags = GW::Items::GetBagArray();
    if (!bags) {
        return false;
    }
    size_t end_bag = static_cast<size_t>(GW::Constants::Bag::Bag_2);

    for (size_t bag_idx = static_cast<size_t>(GW::Constants::Bag::Backpack); bag_idx <= end_bag; bag_idx++) {
        GW::Bag* bag = bags[bag_idx];
        if (!bag || !bag->items.valid()) {
            continue;
        }
        for (size_t slot = 0; slot < bag->items.size(); slot++) {
            if (!bag->items[slot]) {
                return true;
            }
        }
    }
    return false;
}

uint32_t HeroAI::GetSalvageKit()
{
    size_t start_slot = 0;
    auto start_bag_id = GW::Constants::Bag::Backpack;

    for (auto bag_id = start_bag_id; bag_id <= GW::Constants::Bag::Equipment_Pack; bag_id++) {
        GW::Bag* bag = GW::Items::GetBag(bag_id);
        size_t slot = start_slot;
        start_slot = 0;
        if (!bag) {
            continue;
        }
        GW::ItemArray& items = bag->items;
        for (size_t i = slot, size = items.size(); i < size; i++) {
            if (!items[i]) { continue; }
			SafeItem safe_item = SafeItem(items[i]->item_id);
			if (safe_item.IsSalvageKit) {
				return safe_item.item_id;
			}
        }
    }
    return 0;
}
uint32_t HeroAI::GetNextUnsalvagedItem(uint32_t kit, SalvageAllType type)
{
    size_t start_slot = 0;
    auto start_bag_id = GW::Constants::Bag::Backpack;

    for (auto bag_id = start_bag_id; bag_id <= GW::Constants::Bag::Bag_2; bag_id++) {
        size_t slot = start_slot;
        start_slot = 0;
        GW::Bag* bag = GW::Items::GetBag(bag_id);
        if (!bag) {
            continue;
        }
        GW::ItemArray& items = bag->items;
        for (size_t i = slot, size = items.size(); i < size; i++) {
            if (!items[i]) { continue; }
			SafeItem safe_item = SafeItem(items[i]->item_id);

			if (safe_item.value == 0) { continue; } 
			if (!safe_item.IsSalvagable) { continue; }
			if (safe_item.equipped) { continue; }
			if (safe_item.IsRareMaterial) { continue; }
			if (safe_item.IsArmor) { continue; }
			if (safe_item.IsCustomized) { continue; }
			if (safe_item.IsRarityBlue && !safe_item.IsIdentified && (kit && SafeItem(kit).IsLesserKit)) { 
                continue; // Note: lesser kits cant salvage blue unids - Guild Wars bug/feature
            }

            if (safe_item.IsRarityGold && type <= SalvageAllType::GoldAndLower) {
				return safe_item.item_id;
            }
            else {

                if (safe_item.IsRarityPurple && type <= SalvageAllType::PurpleAndLower) {
                    return safe_item.item_id;
                }
                else {
                    if (safe_item.IsRarityBlue && type <= SalvageAllType::BlueAndLower) {
                        return safe_item.item_id;

                    }
                    else {
                        if (type <= SalvageAllType::White) {
                            return safe_item.item_id;
                        }
                    }
                }
            }			
        }
    }
    return 0;
}

void HeroAI::SalvageNext(SalvageAllType type)
{
    
}

void HeroAI::HandleMessaging()
{
    ///// REMOTE CONTROL
    MemMgr.MutexAquire();
    RemoteCommand rc = MemMgr.MemData->command[GameVars.Party.SelfPartyNumber - 1];
    MemMgr.MutexRelease();
    uint32_t npcID;
    GW::Agent* tempAgent;
    GW::AgentLiving* tempAgentLiving;


    switch (rc) {
	case rc_None:
		break;
    case rc_GetQuest:
        npcID = TargetNearestNpc();
        if (npcID) {
            tempAgent = GW::Agents::GetAgentByID(npcID);
            if (tempAgent) {
                tempAgentLiving = tempAgent->GetAsAgentLiving();
                if (tempAgentLiving) {
                    GW::Agents::InteractAgent(tempAgentLiving, false);

                    //DialogModule::SendDialog(0);
                    uint32_t FIRST_DIALOG = 0x84;
                    GW::Agents::SendDialog(FIRST_DIALOG);
                    uint32_t SECOND_DIALOG = 0x85;
                    GW::Agents::SendDialog(SECOND_DIALOG);

                }
                else { WriteChat(GW::Chat::CHANNEL_GWCA1, L"No Living", L"Hero AI"); }
            }
            else { WriteChat(GW::Chat::CHANNEL_GWCA1, L"No Agent", L"Hero AI"); }
        }
        else { WriteChat(GW::Chat::CHANNEL_GWCA1, L"No ID", L"Hero AI"); }
        break;
    case rc_Resign:
        GW::Chat::SendChat('/', "resign");
        WriteChat(GW::Chat::CHANNEL_GWCA1, L"/resign", L"Hero AI");
        break;
    case rc_Chest:
        GW::GameThread::Enqueue([] {
            GW::Items::OpenXunlaiWindow();
            });
        GW::Chat::SendChat('/', "chest");
        WriteChat(GW::Chat::CHANNEL_GWCA1, L"/chest", L"Hero AI");
        break;
    case rc_ID:
        DebugMessage(L"ID Blues");
        IdentifyAll(IdentifyAllType::Blue);
        DebugMessage(L"ID Purples");
        IdentifyAll(IdentifyAllType::Purple);
        break;
    case rc_Salvage:
        want_to_salvage = true;
        want_to_salvage_type = SalvageAllType::PurpleAndLower;
        DebugMessage(L"Salvaging PurpleAndLower");
        SalvageNext(want_to_salvage_type);

        break;
    default:
        break;
    }
    MemMgr.MutexAquire();
    MemMgr.MemData->command[GameVars.Party.SelfPartyNumber - 1] = rc_None;
    MemMgr.MutexRelease();

}

void HeroAI::activateTickOnLoad()
{
    //force Party Flag
    if (!TickOnLoad)
    {
        GW::GameThread::Enqueue([] {
            GW::PartyMgr::Tick(true);
            });
        TickOnLoad = true;
    }
}

bool HeroAI::InCastingRoutine() {
    return GameState.combat.IsSkillCasting;
}

GW::AgentID HeroAI::PartyTargetIDChanged() {
    std::vector<PlayerPartyMember> players = PyParty().players;

	const auto target = players[0].called_target_id;
    if (!target) {
        oldCalledTarget = 0;
        return 0;
    }

    if (oldCalledTarget != target) {
        oldCalledTarget = target;
        return oldCalledTarget;
    }
    return 0;
}

void HeroAI::GetPartyTarget()
{
    /****************** TARGERT MONITOR **********************/
    const auto targetChange = PartyTargetIDChanged();
    if ((GameState.state.isTargettingEnabled()) &&
        (targetChange != 0)) {
        GW::Agents::ChangeTarget(targetChange);
        GameState.combat.StayAlert.reset();
    }
    /****************** END TARGET MONITOR **********************/
}

HeroAI::FollowTarget HeroAI::GetFollowTarget(bool& FalseLeader, bool& DirectFollow)
{
    FollowTarget TargetToFollow;

    PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);
    GW::GamePos self_pos = GW::GamePos(agent_handler.x, agent_handler.y, agent_handler.z);

    TargetToFollow.Pos = self_pos;
    TargetToFollow.DistanceFromLeader = GameVars.Party.DistanceFromLeader;
	TargetToFollow.IsCasting = agent_handler.living_agent.is_casting;
	TargetToFollow.angle = agent_handler.rotation_angle;

    MemMgr.MutexAquire();
    if ((MemMgr.MemData->MemPlayers[0].IsActive) && (MemMgr.MemData->MemPlayers[0].IsFlagged) && (MemMgr.MemData->MemPlayers[0].FlagPos.x) && (MemMgr.MemData->MemPlayers[0].FlagPos.y)) {
        TargetToFollow.Pos = MemMgr.MemData->MemPlayers[0].FlagPos;
        TargetToFollow.DistanceFromLeader = GW::GetDistance(self_pos, TargetToFollow.Pos);
        TargetToFollow.IsCasting = false;
        TargetToFollow.angle = 0.0f;
        FalseLeader = true;
    }
    MemMgr.MutexRelease();

    MemMgr.MutexAquire();
    if ((MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].IsFlagged) && (MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].FlagPos.x != 0) && (MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].FlagPos.y != 0)) {
        TargetToFollow.Pos = MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].FlagPos;
        TargetToFollow.DistanceFromLeader = GW::GetDistance(self_pos, TargetToFollow.Pos);
        TargetToFollow.IsCasting = false;
        TargetToFollow.angle = 0.0f;
        FalseLeader = true;
        DirectFollow = true;
    }
    MemMgr.MutexRelease();

    return TargetToFollow;
}

float HeroAI::AngleChange() {
    PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);

    const auto angle = agent_handler.rotation_angle;
    if (!angle) {
        oldAngle = 0.0f;
        return 0.0f;
    }

    if (oldAngle != angle) {
        oldAngle = angle;
        return oldAngle;
    }
    return 0.0f;
}

float DegToRad(float degree) {
    return (degree * 3.14159 / 180);
}

// Function to convert (x, y) to (q, r) based on GridSize
GW::Vec2f HeroAI::ConvertToGridCoords(float x, float y) {
    int q = static_cast<int>(std::floor(x / GridSize));
    int r = static_cast<int>(std::floor(y / GridSize));
    return GW::Vec2f{ static_cast<float>(q), static_cast<float>(r) };
}

// Function to convert (q, r) to (x, y) based on GridSize
GW::Vec2f HeroAI::ConvertToWorldCoords(int q, int r) {
    float x = q * GridSize;
    float y = r * GridSize;
    return GW::Vec2f{ x, y };
}

bool HeroAI::CheckCollision(const GW::GamePos& position, const GW::GamePos& agentpos, float radius1, float radius2) {
    float distance = GW::GetDistance(position, agentpos);
    float radius = radius1 + radius2; // Radius of the bubble
    return distance <= radius; // Check if the distance from the center is within the bubble's radius
}

bool HeroAI::IsColliding(float x, float y) {
    const auto agents = GW::Agents::GetAgentArray();
    float AgentRadius;
    for (const GW::Agent* agent : *agents)
    {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        const auto AllyID = tAllyLiving->agent_id;
        if (AllyID == GameVars.Target.Self.ID) { continue; }
        if (tAllyLiving->GetIsDead()) { continue; }

        float AgentRadius;
        switch (tAllyLiving->allegiance) {
        case GW::Constants::Allegiance::Ally_NonAttackable:
            AgentRadius = Fields.Agent.CollisionRadius;
            break;
        case GW::Constants::Allegiance::Enemy:
            AgentRadius = Fields.Enemy.CollisionRadius;
            break;
        case GW::Constants::Allegiance::Spirit_Pet:
            AgentRadius = Fields.Spirit.CollisionRadius;
            break;
        default:
            continue;
            break;
        }

        GW::GamePos pos;
        pos.x = x;
        pos.y = y;
        if (CheckCollision(pos, tAllyLiving->pos, Fields.Agent.CollisionRadius, AgentRadius)) {
            return true;
        }

    }
    return false;
}


GW::Vec2f HeroAI::GetClosestValidCoordinate(float x, float y) {
    // Convert the initial (x, y) to grid coordinates (q, r)
    GW::Vec2f initialGridCoords = ConvertToGridCoords(x, y);
    int initialQ = static_cast<int>(initialGridCoords.x);
    int initialR = static_cast<int>(initialGridCoords.y);

    // Define the search radius and step size
    int searchRadius = 1;
    const int maxRadius = 10; // Define a maximum search radius to prevent infinite loops

    while (searchRadius <= maxRadius) {
        for (int dq = -searchRadius; dq <= searchRadius; ++dq) {
            for (int dr = -searchRadius; dr <= searchRadius; ++dr) {
                int q = initialQ + dq;
                int r = initialR + dr;

                // Convert grid coordinates (q, r) back to world coordinates (x, y)
                GW::Vec2f worldCoords = ConvertToWorldCoords(q, r);
                float worldX = worldCoords.x;
                float worldY = worldCoords.y;

                // Check if the point is valid and not colliding
                if (IsPointValid(worldX, worldY) && !IsColliding(worldX, worldY)) {
                    return GW::Vec2f{ worldX, worldY };
                }
            }
        }
        // Increase the search radius
        ++searchRadius;
    }

    // If no valid point is found within the maximum radius, return the initial point
    return GW::Vec2f{ x, y };
}

bool HeroAI::FollowLeader(FollowTarget TargetToFollow, float FollowDistanceOnCombat, bool DirectFollow, bool FalseLeader) {
    /************ FOLLOWING *********************/
    bool DistanceOutOfcombat = ((TargetToFollow.DistanceFromLeader > GW::Constants::Range::Area * 1.5) && (!GameVars.Party.InAggro));
    bool DistanceInCombat = ((TargetToFollow.DistanceFromLeader > FollowDistanceOnCombat) && (GameVars.Party.InAggro));
    bool AngleChangeOutOfCombat = ((!GameVars.Party.InAggro) && (AngleChange()));

    PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);

    if (((DistanceOutOfcombat || DistanceInCombat || AngleChangeOutOfCombat) &&
        (!agent_handler.living_agent.is_casting) &&
        (GameVars.Party.SelfPartyNumber > 1)
        )) {

        float xx = TargetToFollow.Pos.x;
        float yy = TargetToFollow.Pos.y;

        const auto heropos = (GameVars.Party.SelfPartyNumber - 1) + GW::PartyMgr::GetPartyHeroCount() + GW::PartyMgr::GetPartyHenchmanCount();
        const auto angle = TargetToFollow.angle + DegToRad(ScatterPos[heropos]);
        const auto rotcos = cos(angle);
        const auto rotsin = sin(angle);
        float posx = 0.0f;
        float posy = 0.0f;
        if (!DirectFollow) {
            posx = (GW::Constants::Range::Touch + 10) * rotcos;
            posy = (GW::Constants::Range::Touch + 10) * rotsin;
            xx = xx + posx;
            yy = yy + posy;
        }
        if (!FalseLeader) {
			PyPlayer().InteractAgent(GameVars.Party.LeaderID, false);
        }
        if (IsPointValid(xx, yy)) {
            GW::Agents::Move(xx, yy);
        }
        else {
            GW::Vec2f pos = GetClosestValidCoordinate(xx, yy);
            GW::Agents::Move(pos.x, pos.y);
        }
        return true;
    }
    return false;
    /************ END FOLLOWING *********************/
}

GW::Vec2f HeroAI::CalculateAttractionVector(const GW::Vec2f& entityPos, const GW::Vec2f& massCenter, float strength) {
    GW::Vec2f direction = massCenter - entityPos;
    float distance = GW::GetDistance(entityPos, massCenter);

    // Normalize the direction vector
    if (distance != 0) {
        direction.x /= distance;
        direction.y /= distance;
    }

    direction.x *= strength;
    direction.y *= strength;

    // Check for collisions with other agents
    const auto agents = GW::Agents::GetAgentArray();
    for (const GW::Agent* agent : *agents) {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        if (tAllyLiving->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) { continue; }

        if (tAllyLiving->agent_id == GameVars.Target.Self.ID) { continue; } // Skip self

        if (CheckCollision(entityPos, agent->pos, Fields.Agent.CollisionRadius, Fields.Agent.CollisionRadius)) {
            // Adjust the attraction vector to be perpendicular to avoid collision
            float temp = direction.x;
            direction.x = -direction.y;
            direction.y = temp;
        }
    }

    return direction;
}

GW::Vec2f HeroAI::PartyMassCenter() {
    GW::Vec2f massCenter{ 0.0f, 0.0f };
    int count = 0;

    const auto agents = GW::Agents::GetAgentArray();
    for (const GW::Agent* agent : *agents) {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        if (tAllyLiving->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) { continue; }

        massCenter.x += agent->pos.x;
        massCenter.y += agent->pos.y;
        count++;
    }

    if (count > 0) {
        massCenter.x /= count;
        massCenter.y /= count;
    }

    return massCenter;
}

GW::Vec2f HeroAI::CalculateRepulsionVector(const GW::Vec2f& entityPos) {
    GW::Vec2f repulsionVector{ 0.0f, 0.0f };

    float repulsionRadius;
    float strength;
    float combinedStrength = 0.0f;

    const auto agents = GW::Agents::GetAgentArray();
    for (const GW::Agent* agent : *agents) {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        if (tAllyLiving->GetIsDead()) { continue; }

        switch (tAllyLiving->allegiance) {
        case GW::Constants::Allegiance::Ally_NonAttackable:
            if (tAllyLiving->GetIsMoving()) { continue; }
            repulsionRadius = Fields.Agent.CollisionRadius * 2;
            strength = Fields.Agent.RepulsionStrength;
            break;
        case GW::Constants::Allegiance::Enemy:
            repulsionRadius = Fields.Enemy.CollisionRadius * 2;
            strength = Fields.Enemy.RepulsionStrength;
            break;
        case GW::Constants::Allegiance::Spirit_Pet:
            repulsionRadius = Fields.Spirit.CollisionRadius * 2;
            strength = Fields.Spirit.RepulsionStrength;
            break;
        default:
            continue;
            break;
        }


        if (tAllyLiving->agent_id == GameVars.Target.Self.ID) { continue; } // Skip self

        GW::Vec2f agentPos = agent->pos;
        GW::Vec2f direction;
        direction.x = entityPos.x - agentPos.x;
        direction.y = entityPos.y - agentPos.y;

        if (direction.x == 0 && direction.y == 0) {
            GW::Vec2f partymass = PartyMassCenter();
            direction.x = partymass.x - agentPos.x;
            direction.y = partymass.y - agentPos.y;
        }

        float distance = GW::GetDistance(entityPos, agentPos);

        if (distance < 10.0f) { distance = repulsionRadius / 2; } // Avoid division by zero by another agent on top of the entity


        if (distance < repulsionRadius) {
            // Normalize the direction vector
            direction.x /= distance;
            direction.y /= distance;

            // Apply repulsion strength inversely proportional to the distance
            float repulsionStrength = strength * (1.0f - (distance / repulsionRadius));
            direction.x *= repulsionStrength;
            direction.y *= repulsionStrength;

            repulsionVector.x += direction.x;
            repulsionVector.y += direction.y;

            combinedStrength = (combinedStrength + strength) / 2.0f;
        }
    }

    // Normalize the resulting repulsion vector and scale by the desired strength
    float magnitude = sqrt(repulsionVector.x * repulsionVector.x + repulsionVector.y * repulsionVector.y);
    if (magnitude > 0.0f) {
        repulsionVector.x /= magnitude;
        repulsionVector.y /= magnitude;

        repulsionVector.x *= strength;
        repulsionVector.y *= strength;
        //repulsionVector.x *= combinedStrength;
        //repulsionVector.y *= combinedStrength;
    }

    return repulsionVector;
}


bool HeroAI::AvoidCollision()
{
    /*************** COLLISION ****************************/

    bool oc = (GameVars.Party.SelfPartyNumber > 1) && (GameVars.Party.InAggro) ? true : false;
    bool ooc = (GameVars.Party.SelfPartyNumber > 1) && (!GameVars.Party.InAggro) ? true : false;
    PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);

    if ((!agent_handler.living_agent.is_casting) &&
        (oc || ooc) &&
        (!IsMelee(GameVars.Target.Self.ID)) &&
        (ScatterRest.getElapsedTime() > 200) &&
        IsColliding(agent_handler.x, agent_handler.y)
        ) {
        //VECTOR FIELDS

        GW::Vec2f playerPos = { agent_handler.x, agent_handler.y };
        GW::Vec2f repulsionVector = CalculateRepulsionVector(playerPos);

        GW::GamePos result_2d_repulsion_vector;
        result_2d_repulsion_vector.x = agent_handler.x + repulsionVector.x;
        result_2d_repulsion_vector.y = agent_handler.y + repulsionVector.y;

        int tryTimeout = 0;
        const int maxTryTimeout = 10;

        while (true) {
            if (IsPointValid(result_2d_repulsion_vector.x, result_2d_repulsion_vector.y)) {
                if (!IsColliding(result_2d_repulsion_vector.x, result_2d_repulsion_vector.y)) {
                    GW::Agents::Move(result_2d_repulsion_vector.x, result_2d_repulsion_vector.y);
                    break;
                }
                else {
                    repulsionVector = CalculateRepulsionVector(result_2d_repulsion_vector);
                    result_2d_repulsion_vector.x = agent_handler.x + repulsionVector.x;
                    result_2d_repulsion_vector.y = agent_handler.y + repulsionVector.y;
                    tryTimeout++;
                }
            }
            else {
                GW::Vec2f pos = GetClosestValidCoordinate(result_2d_repulsion_vector.x, result_2d_repulsion_vector.y);
                GW::Agents::Move(pos.x, pos.y);
                break;
            }

            if (tryTimeout >= maxTryTimeout) {
                GW::Vec2f pos = GetClosestValidCoordinate(result_2d_repulsion_vector.x, result_2d_repulsion_vector.y);
                GW::Agents::Move(pos.x, pos.y);
                break;
            }
        }

        ScatterRest.reset();
        return true;
    }

    return false;
    /***************** END COLLISION ************************/
}

bool HeroAI::IsSkillready(uint32_t slot) {
    if (GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::No_Skill) { return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0) { return false; }
    return true;
}

GW::AgentID HeroAI::GetAppropiateTarget(uint32_t slot) {
    GW::AgentID vTarget = 0;
    /* --------- TARGET SELECTING PER SKILL --------------*/
    if (GameState.state.isTargettingEnabled()) {
        if (GameVars.SkillBar.Skills[slot].HasCustomData) {
            bool Strict = GameVars.SkillBar.Skills[slot].CustomData.Conditions.TargettingStrict;
            Strict = true;
            switch (GameVars.SkillBar.Skills[slot].CustomData.TargetAllegiance) {
            case Enemy:
                vTarget = GameVars.Target.Called.ID;
                if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyCaster:
                vTarget = GameVars.Target.NearestCaster.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyMartial:
                vTarget = GameVars.Target.NearestMartial.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyMartialMelee:
                vTarget = GameVars.Target.NearestMartialMelee.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyMartialRanged:
                vTarget = GameVars.Target.NearestMartialRanged.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case Ally:
                vTarget = GameVars.Target.LowestAlly.ID;
                break;
            case AllyCaster:
                vTarget = GameVars.Target.LowestAllyCaster.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case AllyMartial:
                vTarget = GameVars.Target.LowestAllyMartial.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case AllyMartialMelee:
                vTarget = GameVars.Target.LowestAllyMartialMelee.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case AllyMartialRanged:
                vTarget = GameVars.Target.LowestAllyMartialRanged.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case OtherAlly:
                if ((GameVars.SkillBar.Skills[slot].HasCustomData) &&
                    (GameVars.SkillBar.Skills[slot].CustomData.Nature == EnergyBuff)) {
                    vTarget = GameVars.Target.LowestEnergyOtherAlly.ID;
                }
                else { vTarget = GameVars.Target.LowestOtherAlly.ID; }
                break;
            case Self:
                vTarget = GameVars.Target.Self.ID;
                break;
            case DeadAlly:
                vTarget = GameVars.Target.DeadAlly.ID;
                break;
            case Spirit:
                vTarget = GameVars.Target.NearestSpirit.ID;
                break;
            case Minion:
                vTarget = GameVars.Target.LowestMinion.ID;
                break;
            case Corpse:
                vTarget = GameVars.Target.NearestCorpse.ID;
                break;
            default:
                vTarget = GameVars.Target.Called.ID;
                if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
                break;
                //  enum SkillTarget { Enemy, EnemyCaster, EnemyMartial, Ally, AllyCaster, AllyMartial, OtherAlly,DeadAlly, Self, Corpse, Minion, Spirit, Pet };
            }
        }
        else {
            vTarget = GameVars.Target.Called.ID;
            if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
        }
    }
    else
    {
        vTarget = GW::Agents::GetTargetId();
    }

    if (!vTarget) { vTarget = GW::Agents::GetTargetId(); }

    return vTarget;
}

bool HeroAI::AreCastConditionsMet(uint32_t slot, GW::AgentID agentID) {

    int Skillptr = 0;
    int ptr = 0;
    int NoOfFeatures = 0;
    int featurecount = 0;

    const auto tempAgent = GW::Agents::GetAgentByID(agentID);
    if (!tempAgent) { return false; }
    const auto tempAgentLiving = tempAgent->GetAsAgentLiving();
    if (!tempAgentLiving) { return false; }

    Skillptr = CustomSkillData.GetPtrBySkillID(GameVars.SkillBar.Skills[slot].Skill.skill_id);
    if (!Skillptr) { return true; }

    if (GameVars.SkillBar.Skills[slot].CustomData.Nature == Resurrection) {
        if (tempAgentLiving->GetIsDead()) {
            //if (tempAgentLiving->allegiance == GW::Constants::Allegiance::Ally_NonAttackable) {
            return true;
            //}
        }
        else { return false; }
    }

    CustomSkillClass::CustomSkillDatatype vSkill = CustomSkillData.GetSkillByPtr(Skillptr);
    CustomSkillClass::CustomSkillDatatype vSkillSB = GameVars.SkillBar.Skills[slot].CustomData;

    PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);


    float SelfEnergy = agent_handler.living_agent.energy;
    float SelfHp = agent_handler.living_agent.hp;


    if (GameVars.SkillBar.Skills[slot].CustomData.Conditions.UniqueProperty) {
        switch (GameVars.SkillBar.Skills[slot].Skill.skill_id) {
        case GW::Constants::SkillID::Energy_Drain:
        case GW::Constants::SkillID::Energy_Tap:
        case GW::Constants::SkillID::Ether_Lord:
            return (SelfEnergy < vSkillSB.Conditions.LessEnergy) ? true : false;
            break;
        case GW::Constants::SkillID::Essence_Strike:
            return ((SelfEnergy < vSkillSB.Conditions.LessEnergy) && (GameVars.Target.NearestSpirit.ID)) ? true : false;
            break;
        case GW::Constants::SkillID::Glowing_Signet:
            return ((SelfEnergy < vSkillSB.Conditions.LessEnergy) && (tempAgentLiving->GetIsConditioned())) ? true : false;
            break;
        case GW::Constants::SkillID::Clamor_of_Souls:
            return ((agent_handler.living_agent.weapon_type == 0) && (SelfEnergy < vSkillSB.Conditions.LessEnergy)) ? true : false;
            break;
        case GW::Constants::SkillID::Waste_Not_Want_Not:
            return (!tempAgentLiving->GetIsCasting() && !tempAgentLiving->GetIsAttacking() && (SelfEnergy < vSkillSB.Conditions.LessEnergy)) ? true : false;
            break;
        case GW::Constants::SkillID::Mend_Body_and_Soul:
            return  ((tempAgentLiving->hp < vSkillSB.Conditions.LessLife) || (tempAgentLiving->GetIsConditioned())) ? true : false;
            break;
        case GW::Constants::SkillID::Grenths_Balance:
            return  ((SelfHp < tempAgentLiving->hp) && (SelfHp < vSkillSB.Conditions.LessLife)) ? true : false;
            break;
        case GW::Constants::SkillID::Deaths_Retreat:
            return  (SelfHp < tempAgentLiving->hp) ? true : false;
            break;
        case GW::Constants::SkillID::Desperate_Strike:
        case GW::Constants::SkillID::Spirit_to_Flesh:
            return  (SelfHp < vSkillSB.Conditions.LessLife) ? true : false;
            break;
        case GW::Constants::SkillID::Plague_Sending:
        case GW::Constants::SkillID::Plague_Signet:
        case GW::Constants::SkillID::Plague_Touch:
            return (agent_handler.living_agent.is_conditioned) ? true : false;
            break;
        case GW::Constants::SkillID::Golden_Fang_Strike:
        case GW::Constants::SkillID::Golden_Fox_Strike:
        case GW::Constants::SkillID::Golden_Lotus_Strike:
        case GW::Constants::SkillID::Golden_Phoenix_Strike:
        case GW::Constants::SkillID::Golden_Skull_Strike:
            return (agent_handler.living_agent.is_enchanted) ? true : false;
            break;
        case GW::Constants::SkillID::Brutal_Weapon:
            return (!tempAgentLiving->GetIsEnchanted()) ? true : false;
            break;
        case GW::Constants::SkillID::Signet_of_Removal:
            return ((!tempAgentLiving->GetIsEnchanted()) && ((tempAgentLiving->GetIsConditioned() || tempAgentLiving->GetIsHexed()))) ? true : false;
            break;
        case GW::Constants::SkillID::Dwaynas_Kiss:
        case GW::Constants::SkillID::Unnatural_Signet:
        case GW::Constants::SkillID::Toxic_Chill:
            return  ((tempAgentLiving->GetIsHexed() || tempAgentLiving->GetIsEnchanted())) ? true : false;
            break;
        case GW::Constants::SkillID::Discord:
            return (tempAgentLiving->GetIsConditioned() && (tempAgentLiving->GetIsHexed() || tempAgentLiving->GetIsEnchanted())) ? true : false;
            break;
        case GW::Constants::SkillID::Empathic_Removal:
        case GW::Constants::SkillID::Iron_Palm:
        case GW::Constants::SkillID::Melandrus_Resilience:
        case GW::Constants::SkillID::Necrosis:
        case GW::Constants::SkillID::Peace_and_Harmony: //same conditionsas below
        case GW::Constants::SkillID::Purge_Signet:
        case GW::Constants::SkillID::Resilient_Weapon:
            return (tempAgentLiving->GetIsConditioned() || tempAgentLiving->GetIsHexed()) ? true : false;
            break;
        case GW::Constants::SkillID::Gaze_from_Beyond:
        case GW::Constants::SkillID::Spirit_Burn:
        case GW::Constants::SkillID::Signet_of_Ghostly_Might:
            return (GameVars.Target.NearestSpirit.ID) ? true : false;
            break;
        default:
            return true;
        }
    }



    featurecount += (vSkill.Conditions.IsAlive) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasCondition) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasDeepWound) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasCrippled) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasBleeding) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasPoison) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasWeaponSpell) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasEnchantment) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasHex) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsCasting) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsNockedDown) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsMoving) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsAttacking) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsHoldingItem) ? 1 : 0;
    featurecount += (vSkill.Conditions.LessLife > 0.0f) ? 1 : 0;
    featurecount += (vSkill.Conditions.MoreLife > 0.0f) ? 1 : 0;
    featurecount += (vSkill.Conditions.LessEnergy > 0.0f) ? 1 : 0;

    if (tempAgentLiving->GetIsAlive() && vSkillSB.Conditions.IsAlive) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsConditioned() && vSkillSB.Conditions.HasCondition) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsDeepWounded() && vSkillSB.Conditions.HasDeepWound) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsCrippled() && vSkillSB.Conditions.HasCrippled) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsBleeding() && vSkillSB.Conditions.HasBleeding) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsPoisoned() && vSkillSB.Conditions.HasPoison) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsWeaponSpelled() && vSkillSB.Conditions.HasWeaponSpell) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsEnchanted() && vSkillSB.Conditions.HasEnchantment) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsHexed() && vSkillSB.Conditions.HasHex) { NoOfFeatures++; }

    if (tempAgentLiving->GetIsKnockedDown() && vSkillSB.Conditions.IsNockedDown) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsMoving() && vSkillSB.Conditions.IsMoving) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsAttacking() && vSkillSB.Conditions.IsAttacking) { NoOfFeatures++; }
    if (tempAgentLiving->weapon_type == 0 && vSkillSB.Conditions.IsHoldingItem) { NoOfFeatures++; }

    if ((vSkillSB.Conditions.LessLife != 0.0f) && (tempAgentLiving->hp < vSkillSB.Conditions.LessLife)) { NoOfFeatures++; }
    if ((vSkillSB.Conditions.MoreLife != 0.0f) && (tempAgentLiving->hp > vSkillSB.Conditions.MoreLife)) { NoOfFeatures++; }


    //ENERGY CHECKS
    MemMgr.MutexAquire();
    int PIndex = GetMemPosByID(agentID);

    if (PIndex >= 0) {
        if ((MemMgr.MemData->MemPlayers[PIndex].IsActive) &&
            (GameVars.SkillBar.Skills[slot].CustomData.Conditions.LessEnergy != 0.0f) &&
            (MemMgr.MemData->MemPlayers[PIndex].Energy < GameVars.SkillBar.Skills[slot].CustomData.Conditions.LessEnergy)
            ) {
            NoOfFeatures++;
        }
    }
    MemMgr.MutexRelease();



    const auto TargetSkill = GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(tempAgentLiving->skill));

    if (tempAgentLiving->GetIsCasting() && GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsCasting && TargetSkill && TargetSkill->activation > 0.250f) { NoOfFeatures++; }

    if (featurecount == NoOfFeatures) { return true; }

    return false;
}

bool HeroAI::doesSpiritBuffExists(GW::Constants::SkillID skillID) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spirit;
    static GW::AgentID SpiritID;
    static GW::AgentID tStoredSpiritID = 0;

    for (const GW::Agent* agent : *agents)
    {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tSpiritLiving = agent->GetAsAgentLiving();
        if (!tSpiritLiving) { continue; }
        if (tSpiritLiving->allegiance != GW::Constants::Allegiance::Spirit_Pet) { continue; }

        SpiritID = tSpiritLiving->agent_id;
        if (!SpiritID) { continue; }

        if (!tSpiritLiving->IsNPC()) { continue; }
        if (!tSpiritLiving->GetIsAlive()) { continue; }

        const auto SpiritModelID = tSpiritLiving->player_number;

        if ((SpiritModelID == 2882) && (skillID == GW::Constants::SkillID::Frozen_Soil)) { return true; }
        if ((SpiritModelID == 4218) && (skillID == GW::Constants::SkillID::Life)) { return true; }
        if ((SpiritModelID == 4227) && (skillID == GW::Constants::SkillID::Bloodsong)) { return true; }
        if ((SpiritModelID == 4229) && (skillID == GW::Constants::SkillID::Signet_of_Spirits)) { return true; } //anger
        if ((SpiritModelID == 4230) && (skillID == GW::Constants::SkillID::Signet_of_Spirits)) { return true; } //hate
        if ((SpiritModelID == 4231) && (skillID == GW::Constants::SkillID::Signet_of_Spirits)) { return true; } //suffering
        if ((SpiritModelID == 5720) && (skillID == GW::Constants::SkillID::Anguish)) { return true; }
        if ((SpiritModelID == 4225) && (skillID == GW::Constants::SkillID::Disenchantment)) { return true; }
        if ((SpiritModelID == 4221) && (skillID == GW::Constants::SkillID::Dissonance)) { return true; }
        if ((SpiritModelID == 4214) && (skillID == GW::Constants::SkillID::Pain)) { return true; }
        if ((SpiritModelID == 4213) && (skillID == GW::Constants::SkillID::Shadowsong)) { return true; }
        if ((SpiritModelID == 4228) && (skillID == GW::Constants::SkillID::Wanderlust)) { return true; }
        if ((SpiritModelID == 5723) && (skillID == GW::Constants::SkillID::Vampirism)) { return true; }
        if ((SpiritModelID == 5854) && (skillID == GW::Constants::SkillID::Agony)) { return true; }
        if ((SpiritModelID == 4217) && (skillID == GW::Constants::SkillID::Displacement)) { return true; }
        if ((SpiritModelID == 4222) && (skillID == GW::Constants::SkillID::Earthbind)) { return true; }
        if ((SpiritModelID == 5721) && (skillID == GW::Constants::SkillID::Empowerment)) { return true; }
        if ((SpiritModelID == 4219) && (skillID == GW::Constants::SkillID::Preservation)) { return true; }
        if ((SpiritModelID == 5719) && (skillID == GW::Constants::SkillID::Recovery)) { return true; }
        if ((SpiritModelID == 4220) && (skillID == GW::Constants::SkillID::Recuperation)) { return true; }
        if ((SpiritModelID == 5853) && (skillID == GW::Constants::SkillID::Rejuvenation)) { return true; }
        if ((SpiritModelID == 4223) && (skillID == GW::Constants::SkillID::Shelter)) { return true; }
        if ((SpiritModelID == 4216) && (skillID == GW::Constants::SkillID::Soothing)) { return true; }
        if ((SpiritModelID == 4224) && (skillID == GW::Constants::SkillID::Union)) { return true; }
        if ((SpiritModelID == 4215) && (skillID == GW::Constants::SkillID::Destruction)) { return true; }
        if ((SpiritModelID == 4226) && (skillID == GW::Constants::SkillID::Restoration)) { return true; }
        if ((SpiritModelID == 2884) && (skillID == GW::Constants::SkillID::Winds)) { return true; }
        if ((SpiritModelID == 4239) && (skillID == GW::Constants::SkillID::Brambles)) { return true; }
        if ((SpiritModelID == 4237) && (skillID == GW::Constants::SkillID::Conflagration)) { return true; }
        if ((SpiritModelID == 2885) && (skillID == GW::Constants::SkillID::Energizing_Wind)) { return true; }
        if ((SpiritModelID == 4236) && (skillID == GW::Constants::SkillID::Equinox)) { return true; }
        if ((SpiritModelID == 2876) && (skillID == GW::Constants::SkillID::Edge_of_Extinction)) { return true; }
        if ((SpiritModelID == 4238) && (skillID == GW::Constants::SkillID::Famine)) { return true; }
        if ((SpiritModelID == 2883) && (skillID == GW::Constants::SkillID::Favorable_Winds)) { return true; }
        if ((SpiritModelID == 2878) && (skillID == GW::Constants::SkillID::Fertile_Season)) { return true; }
        if ((SpiritModelID == 2877) && (skillID == GW::Constants::SkillID::Greater_Conflagration)) { return true; }
        if ((SpiritModelID == 5715) && (skillID == GW::Constants::SkillID::Infuriating_Heat)) { return true; }
        if ((SpiritModelID == 4232) && (skillID == GW::Constants::SkillID::Lacerate)) { return true; }
        if ((SpiritModelID == 2888) && (skillID == GW::Constants::SkillID::Muddy_Terrain)) { return true; }
        if ((SpiritModelID == 2887) && (skillID == GW::Constants::SkillID::Natures_Renewal)) { return true; }
        if ((SpiritModelID == 4234) && (skillID == GW::Constants::SkillID::Pestilence)) { return true; }
        if ((SpiritModelID == 2881) && (skillID == GW::Constants::SkillID::Predatory_Season)) { return true; }
        if ((SpiritModelID == 2880) && (skillID == GW::Constants::SkillID::Primal_Echoes)) { return true; }
        if ((SpiritModelID == 2886) && (skillID == GW::Constants::SkillID::Quickening_Zephyr)) { return true; }
        if ((SpiritModelID == 5718) && (skillID == GW::Constants::SkillID::Quicksand)) { return true; }
        if ((SpiritModelID == 5717) && (skillID == GW::Constants::SkillID::Roaring_Winds)) { return true; }
        if ((SpiritModelID == 2879) && (skillID == GW::Constants::SkillID::Symbiosis)) { return true; }
        if ((SpiritModelID == 5716) && (skillID == GW::Constants::SkillID::Toxicity)) { return true; }
        if ((SpiritModelID == 4235) && (skillID == GW::Constants::SkillID::Tranquility)) { return true; }
        if ((SpiritModelID == 2874) && (skillID == GW::Constants::SkillID::Winter)) { return true; }
        if ((SpiritModelID == 2875) && (skillID == GW::Constants::SkillID::Winnowing)) { return true; }
    }
    return false;
}

bool HeroAI::IsReadyToCast(uint32_t slot) {
    PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);

    if ((GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::Blood_is_Power) &&
        !(BipFixtimer.hasElapsed(2000)))
    {
        return false;
    }
    if ((GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::Blood_Ritual) &&
        !(BipFixtimer.hasElapsed(2000)))
    {
        return false;
    }
    if (agent_handler.living_agent.is_casting) { GameState.combat.IsSkillCasting = false; return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::No_Skill) { GameState.combat.IsSkillCasting = false; return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0) { GameState.combat.IsSkillCasting = false; return false; }

    const auto current_energy = static_cast<uint32_t>((agent_handler.living_agent.energy * agent_handler.living_agent.max_energy));
    if (current_energy < GameVars.SkillBar.Skills[slot].Data.energy_cost) { GameState.combat.IsSkillCasting = false; return false; }

    float hp_target = GameVars.SkillBar.Skills[slot].CustomData.Conditions.SacrificeHealth;
    float current_life = agent_handler.living_agent.hp;
    if ((current_life < hp_target) && (GameVars.SkillBar.Skills[slot].Data.health_cost > 0.0f)) { GameState.combat.IsSkillCasting = false; return false; }

    const auto enough_adrenaline =
        (GameVars.SkillBar.Skills[slot].Data.adrenaline == 0) ||
        (GameVars.SkillBar.Skills[slot].Data.adrenaline > 0 &&
            GameVars.SkillBar.Skills[slot].Skill.adrenaline_a >= GameVars.SkillBar.Skills[slot].Data.adrenaline);

    if (!enough_adrenaline) { GameState.combat.IsSkillCasting = false; return false; }

    GW::AgentID vTarget = GetAppropiateTarget(slot);
    if (!vTarget) { return false; }

    //assassin strike chains
    const auto tempAgent = GW::Agents::GetAgentByID(vTarget);
    if (!tempAgent) { return false; }
    const auto tempAgentLiving = tempAgent->GetAsAgentLiving();
    if (!tempAgentLiving) { return false; }


    if ((GameVars.SkillBar.Skills[slot].Data.combo == 1) && ((tempAgentLiving->dagger_status != 0) && (tempAgentLiving->dagger_status != 3))) { return false; }
    if ((GameVars.SkillBar.Skills[slot].Data.combo == 2) && (tempAgentLiving->dagger_status != 1)) { return false; }
    if ((GameVars.SkillBar.Skills[slot].Data.combo == 3) && (tempAgentLiving->dagger_status != 2)) { return false; }


    if ((GameVars.SkillBar.Skills[slot].HasCustomData) &&
        (!AreCastConditionsMet(slot, vTarget))) {
        GameState.combat.IsSkillCasting = false; return false;
    }
    if (doesSpiritBuffExists(GameVars.SkillBar.Skills[slot].Skill.skill_id)) { GameState.combat.IsSkillCasting = false; return false; }

    if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, vTarget)) { GameState.combat.IsSkillCasting = false; return false; }

    if ((agent_handler.living_agent.is_casting) && (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0)) { GameState.combat.IsSkillCasting = false; return false; }

    return true;
}

bool HeroAI::IsReadyToCast(uint32_t slot, GW::AgentID vTarget) {
    PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);

    if ((GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::Blood_is_Power) &&
        !(BipFixtimer.hasElapsed(2000)))
    {
        return false;
    }
    if ((GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::Blood_Ritual) &&
        !(BipFixtimer.hasElapsed(2000)))
    {
        return false;
    }
    if (agent_handler.living_agent.is_casting) { GameState.combat.IsSkillCasting = false; return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::No_Skill) { GameState.combat.IsSkillCasting = false; return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0) { GameState.combat.IsSkillCasting = false; return false; }

    const auto current_energy = static_cast<uint32_t>((agent_handler.living_agent.energy * agent_handler.living_agent.max_energy));
    if (current_energy < GameVars.SkillBar.Skills[slot].Data.energy_cost) { GameState.combat.IsSkillCasting = false; return false; }

    float hp_target = GameVars.SkillBar.Skills[slot].CustomData.Conditions.SacrificeHealth;
    float current_life = agent_handler.living_agent.hp;
    if ((current_life < hp_target) && (GameVars.SkillBar.Skills[slot].Data.health_cost > 0.0f)) { GameState.combat.IsSkillCasting = false; return false; }

    const auto enough_adrenaline =
        (GameVars.SkillBar.Skills[slot].Data.adrenaline == 0) ||
        (GameVars.SkillBar.Skills[slot].Data.adrenaline > 0 &&
            GameVars.SkillBar.Skills[slot].Skill.adrenaline_a >= GameVars.SkillBar.Skills[slot].Data.adrenaline);

    if (!enough_adrenaline) { GameState.combat.IsSkillCasting = false; return false; }

    //GW::AgentID vTarget = GetAppropiateTarget(slot);
    //if (!vTarget) { return false; }

    //assassin strike chains
    const auto tempAgent = GW::Agents::GetAgentByID(vTarget);
    if (!tempAgent) { return false; }
    const auto tempAgentLiving = tempAgent->GetAsAgentLiving();
    if (!tempAgentLiving) { return false; }


    if ((GameVars.SkillBar.Skills[slot].Data.combo == 1) && ((tempAgentLiving->dagger_status != 0) && (tempAgentLiving->dagger_status != 3))) { return false; }
    if ((GameVars.SkillBar.Skills[slot].Data.combo == 2) && (tempAgentLiving->dagger_status != 1)) { return false; }
    if ((GameVars.SkillBar.Skills[slot].Data.combo == 3) && (tempAgentLiving->dagger_status != 2)) { return false; }


    if ((GameVars.SkillBar.Skills[slot].HasCustomData) &&
        (!AreCastConditionsMet(slot, vTarget))) {
        GameState.combat.IsSkillCasting = false; return false;
    }
    if (doesSpiritBuffExists(GameVars.SkillBar.Skills[slot].Skill.skill_id)) { GameState.combat.IsSkillCasting = false; return false; }

    if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, vTarget)) { GameState.combat.IsSkillCasting = false; return false; }

    if ((agent_handler.living_agent.is_casting) && (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0)) { GameState.combat.IsSkillCasting = false; return false; }


    return true;
}

bool HeroAI::IsOOCSkill(uint32_t slot) {

    if (GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsOutOfCombat) { return true; }

    switch (GameVars.SkillBar.Skills[slot].CustomData.SkillType) {
    case GW::Constants::SkillType::Form:
    case GW::Constants::SkillType::Preparation:
        return true;
        break;
    }

    switch (GameVars.SkillBar.Skills[slot].Data.type) {
    case GW::Constants::SkillType::Form:
    case GW::Constants::SkillType::Preparation:
        return true;
        break;
    }

    switch (GameVars.SkillBar.Skills[slot].CustomData.Nature) {
    case Healing:
    case Hex_Removal:
    case Condi_Cleanse:
    case EnergyBuff:
    case Resurrection:
        return true;
        break;
    default:
        return false;
    }

    return false;
}

void HeroAI::ChooseTarget() {
    PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);
    PyPlayer player_handle = PyPlayer();

    if (GameState.state.isTargettingEnabled()) {
        const auto target_id = PyPlayer().target_id;

        if (!target_id) {
            if (GameVars.Target.Called.ID) {
                player_handle.ChangeTarget(GameVars.Target.Called.ID);
                if (agent_handler.living_agent.weapon_type != 0) {
                    player_handle.InteractAgent(GameVars.Target.Called.ID, false);
                }
            }
            else {
                if (GameVars.Target.Nearest.ID) {
                    player_handle.ChangeTarget(GameVars.Target.Nearest.ID);
                    if (agent_handler.living_agent.weapon_type != 0) {
                        player_handle.InteractAgent(GameVars.Target.Nearest.ID, false);
                    }
                }
            }
        }
    }
}

bool HeroAI::CastSkill(uint32_t slot) {
    GW::AgentID vTarget = 0;

    vTarget = GetAppropiateTarget(slot);
    if (!vTarget) { return false; } //because of forced targetting

    if (IsReadyToCast(slot, vTarget))
    {
        GameState.combat.IntervalSkillCasting = GameVars.SkillBar.Skills[slot].Data.activation + GameVars.SkillBar.Skills[slot].Data.aftercast + 750.0f;
        GameState.combat.LastActivity.reset();

        GW::SkillbarMgr::UseSkill(slot, vTarget);
        if ((GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::Blood_is_Power) ||
            (GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::Blood_Ritual))

        {
            BipFixtimer.reset();
        }

        ChooseTarget();
        return true;
    }
    return false;
}

void HeroAI::HandleOOCombat()
{
    /****************** OUT OF COMBAT ROUTINES *********************************/
    if (GameVars.SkillBar.Skillptr >= 8) {
        GameVars.SkillBar.Skillptr = 0;
        GW::Agents::ChangeTarget(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID));
    }
    switch (GameState.combat.State) {
    case cIdle:
        if (GameState.combat.State != cInCastingRoutine) {
            if ((GameState.CombatSkillState[GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr]]) &&
                (IsSkillready(GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr])) &&
                (!InCastingRoutine()) &&
                (IsReadyToCast(GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr])) &&
                (IsOOCSkill(GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr]))
                )
            {
                GameState.combat.State = cInCastingRoutine;
                GameState.combat.currentCastingSlot = GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr];
                GameState.combat.LastActivity.reset();
                GameState.combat.IntervalSkillCasting = 750.0f + GetPing();
                GameState.combat.IsSkillCasting = true;

                if (CastSkill(GameState.combat.currentCastingSlot))
                {
                    GameVars.SkillBar.Skillptr = 0; ChooseTarget();
                }
                else {
                    GameVars.SkillBar.Skillptr++;
                    GW::Agents::ChangeTarget(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID));
                }
                break;
            }
            else {
                GameVars.SkillBar.Skillptr++;
                GW::Agents::ChangeTarget(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID));
            }
        }
        break;
    case cInCastingRoutine:

        if (InCastingRoutine()) { break; }
        GameState.combat.State = cIdle;

        GW::Agents::ChangeTarget(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID));
        break;
    default:
        GameState.combat.State = cIdle;
    }
    /****************** END OUT OF COMBAT ROUTINES *********************************/
}


void HeroAI::HandleCombat()
{
    PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);
    /******************COMBAT ROUTINES *********************************/
    if (GameVars.SkillBar.Skillptr >= 8) {
        GameVars.SkillBar.Skillptr = 0;
        if (agent_handler.living_agent.weapon_type != 0) {
            GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID), false);
        }
    }
    switch (GameState.combat.State) {
    case cIdle:
        if (GameState.combat.State != cInCastingRoutine) {
            if ((GameState.CombatSkillState[GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr]]) &&
                (IsSkillready(GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr])) &&
                (!InCastingRoutine()) &&
                (IsReadyToCast(GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr])))
            {
                GameState.combat.State = cInCastingRoutine;
                GameState.combat.currentCastingSlot = GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr];
                GameState.combat.LastActivity.reset();
                GameState.combat.IntervalSkillCasting = 750.0f + GetPing();
                GameState.combat.IsSkillCasting = true;

                if (agent_handler.living_agent.weapon_type != 0) {
                    GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID), false);
                }
                if (CastSkill(GameState.combat.currentCastingSlot)) { GameVars.SkillBar.Skillptr = 0; ChooseTarget(); }
                else {
                    if (agent_handler.living_agent.weapon_type != 0) {
                        GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID), false);
                    }
                    GameVars.SkillBar.Skillptr++;
                }
                break;
            }
            else {
                GameVars.SkillBar.Skillptr++;
                GW::Agents::ChangeTarget(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID));
            }
        }
        break;
    case cInCastingRoutine:

        if (InCastingRoutine()) { break; }
        GameState.combat.State = cIdle;

        if (agent_handler.living_agent.weapon_type != 0) {
            GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID), false);
        }
        break;
    default:
        GameState.combat.State = cIdle;
    }

    /******************COMBAT ROUTINES *********************************/

}


void HeroAI::Update() {
    bool GameVarsUpdated = false;

    GameVarsUpdated = UpdateGameVars();

	if (!GameVarsUpdated) { return; }

    

    if (GameVars.Map.IsLoading) { TickOnLoad = false; } //map loading, not good for anything but reset vars

    
    if ((GameVars.Map.IsMapReady) && (GameVars.Map.IsPartyLoaded)) {

        try {
            HandleMessaging();
        }
        catch (const std::exception& e) {
            std::string errorMsg = e.what();
            std::wstring errorMsgW(errorMsg.begin(), errorMsg.end());

            auto msg = "Error Handling Multibox Messaging: " + errorMsg;
            std::wstring msgWStr = std::wstring(msg.begin(), msg.end());
            DebugMessage(msgWStr.c_str());
            return;
        }
        catch (...) {
            DebugMessage(L"Unknown error Handling Multibox Messaging.");
            return;
        }

        if (GameVars.Map.IsOutpost) {
            activateTickOnLoad();
        }
    }

    if (!GameVarsUpdated) { return; };
    
    if (!GameVars.Map.IsMapReady) { return; }
    if (!GameVars.Map.IsPartyLoaded) { return; }
    if (!GameVars.Map.IsExplorable) { return; }
    PyAgent agent_handler = PyAgent(GameVars.Target.Self.ID);
    if (!agent_handler.living_agent.is_alive) { return; }
    if (agent_handler.living_agent.is_knocked_down) { return; }
    if (InCastingRoutine()) { return; }

    GetPartyTarget();

    // ENHANCE SCANNING AREA ON COMBAT ACTIVITY 
    if (agent_handler.living_agent.is_attacking || agent_handler.living_agent.is_casting)
    {
        GameState.combat.StayAlert.reset();
    }

    if (GameState.state.isCombatEnabled() && (!GameVars.Party.InAggro))
    {
        try {
            HandleOOCombat();
        }
        catch (const std::exception& e) {
            std::string errorMsg = e.what();
            std::wstring errorMsgW(errorMsg.begin(), errorMsg.end());

            auto msg = "Error in OOC routine: " + errorMsg;
            std::wstring msgWStr = std::wstring(msg.begin(), msg.end());
            DebugMessage(msgWStr.c_str());
            return;
        }
        catch (...) {
            DebugMessage(L"Unknown error in OOC routine.");
            return;
        }
    }

    if (agent_handler.living_agent.is_moving) { return; }

    if (GameState.state.isLootingEnabled() && (!GameVars.Party.InAggro))
    {
        try {
            //**************** LOOTING *****************
            uint32_t tItem;
            tItem = TargetNearestItem();

            if ((ThereIsSpaceInInventory()) && (tItem != 0)) {
                GW::Agents::InteractAgent(GW::Agents::GetAgentByID(tItem), false);
                return;
            }
            //**************** END LOOTING *************
        }
        catch (const std::exception& e) {
            std::string errorMsg = e.what();
            std::wstring errorMsgW(errorMsg.begin(), errorMsg.end());

            auto msg = "Error in Looting routines: " + errorMsg;
            std::wstring msgWStr = std::wstring(msg.begin(), msg.end());
            DebugMessage(msgWStr.c_str());
            return;
        }
        catch (...) {
            DebugMessage(L"Unknown error in Looting routines.");
            return;
        }
    }

    bool FalseLeader = false;
    bool DirectFollow = false;

    FollowTarget TargetToFollow = GetFollowTarget(FalseLeader, DirectFollow);

    float FollowDistanceOnCombat;
    if (IsMelee(GameVars.Target.Self.ID)) { FollowDistanceOnCombat = MeleeRangeValue; }
    else { FollowDistanceOnCombat = RangedRangeValue; }

    if (GameState.state.isFollowingEnabled()) {
        try {
            if (FollowLeader(TargetToFollow, FollowDistanceOnCombat, DirectFollow, FalseLeader)) { return; }
        }
        catch (const std::exception& e) {
            std::string errorMsg = e.what();
            std::wstring errorMsgW(errorMsg.begin(), errorMsg.end());

            auto msg = "Error Following Leader: " + errorMsg;
            std::wstring msgWStr = std::wstring(msg.begin(), msg.end());
            DebugMessage(msgWStr.c_str());
            return;
        }
        catch (...) {
            DebugMessage(L"Unknown error Following Leader.");
            return;
        }
    }

    if (GameState.state.isCollisionEnabled()) {

        try {
            if (AvoidCollision()) { return; }
        }
        catch (const std::exception& e) {
            std::string errorMsg = e.what();
            std::wstring errorMsgW(errorMsg.begin(), errorMsg.end());

            auto msg = "Error in Collision routines: " + errorMsg;
            std::wstring msgWStr = std::wstring(msg.begin(), msg.end());
            DebugMessage(msgWStr.c_str());
            return;
        }
        catch (...) {
            DebugMessage(L"Unknown error in collision routines.");
            return;
        }
    }

    if (GameState.state.isCombatEnabled() && (GameVars.Party.InAggro))
    {
        try {
            HandleCombat();

        }
        catch (const std::exception& e) {
            std::string errorMsg = e.what();
            std::wstring errorMsgW(errorMsg.begin(), errorMsg.end());

            auto msg = "Error in Combat routine: " + errorMsg;
            std::wstring msgWStr = std::wstring(msg.begin(), msg.end());
            DebugMessage(msgWStr.c_str());
            return;
        }
        catch (...) {
            DebugMessage(L"Unknown error in combat routine.");
            return;
        }
    }
    
}

bool HeroAI::IsActive(int party_pos)
{
    if (party_pos < 1 || party_pos > 8)
        return false;
    return PythonVars[party_pos - 1].is_active;
}

void HeroAI::SetLooting(int party_pos, bool state)
{
    if (party_pos == 0) {
        ControlAll.state.Looting = state;
        for (int i = 0; i < 8; i++)
        {
            PythonVars[i].Looting.command_issued = true;
            PythonVars[i].Looting.bool_parameter = state;
        }
        return;
    }

    if (party_pos < 1 || party_pos > 8)
        return;

    PythonVars[party_pos - 1].Looting.command_issued = true;
    PythonVars[party_pos - 1].Looting.bool_parameter = state;
}

bool HeroAI::GetLooting(int party_pos)
{
    if (party_pos == 0)
        return ControlAll.state.Looting;

    if (party_pos < 1 || party_pos > 8)
        return false;
    return PythonVars[party_pos - 1].Looting.value;
}

void HeroAI::SetFollowing(int party_pos, bool state)
{
    if (party_pos == 0) {
        ControlAll.state.Following = state;
        for (int i = 0; i < 8; i++)
        {
            PythonVars[i].Following.command_issued = true;
            PythonVars[i].Following.bool_parameter = state;
        }
        return;
    }

    if (party_pos < 1 || party_pos > 8)
        return;

    PythonVars[party_pos - 1].Following.command_issued = true;
    PythonVars[party_pos - 1].Following.bool_parameter = state;
}

bool HeroAI::GetFollowing(int party_pos)
{
    if (party_pos == 0)
        return ControlAll.state.Following;

    if (party_pos < 1 || party_pos > 8)
        return false;

    return PythonVars[party_pos - 1].Following.value;
}

void HeroAI::SetCombat(int party_pos, bool state)
{
    if (party_pos == 0) {
        ControlAll.state.Combat = state;
        for (int i = 0; i < 8; i++)
        {
            PythonVars[i].Combat.command_issued = true;
            PythonVars[i].Combat.bool_parameter = state;
        }
        return;
    }

    if (party_pos < 1 || party_pos > 8)
        return;

    PythonVars[party_pos - 1].Combat.command_issued = true;
    PythonVars[party_pos - 1].Combat.bool_parameter = state;
}

bool HeroAI::GetCombat(int party_pos)
{
    if (party_pos == 0)
        return ControlAll.state.Combat;

    if (party_pos < 1 || party_pos > 8)
        return false;
    return PythonVars[party_pos - 1].Combat.value;
}

void HeroAI::SetCollision(int party_pos, bool state)
{
    if (party_pos == 0) {
        ControlAll.state.Collision = state;
        for (int i = 0; i < 8; i++)
        {
            PythonVars[i].Collision.command_issued = true;
            PythonVars[i].Collision.bool_parameter = state;
        }
        return;
    }

    if (party_pos < 1 || party_pos > 8)
        return;

    PythonVars[party_pos - 1].Collision.command_issued = true;
    PythonVars[party_pos - 1].Collision.bool_parameter = state;
}

bool HeroAI::GetCollision(int party_pos)
{
    if (party_pos == 0)
        return ControlAll.state.Collision;

    if (party_pos < 1 || party_pos > 8)
        return false;
    return PythonVars[party_pos - 1].Collision.value;
}

void HeroAI::SetTargetting(int party_pos, bool state)
{
    if (party_pos == 0) {
        ControlAll.state.Targetting = state;
        for (int i = 0; i < 8; i++)
        {
            PythonVars[i].Targetting.command_issued = true;
            PythonVars[i].Targetting.bool_parameter = state;
        }
        return;
    }

    if (party_pos < 1 || party_pos > 8)
        return;

    PythonVars[party_pos - 1].Targetting.command_issued = true;
    PythonVars[party_pos - 1].Targetting.bool_parameter = state;
}

bool HeroAI::GetTargetting(int party_pos)
{
    if (party_pos == 0)
        return ControlAll.state.Targetting;

    if (party_pos < 1 || party_pos > 8)
        return false;
    return PythonVars[party_pos - 1].Targetting.value;
}

void HeroAI::SetSkill(int party_pos, int skill_pos, bool state)
{
    if (party_pos == 0) {
        ControlAll.CombatSkillState[skill_pos - 1] = state;

        for (int i = 0; i < 8; i++)
        {
            PythonVars[i].Skills[skill_pos - 1].command_issued = true;
            PythonVars[i].Skills[skill_pos - 1].bool_parameter = state;
        }
        return;
    }

    if (party_pos < 1 || party_pos > 8)
        return;

    if (skill_pos < 1 || skill_pos > 8)
        return;

    PythonVars[party_pos - 1].Skills[skill_pos - 1].command_issued = true;
    PythonVars[party_pos - 1].Skills[skill_pos - 1].bool_parameter = state;
}

bool HeroAI::GetSkill(int party_pos, int skill_pos)
{
    if (party_pos == 0)
        return ControlAll.CombatSkillState[skill_pos - 1];

    if (party_pos < 1 || party_pos > 8)
        return false;

    if (skill_pos < 1 || skill_pos > 8)
        return false;

    return PythonVars[party_pos - 1].Skills[skill_pos - 1].value;
}

void HeroAI::FlagAIHero(int party_pos, float x, float y)
{
    if (party_pos < 1 || party_pos > 8)
        return;

    PythonVars[party_pos-1].Flagging.command_issued = true;
    PythonVars[party_pos-1].Flagging.parameter_3 = x;
    PythonVars[party_pos-1].Flagging.parameter_4 = y;

}

void HeroAI::UnFlagAIHero(int party_pos)
{
    if (party_pos < 1 || party_pos > 8)
        return;

    PythonVars[party_pos-1].UnFlagging.command_issued = true;
}

float HeroAI::GetEnergy(int party_pos)
{
    if (party_pos < 1 || party_pos > 8)
        return 0.0f;

    return PythonVars[party_pos - 1].energy;
}

float HeroAI::GetEnergyRegen(int party_pos)
{
    if (party_pos < 1 || party_pos > 8)
        return 0.0f;

    return PythonVars[party_pos - 1].energy_regen;
}

int HeroAI::GetPythonAgentID(int party_pos)
{
    if (party_pos < 1 || party_pos > 8)
        return 0;

    return PythonVars[party_pos - 1].agent_id;
}

void HeroAI::Resign(int party_pos)
{
    if (party_pos == 0) {
        for (int i = 0; i < 8; i++) {
            PythonVars[i].Resign.command_issued = true;
        }
        return;
    }

    if (party_pos < 1 || party_pos > 8)
        return;

    PythonVars[party_pos - 1].Resign.command_issued = true;
}

void HeroAI::TakeQuest(uint32_t party_pos, uint32_t dialog)
{
    if (party_pos == 0) {
        for (int i = 0; i < 8; i++) {
            PythonVars[i].TakeQuest.command_issued = true;
        }
        return;
    }

    if (party_pos < 1 || party_pos > 8)
        return;

    PythonVars[party_pos - 1].TakeQuest.command_issued = true;
}

void HeroAI::Identify_cmd(int party_pos)
{
    if (party_pos == 0) {
        for (int i = 0; i < 8; i++) {
            PythonVars[i].Identify.command_issued = true;
        }
        return;
    }

    if (party_pos < 1 || party_pos > 8)
        return;

    PythonVars[party_pos - 1].Identify.command_issued = true;
}

void HeroAI::Salvage_cmd(int party_pos)
{
    if (party_pos == 0) {
        for (int i = 0; i < 8; i++) {
            PythonVars[i].Salvage.command_issued = true;
        }
        return;
    }

    if (party_pos < 1 || party_pos > 8)
        return;

    PythonVars[party_pos - 1].Salvage.command_issued = true;
}

void HeroAI::SetAIEnabled(bool state){ 
    AI_enabled = state; 
	if (AI_enabled) { DebugMessage(L"HeroAI Enabled"); }
	else { DebugMessage(L"HeroAI Disabled"); }

}

int HeroAI::GetMyPartyPos()
{
    return GameVars.Party.SelfPartyNumber;
}

namespace py = pybind11;

