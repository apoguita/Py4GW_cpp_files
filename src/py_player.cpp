#include "py_player.h"


namespace py = pybind11;

class PyTitle {
public:
    uint32_t title_id = 0;
    uint32_t props = 0;
    uint32_t current_points = 0;
    uint32_t current_title_tier_index = 0;
    uint32_t points_needed_current_rank = 0;
    uint32_t next_title_tier_index = 0;
    uint32_t points_needed_next_rank = 0;
    uint32_t max_title_rank = 0;
    uint32_t max_title_tier_index = 0;
    bool is_percentage_based = false;
    bool has_tiers = false;

    PyTitle(uint32_t title) {
        title_id = title;
        GetContext();
    }

    void GetContext() {
        auto title = GW::PlayerMgr::GetTitleTrack(static_cast<GW::Constants::TitleID>(title_id));
        if (title) {
			props = title->props;
			current_points = title->current_points;
			current_title_tier_index = title->current_title_tier_index;
			points_needed_current_rank = title->points_needed_current_rank;
			next_title_tier_index = title->next_title_tier_index;
			points_needed_next_rank = title->points_needed_next_rank;
			max_title_rank = title->max_title_rank;
			max_title_tier_index = title->max_title_tier_index;
			is_percentage_based = title->is_percentage_based();
			has_tiers = title->has_tiers();          
        }
    }
};

PyPlayer::PyPlayer() {
    GetContext();
}

void PyPlayer::GetContext() {
    id = static_cast<int>(GW::Agents::GetControlledCharacterId()); 
    agent = id;
    target_id = static_cast<int>(GW::Agents::GetTargetId());  
    //mouse_over_id = static_cast<int>(GW::Agents::GetMouseoverId());  
    mouse_over_id = 0; 
    observing_id = static_cast<int>(GW::Agents::GetObservingId()); 

    if (GW::Map::GetIsMapLoaded()) {
        GW::WorldContext* world_context = GW::GetWorldContext();
        if (world_context) {
            wchar_t*  waccount_name = world_context->accountInfo->account_name;
            account_name = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(waccount_name);
            wins = world_context->accountInfo->wins;
            losses = world_context->accountInfo->losses;
            rating = world_context->accountInfo->rating;
            qualifier_points = world_context->accountInfo->qualifier_points;
            rank = world_context->accountInfo->rank;
            tournament_reward_points = world_context->accountInfo->tournament_reward_points;
            morale = world_context->morale;
            experience = world_context->experience;
            current_kurzick = world_context->current_kurzick;
            total_earned_kurzick = world_context->total_earned_kurzick;
            max_kurzick = world_context->max_kurzick;
            current_luxon = world_context->current_luxon;
            total_earned_luxon = world_context->total_earned_luxon;
            max_luxon = world_context->max_luxon;
            current_imperial = world_context->current_imperial;
            total_earned_imperial = world_context->total_earned_imperial;
            max_imperial = world_context->max_imperial;
            current_balth = world_context->current_balth;
            total_earned_balth = world_context->total_earned_balth;
            max_balth = world_context->max_balth;
            current_skill_points = world_context->current_skill_points;
            total_earned_skill_points = world_context->total_earned_skill_points;
        }
    }

}

std::vector<int> PyPlayer::GetAgentArray() {
	std::vector<int> agent_ids = {};

    const auto agents = GW::Agents::GetAgentArray();
    if (!agents) return agent_ids;

    for (const GW::Agent* agent : *agents) {
        if (agent && agent->agent_id) {
            agent_ids.push_back(static_cast<int>(agent->agent_id));
        }
    }
    if (agent_ids.empty()) {
        agent_ids.push_back(static_cast<int>(0));
    }
    return agent_ids;
}

std::vector<int> PyPlayer::GetAllyArray() {
    std::vector<int> agent_ids = {};
    
    const auto agents = GW::Agents::GetAgentArray();
    if (!agents) return agent_ids;

    for (const GW::Agent* agent : *agents) {

        int ally_id = 0;  
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto living_agent = agent->GetAsAgentLiving();  
        if (!living_agent) { continue; }
        if (living_agent->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) { continue; }

        ally_id = living_agent->agent_id;  
        agent_ids.push_back(static_cast<int>(ally_id));  
    }

	if (agent_ids.empty()) {
        agent_ids.push_back(static_cast<int>(0));
	}
    return agent_ids;
}

