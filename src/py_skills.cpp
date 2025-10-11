#include "py_skills.h"

void Skill::GetContext() {
    auto skill = GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(ID.ID));
    if (skill) {
        CampaignData = static_cast<int>(skill->campaign);
        Type = static_cast<int>(skill->type);
        Special = skill->special;
        ComboReq = skill->combo_req;
        Effect1 = skill->effect1;
        Condition = skill->condition;
        Effect2 = skill->effect2;
        WeaponReq = skill->weapon_req;
        Profession = static_cast<int>(skill->profession);

        Attribute = AttributeClass(static_cast<int>(skill->attribute));

        Title = skill->title;
        IDPvP = static_cast<int>(skill->skill_id_pvp);
        Combo = skill->combo;
        Target = skill->target;
        SkillEquipType = skill->skill_equip_type;
        Overcast = skill->overcast;
		EnergyCost = skill->GetEnergyCost();
        HealthCost = skill->health_cost;
        Adrenaline = skill->adrenaline;
        Activation = skill->activation;
        Aftercast = skill->aftercast;
        Duration0 = skill->duration0;
        Duration15 = skill->duration15;
        Recharge = skill->recharge;
        SkillArguments = skill->skill_arguments;
        Scale0 = skill->scale0;
        Scale15 = skill->scale15;
        BonusScale0 = skill->bonusScale0;
        BonusScale15 = skill->bonusScale15;
        AoERange = skill->aoe_range;
        ConstEffect = skill->const_effect;
        CasterOverheadAnimationID = skill->caster_overhead_animation_id;
        CasterBodyAnimationID = skill->caster_body_animation_id;
        TargetBodyAnimationID = skill->target_body_animation_id;
        TargetOverheadAnimationID = skill->target_overhead_animation_id;
        ProjectileAnimation1ID = skill->projectile_animation_1_id;
        ProjectileAnimation2ID = skill->projectile_animation_2_id;
        IconFileID = skill->icon_file_id;
        IconFileID2 = skill->icon_file_id_2;
        NameID = skill->name;
        Concise = skill->concise;
        DescriptionID = skill->description;

        IsTouchRange = skill->IsTouchRange();
        IsElite = skill->IsElite();
        IsHalfRange = skill->IsHalfRange();
        IsPvP = skill->IsPvP();
        IsPvE = skill->IsPvE();
        IsPlayable = skill->IsPlayable();

        IsStacking = skill->IsStacking();
        IsNonStacking = skill->IsNonStacking();
        IsUnused = skill->IsUnused();

		h0004 = skill->h0004;
		h0032 = skill->h0032;
		h0037 = skill->h0037;
    }
}


void bind_SkillID(py::module_& m) {
    py::class_<SkillID>(m, "SkillID")
        .def(py::init<>())  // Default constructor
        .def(py::init<int>(), py::arg("id"))  // Constructor with skill ID
        .def(py::init<std::string>(), py::arg("skillname"))  // Constructor with skill name
        .def("__eq__", &SkillID::operator==)  // Equality operator
        .def("__ne__", &SkillID::operator!=)  // Inequality operator
        .def("GetName", &SkillID::get_name)  // Get name method
        .def_readonly("id", &SkillID::ID);   // Public field access
}


void bind_SkillType(py::module_& m) {
    py::class_<SkillType>(m, "SkillType")
        .def(py::init<int>())  // Constructor with skilltype
        .def(py::init<>())     // Default constructor
        .def("__eq__", &SkillType::operator==)  // Equality operator
        .def("__ne__", &SkillType::operator!=)  // Inequality operator
        .def("GetName", &SkillType::get_name)  // Get name method
        .def_readonly("id", &SkillType::ID);   // Public field access
}

