#pragma once
#include "Headers.h"

namespace py = pybind11;

Profession::Profession(int prof) : profession(static_cast<ProfessionType>(prof)) 
{ profession = static_cast<ProfessionType>(prof); }

Profession::Profession(std::string prof) {
	if (prof == "Warrior") {
		profession = ProfessionType::Warrior;
	}
	else if (prof == "Ranger") {
		profession = ProfessionType::Ranger;
	}
	else if (prof == "Monk") {
		profession = ProfessionType::Monk;
	}
	else if (prof == "Necromancer") {
		profession = ProfessionType::Necromancer;
	}
	else if (prof == "Mesmer") {
		profession = ProfessionType::Mesmer;
	}
	else if (prof == "Elementalist") {
		profession = ProfessionType::Elementalist;
	}
	else if (prof == "Assassin") {
		profession = ProfessionType::Assassin;
	}
	else if (prof == "Ritualist") {
		profession = ProfessionType::Ritualist;
	}
	else if (prof == "Paragon") {
		profession = ProfessionType::Paragon;
	}
	else if (prof == "Dervish") {
		profession = ProfessionType::Dervish;
	}
	else {
		profession = ProfessionType::None;
	}
}

// Setter
void Profession::Set(int prof) {
    profession = static_cast<ProfessionType>(prof);
}

// Getter
ProfessionType Profession::Get() const {
    return profession;
}


// Convert to int
int Profession::ToInt() const {
    return static_cast<int>(profession);
}

// Get name as a string
std::string Profession::GetName() const {
    switch (profession) {
    case ProfessionType::None:
        return "None";
    case ProfessionType::Warrior:
        return "Warrior";
    case ProfessionType::Ranger:
        return "Ranger";
    case ProfessionType::Monk:
        return "Monk";
    case ProfessionType::Necromancer:
        return "Necromancer";
    case ProfessionType::Mesmer:
        return "Mesmer";
    case ProfessionType::Elementalist:
        return "Elementalist";
    case ProfessionType::Assassin:
        return "Assassin";
    case ProfessionType::Ritualist:
        return "Ritualist";
    case ProfessionType::Paragon:
        return "Paragon";
    case ProfessionType::Dervish:
        return "Dervish";
    default:
        return "Unknown";
    }
}

// Get short name
std::string Profession::GetShortName() const {
    switch (profession) {
    case ProfessionType::None:
        return "NA";
    case ProfessionType::Warrior:
        return "W";
    case ProfessionType::Ranger:
        return "R";
    case ProfessionType::Monk:
        return "Mo";
    case ProfessionType::Necromancer:
        return "N";
    case ProfessionType::Mesmer:
        return "Me";
    case ProfessionType::Elementalist:
        return "E";
    case ProfessionType::Assassin:
        return "A";
    case ProfessionType::Ritualist:
        return "Rt";
    case ProfessionType::Paragon:
        return "P";
    case ProfessionType::Dervish:
        return "D";
    default:
        return "NA";
    }
}


Allegiance::Allegiance(int value) : allegiance(static_cast<AllegianceType>(value)) {}

// Set allegiance
void Allegiance::Set(int value) {
    allegiance = static_cast<AllegianceType>(value);
}

// Get allegiance
AllegianceType Allegiance::Get() const {
    return allegiance;
}

// Convert to int
int Allegiance::ToInt() const {
    return static_cast<int>(allegiance);
}

// Get name as a string
std::string Allegiance::GetName() const {
    switch (allegiance) {
    case AllegianceType::Ally:
        return "Ally";
    case AllegianceType::Neutral:
        return "Neutral";
    case AllegianceType::Enemy:
        return "Enemy";
    case AllegianceType::SpiritPet:
        return "Spirit/Pet";
    case AllegianceType::Minion:
        return "Minion";
    case AllegianceType::NpcMinipet:
        return "NPC/Minipet";
    default:
        return "Unknown";
    }
}

Weapon::Weapon(int value) : weapon_type(static_cast<PyWeaponType>(value)) {}

// Set weapon type
void Weapon::Set(int value) {
    weapon_type = static_cast<PyWeaponType>(value);
}

// Get weapon type
PyWeaponType Weapon::Get() const {
    return weapon_type;
}

// Convert to int
int Weapon::ToInt() const {
    return static_cast<int>(weapon_type);
}

// Get name as a string
std::string Weapon::GetName() const {
    switch (weapon_type) {
    case PyWeaponType::Bow:
        return "Bow";
    case PyWeaponType::Axe:
        return "Axe";
    case PyWeaponType::Hammer:
        return "Hammer";
    case PyWeaponType::Daggers:
        return "Daggers";
    case PyWeaponType::Scythe:
        return "Scythe";
    case PyWeaponType::Spear:
        return "Spear";
    case PyWeaponType::Sword:
        return "Sword";
    case PyWeaponType::Wand:
        return "Wand";
    case PyWeaponType::Scepter:
    case PyWeaponType::Scepter2:
        return "Scepter";
    case PyWeaponType::Staff:
    case PyWeaponType::Staff1:
    case PyWeaponType::Staff2:
    case PyWeaponType::Staff3:
        return "Staff";
    default:
        return "Unknown";
    }
}


PyLivingAgent::PyLivingAgent(int agent_id) : agent_id(agent_id) {
    GetContext();
}