std::vector<int> PyPlayer::GetNeutralArray() {
	std::vector<int> agent_ids = {};

    const auto agents = GW::Agents::GetAgentArray();
    if (!agents) return agent_ids;

    for (const GW::Agent* agent : *agents) {
        int neutral_id = 0;  // Changed AllyID to neutral_id
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto living_agent = agent->GetAsAgentLiving();  // Changed tAllyLiving to living_agent
        if (!living_agent) { continue; }
        if (living_agent->allegiance != GW::Constants::Allegiance::Neutral) { continue; }

        neutral_id = living_agent->agent_id;  // Changed tAllyLiving to living_agent and AllyID to neutral_id
        agent_ids.push_back(static_cast<int>(neutral_id));  // Changed AllyID to neutral_id
    }
    if (agent_ids.empty()) {
        agent_ids.push_back(static_cast<int>(0));
    }
    return agent_ids;
}


std::vector<int> PyPlayer::GetEnemyArray() {
	std::vector<int> agent_ids = {};

    const auto agents = GW::Agents::GetAgentArray();
    if (!agents) return agent_ids;

    for (const GW::Agent* agent : *agents) {

        int enemy_id = 0;  // Changed AllyID to enemy_id
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto living_agent = agent->GetAsAgentLiving();  // Changed tAllyLiving to living_agent
        if (!living_agent) { continue; }
        if (living_agent->allegiance != GW::Constants::Allegiance::Enemy) { continue; }

        enemy_id = living_agent->agent_id;  // Changed AllyID to enemy_id
        agent_ids.push_back(static_cast<int>(enemy_id));  // Changed AllyID to enemy_id
    }
    if (agent_ids.empty()) {
        agent_ids.push_back(static_cast<int>(0));
    }
    return agent_ids;
}

std::vector<int> PyPlayer::GetSpiritPetArray() {
	std::vector<int> agent_ids = {};

    const auto agents = GW::Agents::GetAgentArray();
    if (!agents) return agent_ids;

    for (const GW::Agent* agent : *agents) {

        int spirit_pet_id = 0;  // Changed AllyID to spirit_pet_id
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto living_agent = agent->GetAsAgentLiving();  // Changed tAllyLiving to living_agent
        if (!living_agent) { continue; }
        if (living_agent->allegiance != GW::Constants::Allegiance::Spirit_Pet) { continue; }

        spirit_pet_id = living_agent->agent_id;  // Changed AllyID to spirit_pet_id
        agent_ids.push_back(static_cast<int>(spirit_pet_id));  // Changed AllyID to spirit_pet_id
    }
    if (agent_ids.empty()) {
        agent_ids.push_back(static_cast<int>(0));
    }
    return agent_ids;
}

std::vector<int> PyPlayer::GetMinionArray() {
	std::vector<int> agent_ids = {};

    const auto agents = GW::Agents::GetAgentArray();
    if (!agents) return agent_ids;

    for (const GW::Agent* agent : *agents) {

        int minion_id = 0;  // Changed AllyID to minion_id
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto living_agent = agent->GetAsAgentLiving();  // Changed tAllyLiving to living_agent
        if (!living_agent) { continue; }
        if (living_agent->allegiance != GW::Constants::Allegiance::Minion) { continue; }

        minion_id = living_agent->agent_id;  // Changed AllyID to minion_id
        agent_ids.push_back(static_cast<int>(minion_id));  // Changed AllyID to minion_id
    }
    if (agent_ids.empty()) {
        agent_ids.push_back(static_cast<int>(0));
    }
    return agent_ids;
}

