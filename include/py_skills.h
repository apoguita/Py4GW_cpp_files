#pragma once
#include "Headers.h"

namespace py = pybind11;


class SkillID {
public:
    int ID = static_cast<int>(GW::Constants::SkillID::No_Skill);

    SkillID(int skillid) : ID(skillid) {}

    SkillID(std::string skillname) {
        NameClass name(0);
        ID = static_cast<int>(name.GetSkillIDByName(skillname));
    }

    SkillID() = default;

    SkillID& operator=(int skillid) { ID = skillid; return *this; }

    bool operator==(int skillid) const { return ID == skillid; }

    bool operator!=(int skillid) const { return ID != skillid; }

    std::string get_name() const {
        NameClass name(0);
        return name.GetSkillNameByID(static_cast<GW::Constants::SkillID>(ID));
    }
};

class SkillType {
public:
    int ID = static_cast<int>(GW::Constants::SkillType::Skill);

    SkillType(int skilltype) : ID(skilltype) {}

    SkillType() = default;

    SkillType& operator=(int skilltype) { ID = skilltype; return *this; }

    bool operator==(int skilltype) const { return ID == skilltype; }

    bool operator!=(int skilltype) const { return ID != skilltype; }

    std::string get_name() const {

        NameClass name = NameClass(0);
        return name.GetTypeByID(static_cast<GW::Constants::SkillType>(ID));
    }
};


class Skill {
public:
    SkillID ID = 0;
    CampaignWrapper CampaignData = static_cast<int>(SafeCampaign::Undefined);
    SkillType Type = static_cast<int>(GW::Constants::SkillType::Skill);
    int Special = 0;
    int ComboReq = 0;
    int Effect1 = 0;
    int Condition = 0;
    int Effect2 = 0;
    int WeaponReq = 0;
    Profession Profession = static_cast<int>(GW::Constants::Profession::None);
    AttributeClass Attribute = 0;
    int Title = 0;
    int IDPvP = 0;
    int Combo = 0;
    int Target = 0;
    int SkillEquipType = 0;
    int Overcast = 0; // only if special flag has 0x000001 set
    int EnergyCost = 0;
    int HealthCost = 0;
    int Adrenaline = 0;
    float Activation = 0;
    float Aftercast = 0;
    int Duration0 = 0;
    int Duration15 = 0;
    int Recharge = 0;
    int SkillArguments = 0;
    int Scale0 = 0;
    int Scale15 = 0;
    int BonusScale0 = 0;
    int BonusScale15 = 0;
    float AoERange = 0;
    float ConstEffect = 0;
    int CasterOverheadAnimationID = 0;
    int CasterBodyAnimationID = 0;
    int TargetBodyAnimationID = 0;
    int TargetOverheadAnimationID = 0;
    int ProjectileAnimation1ID = 0;
    int ProjectileAnimation2ID = 0;
    int IconFileID = 0;
    int IconFileID2 = 0;
    int NameID = 0;
    int Concise = 0;
    int DescriptionID = 0;

    bool IsTouchRange = false;
    bool IsElite = false;
    bool IsHalfRange = false;
    bool IsPvP = false;
    bool IsPvE = false;
    bool IsPlayable = false;

    bool IsStacking = false;
    bool IsNonStacking = false;
    bool IsUnused = false;

    int adrenaline_a = 0;
    int adrenaline_b = 0;
    int recharge = 0;

	uint32_t h0004 = 0; // unused, but present in the original struct
    int h0032 = 0; // unused, but present in the original struct
	int h0037 = 0; // unused, but present in the original struct
	uint16_t h0050[4] = { 0, 0, 0, 0 }; // unused, but present in the original struct

    void GetContext();
    Skill() { GetContext(); }
    Skill(int skillid) : ID(skillid) { GetContext(); }
    Skill(std::string skillname) : ID(skillname) { GetContext(); }

};
