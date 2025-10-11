#pragma once
#include "py_party.h"

namespace py = pybind11;

const std::unordered_map<int, std::pair<std::string, int>> Hero::hero_data = {
    {0, {"None", static_cast<int>(ProfessionType::None)}},
    {1, {"Norgu", static_cast<int>(ProfessionType::Mesmer)}},
    {2, {"Goren", static_cast<int>(ProfessionType::Warrior)}},
    {3, {"Tahlkora", static_cast<int>(ProfessionType::Monk)}},
    {4, {"Master Of Whispers", static_cast<int>(ProfessionType::Necromancer)}},
    {5, {"Acolyte Jin", static_cast<int>(ProfessionType::Ranger)}},
    {6, {"Koss", static_cast<int>(ProfessionType::Warrior)}},
    {7, {"Dunkoro", static_cast<int>(ProfessionType::Monk)}},
    {8, {"Acolyte Sousuke", static_cast<int>(ProfessionType::Elementalist)}},
    {9, {"Melonni", static_cast<int>(ProfessionType::Dervish)}},
    {10, {"Zhed Shadowhoof", static_cast<int>(ProfessionType::Elementalist)}},
    {11, {"General Morgahn", static_cast<int>(ProfessionType::Paragon)}},
    {12, {"Magrid The Sly", static_cast<int>(ProfessionType::Ranger)}},
    {13, {"Zenmai", static_cast<int>(ProfessionType::Assassin)}},
    {14, {"Olias", static_cast<int>(ProfessionType::Necromancer)}},
    {15, {"Razah", static_cast<int>(ProfessionType::None)}},
    {16, {"M.O.X.", static_cast<int>(ProfessionType::Dervish)}},
    {17, {"Keiran Thackeray", static_cast<int>(ProfessionType::Paragon)}},
    {18, {"Jora", static_cast<int>(ProfessionType::Warrior)}},
    {19, {"Pyre Fierceshot", static_cast<int>(ProfessionType::Ranger)}},
    {20, {"Anton", static_cast<int>(ProfessionType::Assassin)}},
    {21, {"Livia", static_cast<int>(ProfessionType::Necromancer)}},
    {22, {"Hayda", static_cast<int>(ProfessionType::Paragon)}},
    {23, {"Kahmu", static_cast<int>(ProfessionType::Dervish)}},
    {24, {"Gwen", static_cast<int>(ProfessionType::Mesmer)}},
    {25, {"Xandra", static_cast<int>(ProfessionType::Ritualist)}},
    {26, {"Vekk", static_cast<int>(ProfessionType::Elementalist)}},
    {27, {"Ogden Stonehealer", static_cast<int>(ProfessionType::Monk)}},
    {28, {"Mercenary Hero 1", static_cast<int>(ProfessionType::None)}},
    {29, {"Mercenary Hero 2", static_cast<int>(ProfessionType::None)}},
    {30, {"Mercenary Hero 3", static_cast<int>(ProfessionType::None)}},
    {31, {"Mercenary Hero 4", static_cast<int>(ProfessionType::None)}},
    {32, {"Mercenary Hero 5", static_cast<int>(ProfessionType::None)}},
    {33, {"Mercenary Hero 6", static_cast<int>(ProfessionType::None)}},
    {34, {"Mercenary Hero 7", static_cast<int>(ProfessionType::None)}},
    {35, {"Mercenary Hero 8", static_cast<int>(ProfessionType::None)}},
    {36, {"Miku", static_cast<int>(ProfessionType::Assassin)}},
    {37, {"Zei Ri", static_cast<int>(ProfessionType::Ritualist)}}
};



// Constructor with hero_id
Hero::Hero(int hero_id) {
    auto it = hero_data.find(hero_id);
    if (it != hero_data.end()) {
        this->hero_id = static_cast<HeroType>(hero_id);
        hero_name = it->second.first;
        hero_profession = Profession(static_cast<int>(it->second.second));
    }
    else {
        this->hero_id = HeroType::None;
        hero_name = "Unknown";
        hero_profession = Profession(static_cast<int>(ProfessionType::None));
    }
}