std::vector<int> PyPlayer::GetNPCMinipetArray() {
	std::vector<int> agent_ids = {};

    const auto agents = GW::Agents::GetAgentArray();
    if (!agents) return agent_ids;

    for (const GW::Agent* agent : *agents) {

        int npc_minipet_id = 0;  // Changed AllyID to npc_minipet_id
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto living_agent = agent->GetAsAgentLiving();  // Changed tAllyLiving to living_agent
        if (!living_agent) { continue; }
        if (living_agent->allegiance != GW::Constants::Allegiance::Npc_Minipet) { continue; }

        npc_minipet_id = living_agent->agent_id;  // Changed AllyID to npc_minipet_id
        agent_ids.push_back(static_cast<int>(npc_minipet_id));  // Changed AllyID to npc_minipet_id
    }
    if (agent_ids.empty()) {
        agent_ids.push_back(static_cast<int>(0));
    }
    return agent_ids;
}

std::vector<int> PyPlayer::GetItemArray() {
	std::vector<int> agent_ids = {};
    const auto agents = GW::Agents::GetAgentArray();
    if (!agents) return agent_ids;  // Return an empty vector if there are no agents

    for (const GW::Agent* agent : *agents) {
        if (!(agent && agent->GetIsItemType())) continue;  // Skip non-item agents

        const auto agent_item = agent->GetAsAgentItem();
        if (!(agent_item && agent_item->agent_id)) continue;  // Skip invalid item agents

        uint32_t item_id = agent_item->agent_id;  // Get the item agent ID

		if (item_id == 0) continue;  // Skip invalid item IDs

        // Add the valid item ID to the list
        agent_ids.push_back(static_cast<int>(item_id));
    }
    return agent_ids;
}

std::vector<int> PyPlayer::GetOwnedItemArray(int owner_agent_id) {
    std::vector<int> agent_ids = {};
	GW::Agent* agent = GW::Agents::GetAgentByID(owner_agent_id);
	if (!agent) return agent_ids;  // Return an empty vector if the agent is invalid
    
    const auto agents = GW::Agents::GetAgentArray();
    if (!agents) return agent_ids;  // Return an empty vector if there are no agents

    for (const GW::Agent* agent : *agents) {
        if (!(agent && agent->GetIsItemType())) continue;  // Skip non-item agents

        const auto agent_item = agent->GetAsAgentItem();
        if (!(agent_item && agent_item->agent_id)) continue;  // Skip invalid item agents
        
		if (agent_item->owner == owner_agent_id)
			agent_ids.push_back(static_cast<int>(agent_item->agent_id));
       
    }
    return agent_ids;
}

std::vector<int> PyPlayer::GetGadgetArray() {
	std::vector<int> agent_ids = {};
    const auto agents = GW::Agents::GetAgentArray();
    if (!agents) return agent_ids;  // Check if the agents array is null

    for (const GW::Agent* agent : *agents) {
        if (!agent || !agent->GetIsGadgetType()) continue;  // Skip null and non-gadget agents

        const auto agent_gadget = agent->GetAsAgentGadget();
        if (!agent_gadget || !agent_gadget->agent_id) continue;  // Skip invalid gadgets

        agent_ids.push_back(static_cast<int>(agent_gadget->agent_id));  // Add valid gadget ID
    }
    if (agent_ids.empty()) {
        agent_ids.push_back(static_cast<int>(0));
    }
    return agent_ids;
}

std::string local_player_WStringToString(const std::wstring& s) {
    // @Cleanup: ASSERT used incorrectly here; value passed could be from anywhere!
    if (s.empty()) {
        return "Error In Wstring";
    }
    // NB: GW uses code page 0 (CP_ACP)
    const auto size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), strTo.data(), size_needed, NULL, NULL);
    // Remove the trailing null character added by WideCharToMultiByte
    strTo.resize(size_needed - 1);

    // **Regex to strip tags like <quote>, <a=1>, <tag>**
    static const std::regex tagPattern(R"(<[^<>]+?>)");
    return std::regex_replace(strTo, tagPattern, "");  // ✅ Removes all tags
}


// Global variables (same as original implementation)
static std::vector<std::string> global_chat_messages;
static bool chat_log_ready = false;