void PyLivingAgent::GetContext() {
    if (!agent_id) return;
    GW::Agent* temp_agent = GW::Agents::GetAgentByID(agent_id);
    if (!temp_agent) return;
    if (!temp_agent->GetIsLivingType()) return;

    GW::AgentLiving* living = temp_agent->GetAsAgentLiving();
    if (!living) return;

    owner_id = living->owner;
    player_number = living->player_number;
    profession = Profession(static_cast<int>(living->primary));
    secondary_profession = Profession(static_cast<int>(living->secondary));
    level = living->level;
    energy = living->energy;
    max_energy = living->max_energy;
    energy_regen = living->energy_regen;
    hp = living->hp;
    max_hp = living->max_hp;
    hp_regen = living->hp_pips;
    login_number = living->login_number;

    /*
    if (login_number) {
        wchar_t* p_name = GW::Agents::GetPlayerNameByLoginNumber(login_number);
        std::wstring wide_name(p_name);
        name = std::string(wide_name.begin(), wide_name.end());
    }
    
    else {
		name = "";
        std::wstring wide_name;
		GW::Agents::AsyncGetAgentName(temp_agent, wide_name);
        if (!wide_name.empty()) {
            char buffer[512];
            const std::string agent_name_str = local_WStringToString(wide_name);
			name = agent_name_str;
        }

    }
    */

    dagger_status = living->dagger_status;
    allegiance = Allegiance(static_cast<int>(living->allegiance));

    weapon_type = Weapon(static_cast<int>(living->weapon_type));
    weapon_item_type = living->weapon_item_type;
    offhand_item_type = living->offhand_item_type;
    weapon_item_id = living->weapon_item_id;
    offhand_item_id = living->offhand_item_id;

    is_bleeding = living->GetIsBleeding();
    is_conditioned = living->GetIsConditioned();
    is_crippled = living->GetIsCrippled();
    is_dead = living->GetIsDead();
    is_deep_wounded = living->GetIsDeepWounded();
    is_poisoned = living->GetIsPoisoned();
    is_enchanted = living->GetIsEnchanted();
    is_degen_hexed = living->GetIsDegenHexed();
    is_hexed = living->GetIsHexed();
    is_weapon_spelled = living->GetIsWeaponSpelled();
    in_combat_stance = living->GetInCombatStance();
    has_quest = living->GetHasQuest();
    is_dead_by_typemap = living->GetIsDeadByTypeMap();
    is_female = living->GetIsFemale();
    has_boss_glow = living->GetHasBossGlow();
    is_hiding_cape = living->GetIsHidingCape();
    can_be_viewed_in_party_window = living->GetCanBeViewedInPartyWindow();
    is_spawned = living->GetIsSpawned();
    is_being_observed = living->GetIsBeingObserved();
    is_knocked_down = living->GetIsKnockedDown();
    is_moving = living->GetIsMoving();
    is_attacking = living->GetIsAttacking();
    is_casting = living->GetIsCasting();
    is_idle = living->GetIsIdle();
    is_alive = living->GetIsAlive();
    is_player = living->IsPlayer();
    is_npc = living->IsNPC();
    casting_skill_id = living->skill;
    overcast = living->h0118;
}

std::string global_agent_name;
bool name_ready = false;

std::string local_WStringToString(const std::wstring& s)
{
    // @Cleanup: ASSERT used incorrectly here; value passed could be from anywhere!
    if (s.empty()) {
        return "Error In Wstring";
    }
    // NB: GW uses code page 0 (CP_ACP)
    const auto size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), strTo.data(), size_needed, NULL, NULL);
    return strTo;
}



// Struct to store agent name request status
struct AgentNameData {
    std::string agent_name;
    bool name_ready = false;
};

// Global map for storing multiple agent name requests
static std::unordered_map<uint32_t, AgentNameData> agent_name_map;

void PyLivingAgent::RequestName() {
    const auto agentid = agent_id;
    agent_name_map[agentid].name_ready = false;  // Reset flag for this agent
	agent_name_map[agentid].agent_name = "";  // Reset name

    std::thread([agentid]() {
        std::wstring temp_name;
        auto start_time = std::chrono::steady_clock::now();

        GW::GameThread::Enqueue([agentid, &temp_name]() {
            GW::Agents::AsyncGetAgentName(GW::Agents::GetAgentByID(agentid), temp_name);
            });

        while (temp_name.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() >= 500) {
                agent_name_map[agentid].agent_name = "Timeout";
                agent_name_map[agentid].name_ready = true;
                return;  // Timeout without modifying anything
            }
        }

        // Store the name inside the global map
        agent_name_map[agentid].agent_name = local_WStringToString(temp_name);
        agent_name_map[agentid].name_ready = true;  // Mark as ready

        }).detach();  // Fully detach the thread so it does not block anything
}

bool PyLivingAgent::IsAgentNameReady() {
    return agent_name_map[agent_id].name_ready;
}

std::string PyLivingAgent::GetName() {
    return agent_name_map[agent_id].agent_name;
}



PyItemAgent::PyItemAgent(int agent_id) : agent_id(agent_id) {
    GetContext();
}

void PyItemAgent::GetContext() {
    if (!agent_id) return;

    GW::Agent* temp_agent = GW::Agents::GetAgentByID(agent_id);
    if (!temp_agent) return;
    if (!temp_agent->GetIsItemType()) return;

    auto item = temp_agent->GetAsAgentItem();
    if (!item) return;

    item_id = static_cast<int>(item->item_id);
    owner_id = static_cast<int>(item->owner);
    h00CC = item->h00CC;
    extra_type = static_cast<int>(item->extra_type);
}