// Constructor with hero_name
Hero::Hero(const std::string& hero_name) {
    for (const auto& it : hero_data) {
        if (it.second.first == hero_name) {
            this->hero_id = static_cast<HeroType>(it.first);
            this->hero_name = it.second.first;
            this->hero_profession = Profession(it.second.second);
            return;
        }
    }
    this->hero_id = HeroType::None;
    this->hero_name = "Unknown";
    this->hero_profession = Profession(static_cast<int>(ProfessionType::None));
}

// Get hero ID
int Hero::GetId() const {
    return static_cast<int>(hero_id);
}

// Get hero name
std::string Hero::GetName() const {
    return hero_name;
}

// Get hero profession
Profession Hero::GetProfession() const {
    return hero_profession;
}

FlaggingState Hero::GetFlaggingState()
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        return def_readonly;
    }
    if (!CaptureMouseClickTypePtr || *CaptureMouseClickTypePtr != CaptureType_FlagHero || !MouseClickCaptureDataPtr || !MouseClickCaptureDataPtr->sub1) {
        return def_readonly;
    }
    return *MouseClickCaptureDataPtr->sub1->sub2->sub3->sub4->sub5->flagging_hero;
}

bool Hero::SetFlaggingState(FlaggingState set_state)
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        return false;
    }
    // keep an internal flag for the minimap flagging until StartCaptureMouseClick_Func is working
    // minimap_flagging_state = set_state;
    if (GetFlaggingState() == set_state) { return true; }
    if (set_state == def_readonly) {
        set_state = GetFlaggingState();
    }
    GW::UI::ControlAction key;
    switch (set_state) {
    case FlagState_Hero1:
        key = GW::UI::ControlAction_CommandHero1;
        break;
    case FlagState_Hero2:
        key = GW::UI::ControlAction_CommandHero2;
        break;
    case FlagState_Hero3:
        key = GW::UI::ControlAction_CommandHero3;
        break;
    case FlagState_Hero4:
        key = GW::UI::ControlAction_CommandHero4;
        break;
    case FlagState_Hero5:
        key = GW::UI::ControlAction_CommandHero5;
        break;
    case FlagState_Hero6:
        key = GW::UI::ControlAction_CommandHero6;
        break;
    case FlagState_Hero7:
        key = GW::UI::ControlAction_CommandHero7;
        break;
    case FlagState_All:
        key = GW::UI::ControlAction_CommandParty;
        break;
    default:
        return false;
    }
    return Keypress(key);
}

bool Hero::IsHeroFlagged(int hero) {
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

    return false;

}

bool Hero::FlagHero(uint32_t idx)
{
    if (idx > def_readonly) {
        return false;
    }
    return SetFlaggingState(static_cast<FlaggingState>(idx));
}


void Hero::UnflagAllHeroes() {

    GW::PartyMgr::UnflagAll();

    for (int i = 1; i <= GW::PartyMgr::GetPartyHeroCount(); i++) {
        GW::PartyMgr::UnflagHero(i);
    }

}




PyParty::PyParty() {
    GetContext();
}

void PyParty::ResetContext() {
	party_id = 0;
	players.clear();
	heroes.clear();
	henchmen.clear();
	is_in_hard_mode = false;  // Changed to snake_case
	is_hard_mode_unlocked = false;  // Changed to snake_case
	party_size = 0;  // Changed to snake_case
	party_player_count = 0;  // Changed to snake_case
	party_hero_count = 0;  // Changed to snake_case
	party_henchman_count = 0;  // Changed to snake_case
	is_party_defeated = false;  // Changed to snake_case
	is_party_loaded = false;  // Changed to snake_case
	is_party_leader = false;  // Changed to snake_case
	tick = false;  // Changed to snake_case
}