void PyPlayer::RequestChatHistory() {
    chat_log_ready = false;
	global_chat_messages = {};

    std::thread([]() {
        const GW::Chat::ChatBuffer* log = GW::Chat::GetChatLog();
        if (!log) {
            chat_log_ready = true;  // No chat log available, mark as done
            return;
        }

        std::vector<std::wstring> temp_chat_log;

        // Read the entire chat log
        for (size_t i = 0; i < GW::Chat::CHAT_LOG_LENGTH; i++) {
            if (log->messages[i]) {
                temp_chat_log.push_back(log->messages[i]->message);
            }
        }

        std::vector<std::wstring> decoded_chat(temp_chat_log.size());
        auto start_time = std::chrono::steady_clock::now();

        // Request decoding inside the game thread
        GW::GameThread::Enqueue([temp_chat_log, &decoded_chat]() {
            for (size_t i = 0; i < temp_chat_log.size(); i++) {
                GW::UI::AsyncDecodeStr(temp_chat_log[i].c_str(), &decoded_chat[i]);
            }
            });

        // Wait for all messages to be decoded (max 1000ms timeout)
        for (size_t i = 0; i < temp_chat_log.size(); i++) {
            while (decoded_chat[i].empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() >= 500) {
                    decoded_chat[i] = L"[ERROR: Timeout]";  // Mark as failed
                    break;
                }
            }
        }

        // Convert all messages to UTF-8
        std::vector<std::string> converted_chat;
        for (const auto& decoded_msg : decoded_chat) {
            converted_chat.push_back(local_player_WStringToString(decoded_msg));
        }

        // Now safely update the global chat log
        global_chat_messages = std::move(converted_chat);
        chat_log_ready = true;  // Mark chat log as ready

        }).detach();  // Fully detach the thread so it does not block anything
}

bool PyPlayer::IsChatHistoryReady() {
    return chat_log_ready;
}

std::vector<std::string> PyPlayer::GetChatHistory() {
    return global_chat_messages;
}





void PyPlayer::SendChatCommand(std::string msg) {
    GW::Chat::SendChat('/', msg.c_str());
}

void PyPlayer::SendChat(char channel, std::string msg) {
    GW::Chat::SendChat(channel, msg.c_str());
}

void PyPlayer::SendWhisper(std::string name, std::string msg) {
    GW::Chat::SendChat(name.c_str(), msg.c_str());
}

bool PyPlayer::ChangeTarget(uint32_t new_target_id) {
	if (new_target_id == 0) return false;

    auto agent = GW::Agents::GetAgentByID(new_target_id);
    if (!agent) return false;

    GW::GameThread::Enqueue([agent] {
        GW::Agents::ChangeTarget(agent);;
        });
    return true;
}

bool PyPlayer::Move(float x, float y, int zplane) {
    GW::GameThread::Enqueue([x,y,zplane] {
        GW::Agents::Move(x, y, zplane);
        });
    return true;
}

bool PyPlayer::Move(float x, float y) {
    GW::GamePos pos;
    pos.x = x;
    pos.y = y;

    GW::GameThread::Enqueue([pos] {
        GW::Agents::Move(pos);
        });
    return true;
}

bool PyPlayer::InteractAgent(int agent_id, bool call_target) {
	if (agent_id == 0) return false;

    GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
    if (!agent) return false;
    GW::GameThread::Enqueue([agent, call_target] {
        GW::Agents::InteractAgent(agent, call_target);;
        });
    return true;
}

bool PyPlayer::OpenLockedChest(bool use_key) {
    //return GW::Items::OpenLockedChest(use_key);
    return false;
}

void PyPlayer::SendDialog(uint32_t dialog_id) {
    GW::Agents::SendDialog(dialog_id);
}

bool PyPlayer::SetActiveTitle(uint32_t title_id) {
    GW::GameThread::Enqueue([title_id] {
	    GW::PlayerMgr::SetActiveTitle(static_cast<GW::Constants::TitleID>(title_id));
        });
    return true;
}

bool PyPlayer::RemoveActiveTitle() {
	return GW::PlayerMgr::RemoveActiveTitle();
}

bool PyPlayer::DepositFaction(uint32_t allegiance) {
	return GW::PlayerMgr::DepositFaction(allegiance);
}

uint32_t PyPlayer::GetActiveTitleId() {
	return static_cast<uint32_t>(GW::PlayerMgr::GetActiveTitleId());
}