PyGadgetAgent::PyGadgetAgent(int agent_id) : agent_id(agent_id) {
    GetContext();
}

void PyGadgetAgent::GetContext() {
    if (!agent_id) return;

    GW::Agent* temp_agent = GW::Agents::GetAgentByID(agent_id);
    if (!temp_agent) return;
    if (!temp_agent->GetIsGadgetType()) return;

    GW::AgentGadget* gadget = temp_agent->GetAsAgentGadget();
    if (!gadget) return;

    gadget_id = static_cast<int>(gadget->gadget_id);
    h00C4 = gadget->h00C4;
    h00C8 = gadget->h00C8;
    extra_type = static_cast<int>(gadget->extra_type);
    // agent_ids.push_back(static_cast<int>(ally_id));
    h00D4.push_back(gadget->h00D4[0]);
    h00D4.push_back(gadget->h00D4[1]);
    h00D4.push_back(gadget->h00D4[2]);
    h00D4.push_back(gadget->h00D4[3]);
}

AttributeClass::AttributeClass(std::string attr_name) {
    if (attr_name == "Fast Casting") { attribute_id = SafeAttribute::FastCasting; }
    else if (attr_name == "Illusion Magic") { attribute_id = SafeAttribute::IllusionMagic; }
    else if (attr_name == "Domination Magic") { attribute_id = SafeAttribute::DominationMagic; }
    else if (attr_name == "Inspiration Magic") { attribute_id = SafeAttribute::InspirationMagic; }
    else if (attr_name == "Blood Magic") { attribute_id = SafeAttribute::BloodMagic; }
    else if (attr_name == "Death Magic") { attribute_id = SafeAttribute::DeathMagic; }
    else if (attr_name == "Soul Reaping") { attribute_id = SafeAttribute::SoulReaping; }
    else if (attr_name == "Curses") { attribute_id = SafeAttribute::Curses; }
    else if (attr_name == "Air Magic") { attribute_id = SafeAttribute::AirMagic; }
    else if (attr_name == "Earth Magic") { attribute_id = SafeAttribute::EarthMagic; }
    else if (attr_name == "Fire Magic") { attribute_id = SafeAttribute::FireMagic; }
    else if (attr_name == "Water Magic") { attribute_id = SafeAttribute::WaterMagic; }
    else if (attr_name == "Energy Storage") { attribute_id = SafeAttribute::EnergyStorage; }
    else if (attr_name == "Healing Prayers") { attribute_id = SafeAttribute::HealingPrayers; }
    else if (attr_name == "Smiting Prayers") { attribute_id = SafeAttribute::SmitingPrayers; }
    else if (attr_name == "Protection Prayers") { attribute_id = SafeAttribute::ProtectionPrayers; }
    else if (attr_name == "Divine Favor") { attribute_id = SafeAttribute::DivineFavor; }
    else if (attr_name == "Strength") { attribute_id = SafeAttribute::Strength; }
    else if (attr_name == "Axe Mastery") { attribute_id = SafeAttribute::AxeMastery; }
    else if (attr_name == "Hammer Mastery") { attribute_id = SafeAttribute::HammerMastery; }
    else if (attr_name == "Swordsmanship") { attribute_id = SafeAttribute::Swordsmanship; }
    else if (attr_name == "Tactics") { attribute_id = SafeAttribute::Tactics; }
    else if (attr_name == "Beast Mastery") { attribute_id = SafeAttribute::BeastMastery; }
    else if (attr_name == "Expertise") { attribute_id = SafeAttribute::Expertise; }
    else if (attr_name == "Wilderness Survival") { attribute_id = SafeAttribute::WildernessSurvival; }
    else if (attr_name == "Marksmanship") { attribute_id = SafeAttribute::Marksmanship; }
    else if (attr_name == "Dagger Mastery") { attribute_id = SafeAttribute::DaggerMastery; }
    else if (attr_name == "Deadly Arts") { attribute_id = SafeAttribute::DeadlyArts; }
    else if (attr_name == "Shadow Arts") { attribute_id = SafeAttribute::ShadowArts; }
    else if (attr_name == "Communing") { attribute_id = SafeAttribute::Communing; }
    else if (attr_name == "Restoration Magic") { attribute_id = SafeAttribute::RestorationMagic; }
    else if (attr_name == "Channeling Magic") { attribute_id = SafeAttribute::ChannelingMagic; }
    else if (attr_name == "Critical Strikes") { attribute_id = SafeAttribute::CriticalStrikes; }
    else if (attr_name == "Spawning Power") { attribute_id = SafeAttribute::SpawningPower; }
    else if (attr_name == "Spear Mastery") { attribute_id = SafeAttribute::SpearMastery; }
    else if (attr_name == "Command") { attribute_id = SafeAttribute::Command; }
    else if (attr_name == "Motivation") { attribute_id = SafeAttribute::Motivation; }
    else if (attr_name == "Leadership") { attribute_id = SafeAttribute::Leadership; }
    else if (attr_name == "Scythe Mastery") { attribute_id = SafeAttribute::ScytheMastery; }
    else if (attr_name == "Wind Prayers") { attribute_id = SafeAttribute::WindPrayers; }
    else if (attr_name == "Earth Prayers") { attribute_id = SafeAttribute::EarthPrayers; }
    else if (attr_name == "Mysticism") { attribute_id = SafeAttribute::Mysticism; }
    else { attribute_id = SafeAttribute::None; }

}