void PyParty::GetContext() {

    auto instance_type = GW::Map::GetInstanceType();
    bool is_map_ready = (GW::Map::GetIsMapLoaded()) && (!GW::Map::GetIsObserving()) && (instance_type != GW::Constants::InstanceType::Loading);

    if (!is_map_ready) {
        ResetContext();
        return;
    }

    is_party_loaded = GW::PartyMgr::GetIsPartyLoaded();

	if (!is_party_loaded) {
		ResetContext();
		return;
	}

    
    tick = GW::PartyMgr::GetIsPartyTicked();  // Changed to snake_case

    GW::PartyInfo* party_info = GW::PartyMgr::GetPartyInfo();

    if (!party_info) return;

    party_id = party_info->party_id;  // Changed to snake_case

    // Loop through players
    players.clear();
    for (size_t i = 0; i < party_info->players.size(); ++i) {
        const GW::PlayerPartyMember& player = party_info->players[i];
        players.emplace_back(player.login_number, player.calledTargetId, player.connected(), player.ticked());
    }

    // Loop through heroes
    heroes.clear();
    for (size_t i = 0; i < party_info->heroes.size(); ++i) {
        const GW::HeroPartyMember& hero = party_info->heroes[i];
        heroes.emplace_back(hero.agent_id, hero.owner_player_id, hero.hero_id, hero.level);
    }

    // Loop through henchmen
    henchmen.clear();
    for (size_t i = 0; i < party_info->henchmen.size(); ++i) {
        const GW::HenchmanPartyMember& henchman = party_info->henchmen[i];
        henchmen.emplace_back(henchman.agent_id, henchman.profession, henchman.level);
    }

	others.clear();
    for (size_t i = 0; i < party_info->others.size(); ++i) {
        const GW::AgentID& p_others = party_info->others[i];
		others.emplace_back(static_cast<uint32_t>(p_others));
		
	}

    is_in_hard_mode = GW::PartyMgr::GetIsPartyInHardMode();  
    is_hard_mode_unlocked = GW::PartyMgr::GetIsHardModeUnlocked();  
    party_size = GW::PartyMgr::GetPartySize();  
    party_player_count = GW::PartyMgr::GetPartyPlayerCount(); 
    party_hero_count = GW::PartyMgr::GetPartyHeroCount();  
    party_henchman_count = GW::PartyMgr::GetPartyHenchmanCount();  
    is_party_defeated = GW::PartyMgr::GetIsPartyDefeated();   
    is_party_leader = GW::PartyMgr::GetIsLeader(); 
}

bool PyParty::ReturnToOutpost() {
    return GW::PartyMgr::ReturnToOutpost();
}

bool PyParty::SetHardMode(bool flag) {
    return GW::PartyMgr::SetHardMode(flag);
}

bool PyParty::RespondToPartyRequest(int new_party_id, bool accept)
{
    return GW::PartyMgr::RespondToPartyRequest(new_party_id, accept);
}

bool PyParty::AddHero(int hero_id) {
    return GW::PartyMgr::AddHero(hero_id);  // Changed to snake_case
}

bool PyParty::KickHero(int hero_id) {
    return GW::PartyMgr::KickHero(hero_id);  // Changed to snake_case
}

bool PyParty::KickAllHeroes() {
    return GW::PartyMgr::KickAllHeroes();
}

bool PyParty::AddHenchman(int henchman_id) {
    return GW::PartyMgr::AddHenchman(henchman_id);  // Changed to snake_case
}

bool PyParty::KickHenchman(int henchman_id) {
    return GW::PartyMgr::KickHenchman(henchman_id);  // Changed to snake_case
}

bool PyParty::KickPlayer(int player_id) {
	auto player_name = GetPlayerNameByLoginNumber(player_id);
    return GW::PartyMgr::KickPlayer(player_id);  // Changed to snake_case
}

bool PyParty::InvitePlayer(int player_id) {
    return GW::PartyMgr::InvitePlayer(player_id);  // Changed to snake_case
}

bool PyParty::LeaveParty() {
    return GW::PartyMgr::LeaveParty();
}

bool PyParty::FlagHero(int agent_id, float x, float y) {
    GW::GamePos pos;
    pos.x = x;
    pos.y = y;
    return GW::PartyMgr::FlagHeroAgent(agent_id, pos);
}