bool PyPlayer::IsAgentIDValid(int agent_id) {
	const auto agent = GW::Agents::GetAgentByID(agent_id);
	if (!agent) return false;
	return true;
}

void BindPyTitle(py::module_& m) {
    py::class_<PyTitle>(m, "PyTitle")
        .def(py::init<uint32_t>(), py::arg("title_id"))  // Constructor with title_id
        .def("GetContext", &PyTitle::GetContext)  // Method to populate the title context

        // Bind the public attributes with snake_case naming convention
        .def_readonly("title_id", &PyTitle::title_id)
        .def_readonly("props", &PyTitle::props)
        .def_readonly("current_points", &PyTitle::current_points)
        .def_readonly("current_title_tier_index", &PyTitle::current_title_tier_index)
        .def_readonly("points_needed_current_rank", &PyTitle::points_needed_current_rank)
        .def_readonly("next_title_tier_index", &PyTitle::next_title_tier_index)
        .def_readonly("points_needed_next_rank", &PyTitle::points_needed_next_rank)
        .def_readonly("max_title_rank", &PyTitle::max_title_rank)
        .def_readonly("max_title_tier_index", &PyTitle::max_title_tier_index)
        .def_readonly("is_percentage_based", &PyTitle::is_percentage_based)
        .def_readonly("has_tiers", &PyTitle::has_tiers);
}