AttributeClass::AttributeClass(std::string attr_name, int lvl) {
    if (attr_name == "Fast Casting") { attribute_id = SafeAttribute::FastCasting; }
    else if (attr_name == "Illusion Magic") { attribute_id = SafeAttribute::IllusionMagic; }
    else if (attr_name == "Domination Magic") { attribute_id = SafeAttribute::DominationMagic; }
    else if (attr_name == "Inspiration Magic") { attribute_id = SafeAttribute::InspirationMagic; }
    else if (attr_name == "Blood Magic") { attribute_id = SafeAttribute::BloodMagic; }
    else if (attr_name == "Death Magic") { attribute_id = SafeAttribute::DeathMagic; }
    else if (attr_name == "Soul Reaping") { attribute_id = SafeAttribute::SoulReaping; }
    else if (attr_name == "Curses") { attribute_id = SafeAttribute::Curses; }
    else if (attr_name == "Air Magic") { attribute_id = SafeAttribute::AirMagic; }
    else if (attr_name == "Earth Magic") { attribute_id = SafeAttribute::EarthMagic; }
    else if (attr_name == "Fire Magic") { attribute_id = SafeAttribute::FireMagic; }
    else if (attr_name == "Water Magic") { attribute_id = SafeAttribute::WaterMagic; }
    else if (attr_name == "Energy Storage") { attribute_id = SafeAttribute::EnergyStorage; }
    else if (attr_name == "Healing Prayers") { attribute_id = SafeAttribute::HealingPrayers; }
    else if (attr_name == "Smiting Prayers") { attribute_id = SafeAttribute::SmitingPrayers; }
    else if (attr_name == "Protection Prayers") { attribute_id = SafeAttribute::ProtectionPrayers; }
    else if (attr_name == "Divine Favor") { attribute_id = SafeAttribute::DivineFavor; }
    else if (attr_name == "Strength") { attribute_id = SafeAttribute::Strength; }
    else if (attr_name == "Axe Mastery") { attribute_id = SafeAttribute::AxeMastery; }
    else if (attr_name == "Hammer Mastery") { attribute_id = SafeAttribute::HammerMastery; }
    else if (attr_name == "Swordsmanship") { attribute_id = SafeAttribute::Swordsmanship; }
    else if (attr_name == "Tactics") { attribute_id = SafeAttribute::Tactics; }
    else if (attr_name == "Beast Mastery") { attribute_id = SafeAttribute::BeastMastery; }
    else if (attr_name == "Expertise") { attribute_id = SafeAttribute::Expertise; }
    else if (attr_name == "Wilderness Survival") { attribute_id = SafeAttribute::WildernessSurvival; }
    else if (attr_name == "Marksmanship") { attribute_id = SafeAttribute::Marksmanship; }
    else if (attr_name == "Dagger Mastery") { attribute_id = SafeAttribute::DaggerMastery; }
    else if (attr_name == "Deadly Arts") { attribute_id = SafeAttribute::DeadlyArts; }
    else if (attr_name == "Shadow Arts") { attribute_id = SafeAttribute::ShadowArts; }
    else if (attr_name == "Communing") { attribute_id = SafeAttribute::Communing; }
    else if (attr_name == "Restoration Magic") { attribute_id = SafeAttribute::RestorationMagic; }
    else if (attr_name == "Channeling Magic") { attribute_id = SafeAttribute::ChannelingMagic; }
    else if (attr_name == "Critical Strikes") { attribute_id = SafeAttribute::CriticalStrikes; }
    else if (attr_name == "Spawning Power") { attribute_id = SafeAttribute::SpawningPower; }
    else if (attr_name == "Spear Mastery") { attribute_id = SafeAttribute::SpearMastery; }
    else if (attr_name == "Command") { attribute_id = SafeAttribute::Command; }
    else if (attr_name == "Motivation") { attribute_id = SafeAttribute::Motivation; }
    else if (attr_name == "Leadership") { attribute_id = SafeAttribute::Leadership; }
    else if (attr_name == "Scythe Mastery") { attribute_id = SafeAttribute::ScytheMastery; }
    else if (attr_name == "Wind Prayers") { attribute_id = SafeAttribute::WindPrayers; }
    else if (attr_name == "Earth Prayers") { attribute_id = SafeAttribute::EarthPrayers; }
    else if (attr_name == "Mysticism") { attribute_id = SafeAttribute::Mysticism; }
    else { attribute_id = SafeAttribute::None; }

    level = lvl;

}


// Constructor definition
AttributeClass::AttributeClass(SafeAttribute attr_id, int lvl_base, int lvl, int dec_points, int inc_points)
    : attribute_id(attr_id), level_base(lvl_base), level(lvl), decrement_points(dec_points), increment_points(inc_points) {}

