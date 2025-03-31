#pragma once
#include "Headers.h"

namespace py = pybind11;



class Profession {
private:
    ProfessionType profession;

public:
    // Constructor
    Profession() : profession(ProfessionType::None) {}
    Profession(int prof);
	Profession(std::string prof);

    // Setter
    void Set(int prof);

    // Getter
    ProfessionType Get() const;

    // Equality operators
    bool Profession::operator==(const Profession& other) const { return profession == other.profession; }
    bool Profession::operator!=(const Profession& other) const { return profession != other.profession; }

    // Convert to int
    int ToInt() const;

    // Get name as a string
    std::string GetName() const;

    // Get short name
    std::string GetShortName() const;
};



class Allegiance {
private:
    AllegianceType allegiance;

public:
    Allegiance(int value);

    void Set(int value);

    AllegianceType Get() const;

    bool operator==(const Allegiance& other) const {
        return allegiance == other.allegiance;
    }

    bool operator!=(const Allegiance& other) const {
        return allegiance != other.allegiance;
    }

    int ToInt() const;

    // Get name as a string
    std::string GetName() const;
};

class Weapon {
private:
    PyWeaponType weapon_type;

public:
    Weapon(int value);

    void Set(int value);

    PyWeaponType Get() const;

    bool Weapon::operator==(const Weapon& other) const {
        return weapon_type == other.weapon_type;
    }

    bool Weapon::operator!=(const Weapon& other) const {
        return weapon_type != other.weapon_type;
    }

    int ToInt() const;

    std::string GetName() const;
};



class PyLivingAgent {
public:
    int agent_id;
    int owner_id = -1;
    int player_number;
    Profession profession = 0;
    Profession secondary_profession = 0;
    int level = 0;
    float energy = 0;
    float max_energy = 0;
    float energy_regen = 0;
    float hp;
    float max_hp;
    float hp_regen;
    int login_number;
    std::string name = "";
    bool name_ready;
    int dagger_status;
    Allegiance allegiance = 0;
    Weapon weapon_type = 0;

    int weapon_item_type = 0;
    int offhand_item_type = 0;
    int weapon_item_id = 0;
    int offhand_item_id = 0;
    

    bool is_bleeding = false;
    bool is_conditioned = false;
    bool is_crippled = false;
    bool is_dead = false;
    bool is_deep_wounded = false;
    bool is_poisoned = false;
    bool is_enchanted = false;
    bool is_degen_hexed = false;
    bool is_hexed = false;
    bool is_weapon_spelled = false;

    // Agent TypeMap Bitmasks
    bool in_combat_stance = false;
    bool has_quest = false;
    bool is_dead_by_typemap = false;
    bool is_female = false;
    bool has_boss_glow = false;
    bool is_hiding_cape = false;
    bool can_be_viewed_in_party_window = false;
    bool is_spawned = false;
    bool is_being_observed = false;

    // Modelstates
    bool is_knocked_down = false;
    bool is_moving = false;
    bool is_attacking = false;
    bool is_casting = false;
    bool is_idle = false;

    // Composite bool, sometimes agents can be dead but have hp (e.g. packets are received in wrong order)
    bool is_alive = false;

    bool is_player = false;
    bool is_npc = false;

    int casting_skill_id = 0;
    float overcast = 0;
public:
    PyLivingAgent(int agent_id);
    void GetContext();
	void RequestName();
    bool IsAgentNameReady();
	std::string GetName();
    
};



class PyItemAgent {
public:
    int agent_id = 0;
    int owner_id = -1;
    int item_id = 0;
    uint32_t h00CC;
    uint32_t extra_type = 0;

    PyItemAgent(int agent_id);
    void GetContext();
};


class PyGadgetAgent {
public:
    int agent_id = 0;
    uint32_t h00C4;
    uint32_t h00C8;
    int extra_type = 0;
    int gadget_id = 0;
    std::vector<uint32_t> h00D4;

    PyGadgetAgent(int agent_id);
    void GetContext();
};



class AttributeClass {
public:
    SafeAttribute attribute_id;  // The attribute ID (e.g. FastCasting, IllusionMagic, etc.)
    int level_base = 0;          // Base level of the attribute (without modifiers)
    int level = 0;               // Final level of the attribute (with modifiers)
    int decrement_points = 0;    // Points available for decrementing the attribute
    int increment_points = 0;    // Points needed to increment the attribute

    // Constructor to initialize the attribute with given values
    AttributeClass(SafeAttribute attr_id, int lvl_base, int lvl, int dec_points, int inc_points);

    AttributeClass(int attr_id) {
        attribute_id = static_cast<SafeAttribute>(attr_id);
    }

    AttributeClass(std::string attr_name);
    AttributeClass(std::string attr_name, int lvl);

    // Method to get the name of the attribute as a string
    std::string GetName() const;

    // Equality operator
    bool operator==(const AttributeClass& other) const {
        return attribute_id == other.attribute_id;
    }

    // Inequality operator
    bool operator!=(const AttributeClass& other) const {
        return attribute_id != other.attribute_id;
    }

};


class PyAgent {
public:
     int id = 0;
    float x = 0;
    float y = 0;
    float z = 0;
	float screen_x = 0;
	float screen_y = 0;
    int zplane = 0;
    float rotation_angle = 0;
    float rotation_cos = 0;
    float rotation_sin = 0;
    float velocity_x = 0;
    float velocity_y = 0;
    bool is_living = false;
    bool is_item = false;
    bool is_gadget = false;
    GW::AgentLiving* living = nullptr;
    PyLivingAgent living_agent = 0;
    PyItemAgent item_agent = 0;
    PyGadgetAgent gadget_agent = 0;
    std::vector<AttributeClass> attributes;

    PyAgent(int agent_id);  // Updated constructor
    void Set(int agent_id);
    void GetContext();  // Updated method to take agent_id as a parameter
	bool IsValid(int agent_id);
};