bool PyParty::FlagAllHeroes(float x, float y) {
    GW::GamePos pos;
    pos.x = x;
    pos.y = y;
    return GW::PartyMgr::FlagAll(pos);
}

bool PyParty::UnflagHero(int agent_id) {
    return GW::PartyMgr::UnflagHero(agent_id);
}

bool PyParty::UnflagAllHeroes() {
    return GW::PartyMgr::UnflagAll();
}

bool PyParty::IsHeroFlagged(int hero) {
    Hero hero_handler = Hero(hero);
	return hero_handler.IsHeroFlagged(hero);
}

bool PyParty::IsAllFlagged() {
    const GW::Vec3f& allflag = GW::GetGameContext()->world->all_flag;
    if (allflag.x != 0 && allflag.y != 0 && (!std::isinf(allflag.x) || !std::isinf(allflag.y))) {
        return true;
    }
	return false;
}

float PyParty::GetAllFlagX() {
	return GW::GetGameContext()->world->all_flag.x;
}

float PyParty::GetAllFlagY() {
	return GW::GetGameContext()->world->all_flag.y;
}


int PyParty::GetHeroAgentID(int hero_index) {
    return GW::PartyMgr::GetHeroAgentID(hero_index);
}

int PyParty::GetAgentHeroID(int agent_id) {
    return GW::PartyMgr::GetAgentHeroID(agent_id);
}

int PyParty::GetAgentIDByLoginNumber(int login_number) {
    return GW::Agents::GetAgentIdByLoginNumber(login_number);
}

std::string PyParty::GetPlayerNameByLoginNumber(int login_number) {
    wchar_t* p_name = GW::Agents::GetPlayerNameByLoginNumber(login_number);
    std::wstring wide_name(p_name);
    auto name = std::string(wide_name.begin(), wide_name.end());
    return name;
}
bool PyParty::SearchParty(uint32_t search_type, std::string advertisement) {
    std::wstring wide_advertisement(advertisement.begin(), advertisement.end());
    return GW::PartyMgr::SearchParty(search_type, wide_advertisement.c_str());
}

bool PyParty::SearchPartyCancel() {
    return GW::PartyMgr::SearchPartyCancel();
}

bool PyParty::SearchPartyReply(bool accept) {
    return GW::PartyMgr::SearchPartyReply(accept);
}

void PyParty::SetHeroBehavior(int agent_id, int behaviour) {
    //Fight, Guard, AvoidCombat
    GW::PartyMgr::SetHeroBehavior(agent_id, static_cast<GW::HeroBehavior>(behaviour));
}

void PyParty::SetPetBehaviour(int behavior, int lock_target_id) {
    GW::PartyMgr::SetPetBehavior(static_cast<GW::HeroBehavior>(behavior), lock_target_id);
}




PetInfo PyParty::GetPetInfo(uint32_t owner_agent_id) {
    return PetInfo(owner_agent_id);
}

bool PyParty::GetIsPlayerTicked(int player_id) {
    return GW::PartyMgr::GetIsPlayerTicked(player_id);
}


void PyParty::UseHeroSkillInstant(uint32_t hero_id, uint32_t skill_slot, uint32_t target_id) {
    using namespace GW;

    static UseHeroSkillInstant_t UseHeroSkillInstant_Func = nullptr;
    if (!UseHeroSkillInstant_Func) {
        uintptr_t func_addr = GW::Scanner::Find("\xD9\x45\xE0\x8D\x45\xD8\xD9\x55\xE8", "xxxxxxxxx", -0x59);
		Logger::AssertAddress("UseHeroSkillInstant", func_addr);
        UseHeroSkillInstant_Func = (UseHeroSkillInstant_t)GW::Scanner::FunctionFromNearCall(func_addr);

        if (!UseHeroSkillInstant_Func) {
            return;
        }
    }

    GW::GameThread::Enqueue([hero_id, skill_slot, target_id] {
        // Call the function
		UseHeroSkillInstant_Func(hero_id,skill_slot, target_id);
     });

}