// Get the name of the attribute
std::string AttributeClass::GetName() const {
    switch (attribute_id) {
    case SafeAttribute::FastCasting: return "Fast Casting";
    case SafeAttribute::IllusionMagic: return "Illusion Magic";
    case SafeAttribute::DominationMagic: return "Domination Magic";
    case SafeAttribute::InspirationMagic: return "Inspiration Magic";
    case SafeAttribute::BloodMagic: return "Blood Magic";
    case SafeAttribute::DeathMagic: return "Death Magic";
    case SafeAttribute::SoulReaping: return "Soul Reaping";
    case SafeAttribute::Curses: return "Curses";
    case SafeAttribute::AirMagic: return "Air Magic";
    case SafeAttribute::EarthMagic: return "Earth Magic";
    case SafeAttribute::FireMagic: return "Fire Magic";
    case SafeAttribute::WaterMagic: return "Water Magic";
    case SafeAttribute::EnergyStorage: return "Energy Storage";
    case SafeAttribute::HealingPrayers: return "Healing Prayers";
    case SafeAttribute::SmitingPrayers: return "Smiting Prayers";
    case SafeAttribute::ProtectionPrayers: return "Protection Prayers";
    case SafeAttribute::DivineFavor: return "Divine Favor";
    case SafeAttribute::Strength: return "Strength";
    case SafeAttribute::AxeMastery: return "Axe Mastery";
    case SafeAttribute::HammerMastery: return "Hammer Mastery";
    case SafeAttribute::Swordsmanship: return "Swordsmanship";
    case SafeAttribute::Tactics: return "Tactics";
    case SafeAttribute::BeastMastery: return "Beast Mastery";
    case SafeAttribute::Expertise: return "Expertise";
    case SafeAttribute::WildernessSurvival: return "Wilderness Survival";
    case SafeAttribute::Marksmanship: return "Marksmanship";
    case SafeAttribute::DaggerMastery: return "Dagger Mastery";
    case SafeAttribute::DeadlyArts: return "Deadly Arts";
    case SafeAttribute::ShadowArts: return "Shadow Arts";
    case SafeAttribute::Communing: return "Communing";
    case SafeAttribute::RestorationMagic: return "Restoration Magic";
    case SafeAttribute::ChannelingMagic: return "Channeling Magic";
    case SafeAttribute::CriticalStrikes: return "Critical Strikes";
    case SafeAttribute::SpawningPower: return "Spawning Power";
    case SafeAttribute::SpearMastery: return "Spear Mastery";
    case SafeAttribute::Command: return "Command";
    case SafeAttribute::Motivation: return "Motivation";
    case SafeAttribute::Leadership: return "Leadership";
    case SafeAttribute::ScytheMastery: return "Scythe Mastery";
    case SafeAttribute::WindPrayers: return "Wind Prayers";
    case SafeAttribute::EarthPrayers: return "Earth Prayers";
    case SafeAttribute::Mysticism: return "Mysticism";
    case SafeAttribute::None: return "None";
    default: return "Unknown";
    }
}


PyAgent::PyAgent(int agent_id) : id(agent_id) {
    id = agent_id;
    GetContext();  // Initialize the context with the given agent ID
}

void PyAgent::Set(int agent_id) {
    id = agent_id;
    GetContext();  // Update the context with the given agent ID
}

void PyAgent::GetContext() {
    GW::Agent* agent = GW::Agents::GetAgentByID(id);  // Get the agent by the given ID
    if (!agent) return;

    x = agent->x;  // Get the x coordinate
    y = agent->y;  // Get the y coordinate
    z = agent->z;  // Get the z coordinate
    zplane = agent->plane;  // Get the z plane
    rotation_angle = agent->rotation_angle;  // Get the rotation angle
    rotation_cos = agent->rotation_cos;  // Get the rotation cosine
    rotation_sin = agent->rotation_sin;  // Get the rotation sine
    velocity_x = agent->move_x;  // Get the x velocity
    velocity_y = agent->move_y;  // Get the y velocity

    is_living = agent->GetIsLivingType();  // Check if the agent is a living type
    is_item = agent->GetIsItemType();  // Check if the agent is an item type
    is_gadget = agent->GetIsGadgetType();  // Check if the agent is a gadget type

    living_agent = id;
    item_agent = id;
    gadget_agent = id;

    GW::Attribute* temp = GW::PartyMgr::GetAgentAttributes(id);
    if (!temp) return;

    // Clear existing attributes before updating
    attributes.clear();

    int controlled_id = static_cast<int>(GW::Agents::GetControlledCharacterId());

    if (controlled_id == id) {
        for (int i = 0; i < static_cast<int>(SafeAttribute::None); ++i) {
            if (temp[i].level_base > 0 || temp[i].level > 0 || temp[i].decrement_points > 0 || temp[i].increment_points > 0) {
                attributes.emplace_back(static_cast<SafeAttribute>(i), temp[i].level_base, temp[i].level, temp[i].decrement_points, temp[i].increment_points);
            }
        }
    }
}



// Bind the Profession enum
void bind_ProfessionType(py::module_& m) {
    py::enum_<ProfessionType>(m, "ProfessionType")
        .value("None", ProfessionType::None)
        .value("Warrior", ProfessionType::Warrior)
        .value("Ranger", ProfessionType::Ranger)
        .value("Monk", ProfessionType::Monk)
        .value("Necromancer", ProfessionType::Necromancer)
        .value("Mesmer", ProfessionType::Mesmer)
        .value("Elementalist", ProfessionType::Elementalist)
        .value("Assassin", ProfessionType::Assassin)
        .value("Ritualist", ProfessionType::Ritualist)
        .value("Paragon", ProfessionType::Paragon)
        .value("Dervish", ProfessionType::Dervish)
        .export_values();
}

// Bind the ProfessionClass
void bind_Profession(py::module_& m) {
    py::class_<Profession>(m, "Profession")
        .def(py::init<int>())  // Constructor that takes an int (Profession enum value)
		.def(py::init<std::string>())  // Constructor that takes a string (Profession name)
        .def("Set", &Profession::Set)  // Set method for Profession
        .def("Get", &Profession::Get)  // Get method returning Profession
        .def("ToInt", &Profession::ToInt)  // Converts Profession to int
        .def("GetName", &Profession::GetName)  // Returns name of Profession
        .def("GetShortName", &Profession::GetShortName)  // Returns short name of Profession
        .def("__eq__", &Profession::operator==)  // Equality operator
        .def("__ne__", &Profession::operator!=);  // Inequality operator
}