void BindPyPlayer(py::module_& m) {
    py::class_<PyPlayer>(m, "PyPlayer")
        .def(py::init<>())  // Bind the constructor
        .def("GetContext", &PyPlayer::GetContext)  // Bind the GetContext method
        .def("GetAgentArray", &PyPlayer::GetAgentArray)  // Bind the GetAgentArray method
        .def("GetAllyArray", &PyPlayer::GetAllyArray)  // Bind the GetAllyArray method
        .def("GetNeutralArray", &PyPlayer::GetNeutralArray)  // Bind the GetNeutralArray method
        .def("GetEnemyArray", &PyPlayer::GetEnemyArray)  // Bind the GetEnemyArray method
        .def("GetSpiritPetArray", &PyPlayer::GetSpiritPetArray)  // Bind the GetSpiritPetArray method
        .def("GetMinionArray", &PyPlayer::GetMinionArray)  // Bind the GetMinionArray method
        .def("GetNPCMinipetArray", &PyPlayer::GetNPCMinipetArray)  // Bind the GetNPCMinipetArray method
        .def("GetItemArray", &PyPlayer::GetItemArray)  // Bind the GetItemArray method
		.def("GetOwnedItemArray", &PyPlayer::GetOwnedItemArray, py::arg("owner_agent_id"))  // Bind the GetOwnedItemArray method
        .def("GetGadgetArray", &PyPlayer::GetGadgetArray)  // Bind the GetGadgetArray method
		.def("IsAgentIDValid", &PyPlayer::IsAgentIDValid, py::arg("agent_id"))  // Bind the IsAgentIDValid method
		.def("GetChatHistory", &PyPlayer::GetChatHistory)  // Bind the GetChatHistory method
		.def("RequestChatHistory", &PyPlayer::RequestChatHistory)  // Bind the RequestChatHistory method
		.def("IsChatHistoryReady", &PyPlayer::IsChatHistoryReady)  // Bind the IsChatHistoryReady method
        .def("SendChatCommand", &PyPlayer::SendChatCommand, py::arg("msg"))  // Bind the SendChatCommand method
        .def("SendChat", &PyPlayer::SendChat, py::arg("channel"), py::arg("msg"))  // Bind the SendChat method
        .def("SendWhisper", &PyPlayer::SendWhisper, py::arg("name"), py::arg("msg"))  // Bind the SendWhisper method
        .def("ChangeTarget", &PyPlayer::ChangeTarget, py::arg("target_id"))  // Bind the ChangeTarget method
        .def("Move", py::overload_cast<float, float, int>(&PyPlayer::Move), py::arg("x"), py::arg("y"), py::arg("zplane"))  // Bind the Move method (with zplane)
        .def("Move", py::overload_cast<float, float>(&PyPlayer::Move), py::arg("x"), py::arg("y"))  // Bind the Move method (without zplane)
        .def("InteractAgent", &PyPlayer::InteractAgent, py::arg("agent_id"), py::arg("call_target"))  // Bind the InteractAgent method
        .def("OpenLockedChest", &PyPlayer::OpenLockedChest, py::arg("use_key"))  // Bind the OpenLockedChest method
        .def("SendDialog", &PyPlayer::SendDialog, py::arg("dialog_id"))  // Bind the SendDialog method
		.def("SetActiveTitle", &PyPlayer::SetActiveTitle, py::arg("title_id"))  // Bind the SetActiveTitle method
		.def("RemoveActiveTitle", &PyPlayer::RemoveActiveTitle)  // Bind the RemoveActiveTitle method
		.def("DepositFaction", &PyPlayer::DepositFaction, py::arg("allegiance"))  // Bind the DepositFaction method
		.def("GetActiveTitleId", &PyPlayer::GetActiveTitleId)  // Bind the GetActiveTitleId method

        // Bind public attributes with snake_case naming convention
        .def_readonly("id", &PyPlayer::id)  // Bind the id attribute
        .def_readonly("agent", &PyPlayer::agent)  // Bind the agent attribute
        .def_readonly("target_id", &PyPlayer::target_id)  // Bind the target_id attribute
        .def_readonly("mouse_over_id", &PyPlayer::mouse_over_id)  // Bind the mouse_over_id attribute
        .def_readonly("observing_id", &PyPlayer::observing_id)  // Bind the observing_id attribute
	    .def_readonly("account_name", &PyPlayer::account_name) // Bind the account_name attribute
	    .def_readonly("wins", &PyPlayer::wins)  // Bind the wins attribute
	    .def_readonly("losses", &PyPlayer::losses)  // Bind the losses attribute
	    .def_readonly("rating", &PyPlayer::rating)  // Bind the rating attribute
	    .def_readonly("qualifier_points", &PyPlayer::qualifier_points)  // Bind the qualifier_points attribute
	    .def_readonly("rank", &PyPlayer::rank)  // Bind the rank attribute
	    .def_readonly("tournament_reward_points", &PyPlayer::tournament_reward_points)  // Bind the tournament_reward_points attribute
	    .def_readonly("morale", &PyPlayer::morale)  // Bind the morale attribute
	    .def_readonly("experience", &PyPlayer::experience)  // Bind the experience attribute
	    .def_readonly("current_kurzick", &PyPlayer::current_kurzick)  // Bind the current_kurzick attribute
	    .def_readonly("total_earned_kurzick", &PyPlayer::total_earned_kurzick)  // Bind the total_earned_kurzick attribute
	    .def_readonly("max_kurzick", &PyPlayer::max_kurzick) // Bind the max_kurzick attribute
	    .def_readonly("current_luxon", &PyPlayer::current_luxon)  // Bind the current_luxon attribute
	    .def_readonly("total_earned_luxon", &PyPlayer::total_earned_luxon)  // Bind the total_earned_luxon attribute
	    .def_readonly("max_luxon", &PyPlayer::max_luxon)  // Bind the max_luxon attribute
	    .def_readonly("current_imperial", &PyPlayer::current_imperial)  // Bind the current_imperial attribute
	    .def_readonly("total_earned_imperial", &PyPlayer::total_earned_imperial)  // Bind the total_earned_imperial attribute
	    .def_readonly("max_imperial", &PyPlayer::max_imperial)  // Bind the max_imperial attribute
	    .def_readonly("current_balth", &PyPlayer::current_balth)  // Bind the current_balth attribute
	    .def_readonly("total_earned_balth", &PyPlayer::total_earned_balth)  // Bind the total_earned_balth attribute
	    .def_readonly("max_balth", &PyPlayer::max_balth)  // Bind the max_balth attribute
	    .def_readonly("current_skill_points", &PyPlayer::current_skill_points)  // Bind the current_skill_points attribute
	    .def_readonly("total_earned_skill_points", &PyPlayer::total_earned_skill_points);  // Bind the total_earned_skill_points attribute
}

PYBIND11_EMBEDDED_MODULE(PyPlayer, m) {
	BindPyTitle(m);
    BindPyPlayer(m);
}