void bind_HeroType(py::module_& m) {
    py::enum_<HeroType>(m, "HeroType")
        .value("None", HeroType::None)
        .value("Norgu", HeroType::Norgu)
        .value("Goren", HeroType::Goren)
        .value("Tahlkora", HeroType::Tahlkora)
        .value("MasterOfWhispers", HeroType::MasterOfWhispers)
        .value("AcolyteJin", HeroType::AcolyteJin)
        .value("Koss", HeroType::Koss)
        .value("Dunkoro", HeroType::Dunkoro)
        .value("AcolyteSousuke", HeroType::AcolyteSousuke)
        .value("Melonni", HeroType::Melonni)
        .value("ZhedShadowhoof", HeroType::ZhedShadowhoof)
        .value("GeneralMorgahn", HeroType::GeneralMorgahn)
        .value("MagridTheSly", HeroType::MagridTheSly)
        .value("Zenmai", HeroType::Zenmai)
        .value("Olias", HeroType::Olias)
        .value("Razah", HeroType::Razah)
        .value("MOX", HeroType::MOX)
        .value("KeiranThackeray", HeroType::KeiranThackeray)
        .value("Jora", HeroType::Jora)
        .value("PyreFierceshot", HeroType::PyreFierceshot)
        .value("Anton", HeroType::Anton)
        .value("Livia", HeroType::Livia)
        .value("Hayda", HeroType::Hayda)
        .value("Kahmu", HeroType::Kahmu)
        .value("Gwen", HeroType::Gwen)
        .value("Xandra", HeroType::Xandra)
        .value("Vekk", HeroType::Vekk)
        .value("Ogden", HeroType::Ogden)
        .value("MercenaryHero1", HeroType::MercenaryHero1)
        .value("MercenaryHero2", HeroType::MercenaryHero2)
        .value("MercenaryHero3", HeroType::MercenaryHero3)
        .value("MercenaryHero4", HeroType::MercenaryHero4)
        .value("MercenaryHero5", HeroType::MercenaryHero5)
        .value("MercenaryHero6", HeroType::MercenaryHero6)
        .value("MercenaryHero7", HeroType::MercenaryHero7)
        .value("MercenaryHero8", HeroType::MercenaryHero8)
        .value("Miku", HeroType::Miku)
        .value("ZeiRi", HeroType::ZeiRi);
}


void bind_Hero(py::module_& m) {
    py::class_<Hero>(m, "Hero")  // Renamed SafeHeroClass to Hero
        .def(py::init<int>())  // Bind the constructor with hero_id
        .def(py::init<std::string>())  // Bind the constructor with hero_name
        .def("GetID", &Hero::GetId)  // Get HeroID
        .def("GetName", &Hero::GetName)  // Get HeroName
        .def("GetProfession", &Hero::GetProfession)  // Get HeroProfession
		.def("FlagHero", &Hero::FlagHero)  // Flag hero
        .def("__eq__", &Hero::operator==)  // Equality operator
        .def("__ne__", &Hero::operator!=)  // Inequality operator
        .def("__repr__",
            [](const Hero& h) {
                return "<Hero name='" + h.GetName() + "' id=" + std::to_string(h.GetId()) + ">";
            }
        );
}

void bind_PartyTick(py::module_& m) {
    py::class_<PartyTick>(m, "PartyTick")
        .def(py::init<bool>())  // Constructor with ticked
        .def("IsTicked", &PartyTick::IsTicked)  // Get tick status
        .def("SetTicked", &PartyTick::SetTicked)  // Set tick status
        .def("ToggleTicked", &PartyTick::ToggleTicked)  // Toggle tick status
        .def("SetTickToggle", &PartyTick::SetTickToggle);  // Set tick toggle status
}

void bind_PlayerPartyMember(py::module_& m) {
    py::class_<PlayerPartyMember>(m, "PlayerPartyMember")
        .def(py::init<int, int, bool, bool>())  // Constructor with player_id, target_id, connected, ticked
        .def_readwrite("login_number", &PlayerPartyMember::login_number)  // Player ID
        .def_readwrite("called_target_id", &PlayerPartyMember::called_target_id)  // Called Target ID
        .def_readwrite("is_connected", &PlayerPartyMember::is_connected)  // Connection status
        .def_readwrite("is_ticked", &PlayerPartyMember::is_ticked);  // Ticked status
}