void bind_AllegianceType(py::module_& m) {
    py::enum_<AllegianceType>(m, "AllegianceType")
        .value("Unknown", AllegianceType::Unknown)
        .value("Ally", AllegianceType::Ally)
        .value("Neutral", AllegianceType::Neutral)
        .value("Enemy", AllegianceType::Enemy)
        .value("SpiritPet", AllegianceType::SpiritPet)
        .value("Minion", AllegianceType::Minion)
        .value("NpcMinipet", AllegianceType::NpcMinipet)
        .export_values();
}

void bind_Allegiance(py::module_& m) {
    py::class_<Allegiance>(m, "Allegiance")
        .def(py::init<int>())
        .def("Set", &Allegiance::Set)         // Renamed to PascalCase
        .def("Get", &Allegiance::Get)         // Renamed to PascalCase
        .def("ToInt", &Allegiance::ToInt)     // Renamed to PascalCase
        .def("GetName", &Allegiance::GetName) // Renamed to PascalCase
        .def("__eq__", &Allegiance::operator==)
        .def("__ne__", &Allegiance::operator!=);
}

void bind_PyWeaponType(py::module_& m) {
    py::enum_<PyWeaponType>(m, "PyWeaponType")
        .value("Unknown", PyWeaponType::Unknown)
        .value("Bow", PyWeaponType::Bow)
        .value("Axe", PyWeaponType::Axe)
        .value("Hammer", PyWeaponType::Hammer)
        .value("Daggers", PyWeaponType::Daggers)
        .value("Scythe", PyWeaponType::Scythe)
        .value("Spear", PyWeaponType::Spear)
        .value("Sword", PyWeaponType::Sword)
        .value("Scepter", PyWeaponType::Scepter)
        .value("Scepter2", PyWeaponType::Scepter2)
        .value("Wand", PyWeaponType::Wand)
        .value("Staff1", PyWeaponType::Staff1)
        .value("Staff", PyWeaponType::Staff)
        .value("Staff2", PyWeaponType::Staff2)
        .value("Staff3", PyWeaponType::Staff3)
        .value("Unknown1", PyWeaponType::Unknown1)
        .value("Unknown2", PyWeaponType::Unknown2)
        .value("Unknown3", PyWeaponType::Unknown3)
        .value("Unknown4", PyWeaponType::Unknown4)
        .value("Unknown5", PyWeaponType::Unknown5)
        .value("Unknown6", PyWeaponType::Unknown6)
        .value("Unknown7", PyWeaponType::Unknown7)
        .value("Unknown8", PyWeaponType::Unknown8)
        .value("Unknown9", PyWeaponType::Unknown9)
        .value("Unknown10", PyWeaponType::Unknown10)
        .export_values();
}

void bind_Weapon(py::module_& m) {
    py::class_<Weapon>(m, "Weapon")
        .def(py::init<int>())
        .def("Set", &Weapon::Set)           // Renamed to PascalCase
        .def("Get", &Weapon::Get)           // Renamed to PascalCase
        .def("ToInt", &Weapon::ToInt)       // Renamed to PascalCase
        .def("GetName", &Weapon::GetName)   // Renamed to PascalCase
        .def("__eq__", &Weapon::operator==)
        .def("__ne__", &Weapon::operator!=);
}

void bind_attributeTypeClass(py::module_& m) {
        py::enum_<SafeAttribute>(m, "SafeAttribute")
            .value("FastCasting", SafeAttribute::FastCasting)
            .value("IllusionMagic", SafeAttribute::IllusionMagic)
            .value("DominationMagic", SafeAttribute::DominationMagic)
            .value("InspirationMagic", SafeAttribute::InspirationMagic)
            .value("BloodMagic", SafeAttribute::BloodMagic)
            .value("DeathMagic", SafeAttribute::DeathMagic)
            .value("SoulReaping", SafeAttribute::SoulReaping)
            .value("Curses", SafeAttribute::Curses)
            .value("AirMagic", SafeAttribute::AirMagic)
            .value("EarthMagic", SafeAttribute::EarthMagic)
            .value("FireMagic", SafeAttribute::FireMagic)
            .value("WaterMagic", SafeAttribute::WaterMagic)
            .value("EnergyStorage", SafeAttribute::EnergyStorage)
            .value("HealingPrayers", SafeAttribute::HealingPrayers)
            .value("SmitingPrayers", SafeAttribute::SmitingPrayers)
            .value("ProtectionPrayers", SafeAttribute::ProtectionPrayers)
            .value("DivineFavor", SafeAttribute::DivineFavor)
            .value("Strength", SafeAttribute::Strength)
            .value("AxeMastery", SafeAttribute::AxeMastery)
            .value("HammerMastery", SafeAttribute::HammerMastery)
            .value("Swordsmanship", SafeAttribute::Swordsmanship)
            .value("Tactics", SafeAttribute::Tactics)
            .value("BeastMastery", SafeAttribute::BeastMastery)
            .value("Expertise", SafeAttribute::Expertise)
            .value("WildernessSurvival", SafeAttribute::WildernessSurvival)
            .value("Marksmanship", SafeAttribute::Marksmanship)
            .value("DaggerMastery", SafeAttribute::DaggerMastery)
            .value("DeadlyArts", SafeAttribute::DeadlyArts)
            .value("ShadowArts", SafeAttribute::ShadowArts)
            .value("Communing", SafeAttribute::Communing)
            .value("RestorationMagic", SafeAttribute::RestorationMagic)
            .value("ChannelingMagic", SafeAttribute::ChannelingMagic)
            .value("CriticalStrikes", SafeAttribute::CriticalStrikes)
            .value("SpawningPower", SafeAttribute::SpawningPower)
            .value("SpearMastery", SafeAttribute::SpearMastery)
            .value("Command", SafeAttribute::Command)
            .value("Motivation", SafeAttribute::Motivation)
            .value("Leadership", SafeAttribute::Leadership)
            .value("ScytheMastery", SafeAttribute::ScytheMastery)
            .value("WindPrayers", SafeAttribute::WindPrayers)
            .value("EarthPrayers", SafeAttribute::EarthPrayers)
            .value("Mysticism", SafeAttribute::Mysticism)
            .value("None", SafeAttribute::None)
            .export_values();
    }