void bind_Skill(py::module_& m) {
    py::class_<Skill>(m, "Skill")
        .def(py::init<>())  // Default constructor
        .def(py::init<int>(), py::arg("id"))  // Constructor with skillid
        .def(py::init<std::string>(), py::arg("skillname"))  // Constructor with skillname
        .def("GetContext", &Skill::GetContext)  // Method to populate context
        // Exposing all public fields as readwrite properties
        .def_readonly("id", &Skill::ID)
        .def_readonly("campaign", &Skill::CampaignData)
        .def_readonly("type", &Skill::Type)
        .def_readonly("special", &Skill::Special)
        .def_readonly("combo_req", &Skill::ComboReq)
        .def_readonly("effect1", &Skill::Effect1)
        .def_readonly("condition", &Skill::Condition)
        .def_readonly("effect2", &Skill::Effect2)
        .def_readonly("weapon_req", &Skill::WeaponReq)
        .def_readonly("profession", &Skill::Profession)
        .def_readonly("attribute", &Skill::Attribute)
        .def_readonly("title", &Skill::Title)
        .def_readonly("id_pvp", &Skill::IDPvP)
        .def_readonly("combo", &Skill::Combo)
        .def_readonly("target", &Skill::Target)
        .def_readonly("skill_equip_type", &Skill::SkillEquipType)
        .def_readonly("overcast", &Skill::Overcast)
        .def_readonly("energy_cost", &Skill::EnergyCost)
        .def_readonly("health_cost", &Skill::HealthCost)
        .def_readonly("adrenaline", &Skill::Adrenaline)
        .def_readonly("activation", &Skill::Activation)
        .def_readonly("aftercast", &Skill::Aftercast)
        .def_readonly("duration_0pts", &Skill::Duration0)
        .def_readonly("duration_15pts", &Skill::Duration15)
        .def_readonly("recharge", &Skill::Recharge)
        .def_readonly("skill_arguments", &Skill::SkillArguments)
        .def_readonly("scale_0pts", &Skill::Scale0)
        .def_readonly("scale_15pts", &Skill::Scale15)
        .def_readonly("bonus_scale_0pts", &Skill::BonusScale0)
        .def_readonly("bonus_scale_15pts", &Skill::BonusScale15)
        .def_readonly("aoe_range", &Skill::AoERange)
        .def_readonly("const_effect", &Skill::ConstEffect)
        .def_readonly("caster_overhead_animation_id", &Skill::CasterOverheadAnimationID)
        .def_readonly("caster_body_animation_id", &Skill::CasterBodyAnimationID)
        .def_readonly("target_body_animation_id", &Skill::TargetBodyAnimationID)
        .def_readonly("target_overhead_animation_id", &Skill::TargetOverheadAnimationID)
        .def_readonly("projectile_animation1_id", &Skill::ProjectileAnimation1ID)
        .def_readonly("projectile_animation2_id", &Skill::ProjectileAnimation2ID)
        .def_readonly("icon_file_id", &Skill::IconFileID)
        .def_readonly("icon_file2_id", &Skill::IconFileID2)
        .def_readonly("name_id", &Skill::NameID)
        .def_readonly("concise", &Skill::Concise)
        .def_readonly("description_id", &Skill::DescriptionID)
        .def_readonly("is_touch_range", &Skill::IsTouchRange)
        .def_readonly("is_elite", &Skill::IsElite)
        .def_readonly("is_half_range", &Skill::IsHalfRange)
        .def_readonly("is_pvp", &Skill::IsPvP)
        .def_readonly("is_pve", &Skill::IsPvE)
        .def_readonly("is_playable", &Skill::IsPlayable)
        .def_readonly("is_stacking", &Skill::IsStacking)
        .def_readonly("is_non_stacking", &Skill::IsNonStacking)
        .def_readonly("is_unused", &Skill::IsUnused)
        .def_readonly("adrenaline_a", &Skill::adrenaline_a)
        .def_readonly("adrenaline_b", &Skill::adrenaline_b)
        .def_readonly("recharge2", &Skill::recharge)
		.def_readonly("h0004", &Skill::h0004)
		.def_readonly("h0032", &Skill::h0032)
		.def_readonly("h0037", &Skill::h0037);
}

PYBIND11_EMBEDDED_MODULE(PySkill, m) {
    bind_SkillID(m);
    bind_SkillType(m);
    bind_Skill(m);
}