void bind_HeroPartyMember(py::module_& m) {
    py::class_<HeroPartyMember>(m, "HeroPartyMember")
        .def(py::init<int, int, int, int>())  // Constructor
        .def_readwrite("agent_id", &HeroPartyMember::agent_id)  // Use snake_case for variables
        .def_readwrite("owner_player_id", &HeroPartyMember::owner_player_id)  // Use snake_case for variables
        .def_readwrite("hero_id", &HeroPartyMember::hero_id)  // Use snake_case for variables
        .def_readwrite("level", &HeroPartyMember::level)  // Use snake_case for variables
        .def_readwrite("primary", &HeroPartyMember::primary)  // Use snake_case for variables
        .def_readwrite("secondary", &HeroPartyMember::secondary);  // Use snake_case for variables
}

void bind_HenchmanPartyMember(py::module_& m) {
    py::class_<HenchmanPartyMember>(m, "HenchmanPartyMember")
        .def(py::init<int, int, int>())  // Constructor
        .def_readwrite("agent_id", &HenchmanPartyMember::agent_id)  // Use snake_case for variables
        .def_readwrite("profession", &HenchmanPartyMember::profession)  // Use snake_case for variables
        .def_readwrite("level", &HenchmanPartyMember::level);  // Use snake_case for variables
}

void BindPetInfo(py::module_& m) {
    py::class_<PetInfo>(m, "PetInfo")
        .def(py::init<uint32_t>(), py::arg("owner_agent_id"))
        .def_readonly("agent_id", &PetInfo::agent_id)
        .def_readonly("owner_agent_id", &PetInfo::owner_agent_id)
        .def_readonly("pet_name", &PetInfo::pet_name)
        .def_readonly("model_file_id1", &PetInfo::model_file_id1)
        .def_readonly("model_file_id2", &PetInfo::model_file_id2)
        .def_readonly("behavior", &PetInfo::behavior)
        .def_readonly("locked_target_id", &PetInfo::locked_target_id);
}