void bind_attribute_class(py::module_& m) {
    py::class_<AttributeClass>(m, "AttributeClass")
        .def(py::init<SafeAttribute, int, int, int, int>())  // Constructor
		.def(py::init<std::string>())  // Constructor with attribute name
		.def(py::init<std::string, int>())  // Constructor with attribute name and level
        .def("GetName", &AttributeClass::GetName)          // Get name of the attribute
        .def_readonly("attribute_id", &AttributeClass::attribute_id)  // Attribute ID
        .def_readonly("level_base", &AttributeClass::level_base)      // Base level
        .def_readonly("level", &AttributeClass::level)                // Level with modifiers
        .def_readonly("decrement_points", &AttributeClass::decrement_points)  // Points to decrement
        .def_readonly("increment_points", &AttributeClass::increment_points)  // Points to increment
        .def("__eq__", &AttributeClass::operator==)                    // Equality operator
        .def("__ne__", &AttributeClass::operator!=);                   // Inequality operator
}



// Bind the PyLivingAgent class
void bind_PyLivingAgent(py::module_& m) {
    py::class_<PyLivingAgent>(m, "PyLivingAgent")
        .def(py::init<int>())  // Constructor with agent_id
        .def("GetContext", &PyLivingAgent::GetContext)  // Method to get context

        // Public fields
        .def_readonly("agent_id", &PyLivingAgent::agent_id)
        .def_readonly("owner_id", &PyLivingAgent::owner_id)
        .def_readonly("player_number", &PyLivingAgent::player_number)
        .def_readonly("profession", &PyLivingAgent::profession)
        .def_readonly("secondary_profession", &PyLivingAgent::secondary_profession)
        .def_readonly("level", &PyLivingAgent::level)
        .def_readonly("energy", &PyLivingAgent::energy)
        .def_readonly("max_energy", &PyLivingAgent::max_energy)
        .def_readonly("energy_regen", &PyLivingAgent::energy_regen)
        .def_readonly("hp", &PyLivingAgent::hp)
        .def_readonly("max_hp", &PyLivingAgent::max_hp)
        .def_readonly("hp_regen", &PyLivingAgent::hp_regen)
        .def_readonly("login_number", &PyLivingAgent::login_number)
        .def_readonly("name", &PyLivingAgent::name)
        .def_readonly("dagger_status", &PyLivingAgent::dagger_status)
        .def_readonly("allegiance", &PyLivingAgent::allegiance)
        .def_readonly("weapon_type", &PyLivingAgent::weapon_type)
        .def_readonly("weapon_item_type", &PyLivingAgent::weapon_item_type)
        .def_readonly("offhand_item_type", &PyLivingAgent::offhand_item_type)
        .def_readonly("weapon_item_id", &PyLivingAgent::weapon_item_id)
        .def_readonly("offhand_item_id", &PyLivingAgent::offhand_item_id)


        // Boolean fields
        .def_readonly("is_bleeding", &PyLivingAgent::is_bleeding)
        .def_readonly("is_conditioned", &PyLivingAgent::is_conditioned)
        .def_readonly("is_crippled", &PyLivingAgent::is_crippled)
        .def_readonly("is_dead", &PyLivingAgent::is_dead)
        .def_readonly("is_deep_wounded", &PyLivingAgent::is_deep_wounded)
        .def_readonly("is_poisoned", &PyLivingAgent::is_poisoned)
        .def_readonly("is_enchanted", &PyLivingAgent::is_enchanted)
        .def_readonly("is_degen_hexed", &PyLivingAgent::is_degen_hexed)
        .def_readonly("is_hexed", &PyLivingAgent::is_hexed)
        .def_readonly("is_weapon_spelled", &PyLivingAgent::is_weapon_spelled)
        .def_readonly("in_combat_stance", &PyLivingAgent::in_combat_stance)
        .def_readonly("has_quest", &PyLivingAgent::has_quest)
        .def_readonly("is_dead_by_typemap", &PyLivingAgent::is_dead_by_typemap)
        .def_readonly("is_female", &PyLivingAgent::is_female)
        .def_readonly("has_boss_glow", &PyLivingAgent::has_boss_glow)
        .def_readonly("is_hiding_cape", &PyLivingAgent::is_hiding_cape)
        .def_readonly("can_be_viewed_in_party_window", &PyLivingAgent::can_be_viewed_in_party_window)
        .def_readonly("is_spawned", &PyLivingAgent::is_spawned)
        .def_readonly("is_being_observed", &PyLivingAgent::is_being_observed)
        .def_readonly("is_knocked_down", &PyLivingAgent::is_knocked_down)
        .def_readonly("is_moving", &PyLivingAgent::is_moving)
        .def_readonly("is_attacking", &PyLivingAgent::is_attacking)
        .def_readonly("is_casting", &PyLivingAgent::is_casting)
        .def_readonly("is_idle", &PyLivingAgent::is_idle)
        .def_readonly("is_alive", &PyLivingAgent::is_alive)
        .def_readonly("is_player", &PyLivingAgent::is_player)
        .def_readonly("is_npc", &PyLivingAgent::is_npc)
        .def_readonly("casting_skill_id", &PyLivingAgent::casting_skill_id)
        .def_readonly("overcast", &PyLivingAgent::overcast)
        .def("GetName", &PyLivingAgent::GetName)
        .def("RequestName", &PyLivingAgent::RequestName)
        .def("IsAgentNameReady", &PyLivingAgent::IsAgentNameReady);
}


void bind_PyItemAgent(py::module_& m) {
    py::class_<PyItemAgent>(m, "PyItemAgent")
        .def(py::init<int>())  // Constructor with int parameter
        .def("GetContext", &PyItemAgent::GetContext)  // Bind GetContext method
        .def_readonly("agent_id", &PyItemAgent::agent_id)  // Expose agent_id as a read-write attribute
        .def_readonly("owner_id", &PyItemAgent::owner_id)  // Expose owner_id as a read-write attribute
        .def_readonly("item_id", &PyItemAgent::item_id)  // Expose item_id as a read-write attribute
        .def_readonly("h00CC", &PyItemAgent::h00CC)  // Expose h00CC as a read-write attribute
        .def_readonly("extra_type", &PyItemAgent::extra_type);  // Expose extra_type as a read-write attribute
}

void bind_PyGadgetAgent(py::module_& m) {
    py::class_<PyGadgetAgent>(m, "PyGadgetAgent")
        .def(py::init<int>())  // Constructor with int parameter
        .def("GetContext", &PyGadgetAgent::GetContext)  // Bind GetContext method
        .def_readonly("agent_id", &PyGadgetAgent::agent_id)  // Expose agent_id as a read-write attribute
        .def_readonly("h00C4", &PyGadgetAgent::h00C4)  // Expose h00C4 as a read-write attribute
        .def_readonly("h00C8", &PyGadgetAgent::h00C8)  // Expose h00C8 as a read-write attribute
        .def_readonly("h00D4", &PyGadgetAgent::h00D4)  // Expose h00D4 as a read-write attribute
        .def_readonly("extra_type", &PyGadgetAgent::extra_type)  // Expose extra_type as a read-write attribute
        .def_readonly("gadget_id", &PyGadgetAgent::gadget_id);  // Expose gadget_id as a read-write attribute
}



// Bind the PyAgent class
void bind_PyAgent(py::module_& m) {
    py::class_<PyAgent>(m, "PyAgent")
        .def(py::init<int>())  // Constructor with agent_id
        .def("Set", &PyAgent::Set)  // Set method to update agent_id and context
        .def("GetContext", &PyAgent::GetContext)  // Method to refresh the context
        .def_readonly("id", &PyAgent::id)  // Access to the id field
        .def_readonly("x", &PyAgent::x)  // Access to the x field
        .def_readonly("y", &PyAgent::y)  // Access to the y field
        .def_readonly("z", &PyAgent::z)  // Access to the z field
        .def_readonly("zplane", &PyAgent::zplane)  // Access to the zplane field
        .def_readonly("rotation_angle", &PyAgent::rotation_angle)  // Access to the rotation_angle field
        .def_readonly("rotation_cos", &PyAgent::rotation_cos)  // Access to the rotation_cos field
        .def_readonly("rotation_sin", &PyAgent::rotation_sin)  // Access to the rotation_sin field
        .def_readonly("velocity_x", &PyAgent::velocity_x)  // Access to the velocity_x field
        .def_readonly("velocity_y", &PyAgent::velocity_y)  // Access to the velocity_y field
        .def_readonly("is_living", &PyAgent::is_living)  // Access to the is_living field
        .def_readonly("is_item", &PyAgent::is_item)  // Access to the is_item field
        .def_readonly("is_gadget", &PyAgent::is_gadget)  // Access to the is_gadget field
        .def_readonly("living_agent", &PyAgent::living_agent)  // Access to the living_agent object
        .def_readonly("item_agent", &PyAgent::item_agent)  // Access to the item_agent object
        .def_readonly("gadget_agent", &PyAgent::gadget_agent)  // Access to the gadget_agent object
        .def_readonly("attributes", &PyAgent::attributes);  // Access to the attributes object
}


// Main function to create bindings
PYBIND11_EMBEDDED_MODULE(PyAgent, m) {
    bind_ProfessionType(m);
    bind_Profession(m);
    bind_AllegianceType(m);
    bind_Allegiance(m);
    bind_PyWeaponType(m);
    bind_Weapon(m);
    bind_PyLivingAgent(m);
    bind_attributeTypeClass(m);
    bind_attribute_class(m);
    bind_PyItemAgent(m);
    bind_PyGadgetAgent(m);
    bind_PyAgent(m);
}