void bind_PyParty(py::module_& m) {
    py::class_<PyParty>(m, "PyParty")
        .def(py::init<>())  // Constructor
        .def("GetContext", &PyParty::GetContext)  // Bind GetContext method
        .def("ReturnToOutpost", &PyParty::ReturnToOutpost)  // Bind ReturnToOutpost method
        .def("SetHardMode", &PyParty::SetHardMode, py::arg("flag"))  // Bind SetHardMode method
        .def("RespondToPartyRequest", &PyParty::RespondToPartyRequest, py::arg("party_id"), py::arg("accept"))  // Bind RespondToPartyRequest method
        .def("AddHero", &PyParty::AddHero, py::arg("hero_id"))  // Bind AddHero method
        .def("KickHero", &PyParty::KickHero, py::arg("hero_id"))  // Bind KickHero method
        .def("KickAllHeroes", &PyParty::KickAllHeroes)  // Bind KickAllHeroes method
        .def("AddHenchman", &PyParty::AddHenchman, py::arg("henchman_id"))  // Bind AddHenchman method
        .def("KickHenchman", &PyParty::KickHenchman, py::arg("henchman_id"))  // Bind KickHenchman method
        .def("KickPlayer", &PyParty::KickPlayer, py::arg("player_id"))  // Bind KickPlayer method
        .def("InvitePlayer", &PyParty::InvitePlayer, py::arg("player_id"))  // Bind InvitePlayer method
        .def("LeaveParty", &PyParty::LeaveParty)  // Bind LeaveParty method
        .def("FlagHero", &PyParty::FlagHero, py::arg("agent_id"), py::arg("x"), py::arg("y"))  // Bind FlagHero method
        .def("FlagAllHeroes", &PyParty::FlagAllHeroes, py::arg("x"), py::arg("y"))  // Bind FlagAllHeroes method
        .def("UnflagHero", &PyParty::UnflagHero, py::arg("agent_id"))  // Bind UnflagHero method
        .def("UnflagAllHeroes", &PyParty::UnflagAllHeroes)  // Bind UnflagAllHeroes method
		.def("IsHeroFlagged", &PyParty::IsHeroFlagged, py::arg("hero"))  // Bind IsHeroFlagged method
		.def("IsAllFlagged", &PyParty::IsAllFlagged)  // Bind IsAllFlagged method
		.def("GetAllFlagX", &PyParty::GetAllFlagX)  // Bind GetAllFlagX method
		.def("GetAllFlagY", &PyParty::GetAllFlagY)  // Bind GetAllFlagY method
        .def("GetHeroAgentID", &PyParty::GetHeroAgentID, py::arg("hero_index"))  // Bind GetHeroAgentID method
        .def("GetAgentHeroID", &PyParty::GetAgentHeroID, py::arg("agent_id"))  // Bind GetAgentHeroID method
        .def("GetAgentIDByLoginNumber", &PyParty::GetAgentIDByLoginNumber, py::arg("login_number"))  
        .def("GetPlayerNameByLoginNumber", &PyParty::GetPlayerNameByLoginNumber, py::arg("login_number"))  // Bind GetPlayerNameByLoginNumber method
        .def("SearchParty", &PyParty::SearchParty, py::arg("search_type"), py::arg("advertisement"))  // Bind SearchParty method
        .def("SearchPartyCancel", &PyParty::SearchPartyCancel)  // Bind SearchPartyCancel method
        .def("SearchPartyReply", &PyParty::SearchPartyReply, py::arg("accept"))  // Bind SearchPartyReply method
        .def("SetHeroBehavior", &PyParty::SetHeroBehavior, py::arg("agent_id"), py::arg("behavior"))  // Bind SetHeroBehavior method
        .def("SetPetBehavior", &PyParty::SetPetBehaviour, py::arg("behaviour"), py::arg("lock_target_id"))  // Bind SetPetBehaviour method
        .def("GetPetInfo", &PyParty::GetPetInfo, py::arg("owner_agent_id")) // Bind GetPetInfo method
        .def("GetIsPlayerTicked", &PyParty::GetIsPlayerTicked)
		.def("UseHeroSkill", &PyParty::UseHeroSkillInstant, py::arg("hero_id"), py::arg("skill_slot"), py::arg("target_id"))  // Bind UseHeroSkillInstant method

        // Bind public attributes (use snake_case)
        .def_readwrite("party_id", &PyParty::party_id)
        .def_readwrite("players", &PyParty::players)
        .def_readwrite("heroes", &PyParty::heroes)
        .def_readwrite("henchmen", &PyParty::henchmen)
		.def_readwrite("others", &PyParty::others)  
        .def_readwrite("is_in_hard_mode", &PyParty::is_in_hard_mode) 
        .def_readwrite("is_hard_mode_unlocked", &PyParty::is_hard_mode_unlocked)
        .def_readwrite("party_size", &PyParty::party_size)
        .def_readwrite("party_player_count", &PyParty::party_player_count)
        .def_readwrite("party_hero_count", &PyParty::party_hero_count)
        .def_readwrite("party_henchman_count", &PyParty::party_henchman_count)
        .def_readwrite("is_party_defeated", &PyParty::is_party_defeated)
        .def_readwrite("is_party_loaded", &PyParty::is_party_loaded)
        .def_readwrite("is_party_leader", &PyParty::is_party_leader)
        .def_readwrite("tick", &PyParty::tick);  // snake_case for public attributes
}

PYBIND11_EMBEDDED_MODULE(PyParty, m) {
    bind_HeroType(m);  // Corrected SafeHeroType to HeroType as previously discussed
    bind_Hero(m);  // Corrected SafeHeroClass to Hero
    bind_PartyTick(m);
    bind_PlayerPartyMember(m);
    bind_HeroPartyMember(m);
    bind_HenchmanPartyMember(m);
    BindPetInfo(m);
    bind_PyParty(m);
}


