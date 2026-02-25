#include "py_player.h"

namespace py = pybind11;

PyPlayer::PyPlayer() {
    GetContext();
}
void PyPlayer::ResetContext() {
	id = 0;
	agent = 0;
	target_id = 0;
	mouse_over_id = 0;
	observing_id = 0;
	account_name = "";
	account_email = "";
	wins = 0;
	losses = 0;
	rating = 0;
	qualifier_points = 0;
	rank = 0;
	tournament_reward_points = 0;
	morale = 0;
	experience = 0;
	level = 0;
	current_kurzick = 0;
	total_earned_kurzick = 0;
	max_kurzick = 0;
	current_luxon = 0;
	total_earned_luxon = 0;
	max_luxon = 0;
	current_imperial = 0;
	total_earned_imperial = 0;
	max_imperial = 0;
	current_balth = 0;
	total_earned_balth = 0;
	max_balth = 0;
	current_skill_points = 0;
	total_earned_skill_points = 0;
	party_morale.clear();
	player_uuid = { 0, 0, 0, 0 };

	missions_completed.clear();
	missions_bonus.clear();
	missions_completed_hm.clear();
	missions_bonus_hm.clear();
	controlled_minions.clear();
	unlocked_map.clear();
	learnable_character_skills.clear();
	unlocked_character_skills.clear();


}

inline uint32_t PickHighestValid(uint32_t a, uint32_t b) {
    constexpr uint32_t INVALID_LOW = 0;
    constexpr uint32_t INVALID_HIGH = std::numeric_limits<uint32_t>::max();

    bool a_valid = (a != INVALID_LOW && a != INVALID_HIGH);
    bool b_valid = (b != INVALID_LOW && b != INVALID_HIGH);

    if (a_valid && b_valid)
        return std::max(a, b);
    if (a_valid)
        return a;
    if (b_valid)
        return b;

    return 0;
}

void PyPlayer::GetContext() {
    auto instance_type = GW::Map::GetInstanceType();
    bool is_map_ready = (GW::Map::GetIsMapLoaded()) && (!GW::Map::GetIsObserving()) && (instance_type != GW::Constants::InstanceType::Loading);

    if (!is_map_ready) {
        ResetContext();
        return;
    }

	auto player_loaded = GW::PartyMgr::GetIsPlayerLoaded();

	if (!player_loaded) {
		ResetContext();
		return;
	}


    id = static_cast<int>(GW::Agents::GetControlledCharacterId()); 
    agent = id;
    target_id = static_cast<int>(GW::Agents::GetTargetId());  
    //mouse_over_id = static_cast<int>(GW::Agents::GetMouseoverId());  
    //mouse_over_id = 0; 
    observing_id = static_cast<int>(GW::Agents::GetObservingId()); 

    if (GW::Map::GetIsMapLoaded()) {
        GW::WorldContext* world_context = GW::GetWorldContext();
        if (world_context) {
            const auto char_context = GW::GetCharContext();
            if (char_context) {
                std::wstring wplayer_email = char_context->player_email;
                account_email = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wplayer_email);
				for (int i = 0; i < 4; ++i)
                    player_uuid[i] = char_context->player_uuid[i];

            }

            auto account_info = world_context->accountInfo;
            if (account_info) {
                wchar_t* waccount_name = account_info->account_name;
                account_name = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(waccount_name);
                wins = account_info->wins;
                losses = account_info->losses;
                rating = account_info->rating;
                qualifier_points = account_info->qualifier_points;
                rank = account_info->rank;
                tournament_reward_points = account_info->tournament_reward_points;
            }
            morale = PickHighestValid(world_context->morale, world_context->morale_dupe);

            std::vector<std::pair<int, int>> party_morale;

            auto party_morale_info = world_context->player_morale_info;
            party_morale.clear();

            constexpr size_t MAX_PARTY_SIZE = 16; // Safety cap to prevent runaway loops
            const uint32_t UINT_MAX_VALUE = std::numeric_limits<uint32_t>::max();

            for (size_t i = 0; i < MAX_PARTY_SIZE && party_morale_info; ++i) {
                uint32_t pm_agent_id = party_morale_info->agent_id;
                uint32_t pm_morale = party_morale_info->morale;

                // Stop if invalid/garbage data
                if (pm_agent_id == 0 || pm_agent_id == UINT_MAX_VALUE)
                    break;

                party_morale.emplace_back(static_cast<int>(pm_agent_id),
                    static_cast<int>(pm_morale));

                // Advance to next struct (array-like traversal)
                ++party_morale_info;
            }


            experience = PickHighestValid(world_context->experience, world_context->experience_dupe);
			level = PickHighestValid(world_context->level, world_context->level_dupe);
            current_kurzick = PickHighestValid(world_context->current_kurzick, world_context->current_kurzick_dupe);
            total_earned_kurzick = PickHighestValid(world_context->total_earned_kurzick, world_context->total_earned_kurzick_dupe);
            max_kurzick = world_context->max_kurzick;
            current_luxon = PickHighestValid(world_context->current_luxon, world_context->current_luxon_dupe);
            total_earned_luxon = PickHighestValid(world_context->total_earned_luxon, world_context->total_earned_luxon_dupe);
            max_luxon = world_context->max_luxon;
            current_imperial = PickHighestValid(world_context->current_imperial, world_context->current_imperial_dupe);
            total_earned_imperial = PickHighestValid(world_context->total_earned_imperial, world_context->total_earned_imperial_dupe);
            max_imperial = world_context->max_imperial;
            current_balth = PickHighestValid(world_context->current_balth, world_context->current_balth_dupe);
            total_earned_balth = PickHighestValid(world_context->total_earned_balth, world_context->total_earned_balth_dupe);
            max_balth = world_context->max_balth;
            current_skill_points = PickHighestValid(world_context->current_skill_points, world_context->current_skill_points_dupe);
            total_earned_skill_points = PickHighestValid(world_context->total_earned_skill_points, world_context->total_earned_skill_points_dupe);

            auto copy_uint_array = [](const GW::Array<uint32_t>& arr, std::vector<uint32_t>& out) {
                out.assign(arr.begin(), arr.end());
                };

            // Missions
            copy_uint_array(world_context->missions_completed, missions_completed);
            copy_uint_array(world_context->missions_bonus, missions_bonus);
            copy_uint_array(world_context->missions_completed_hm, missions_completed_hm);
            copy_uint_array(world_context->missions_bonus_hm, missions_bonus_hm);

            // Controlled minions — USE minion_count (not "count")
            for (size_t i = 0; i < world_context->controlled_minion_count.size(); ++i) {
                const auto& cm = world_context->controlled_minion_count[i];
                controlled_minions.emplace_back(
                    static_cast<int>(cm.agent_id),
                    static_cast<int>(cm.minion_count)
                );
            }

            // Map & skills
            copy_uint_array(world_context->unlocked_map, unlocked_map);
            copy_uint_array(world_context->learnable_character_skills, learnable_character_skills);
            copy_uint_array(world_context->unlocked_character_skills, unlocked_character_skills);

        }
    }

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

bool PyPlayer::Istyping() {
	return GW::Chat::GetIsTyping();
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

void PyPlayer::SendFakeChat(int channel, std::string message) {
	GW::Chat::SendFakeChat(channel, message);
}

void PyPlayer::SendFakeChatColored(int channel, std::string message, int r, int g, int b) {
	GW::Chat::SendFakeChatColored(channel, message, r, g, b);
}



void PyPlayer::SendDialog(uint32_t dialog_id) {
    GW::Agents::SendDialog(dialog_id);
}



namespace GW {
    struct AvailableCharacterInfo {
        /* + h0000 */
        uint32_t h0000[2];
        /* + h0008 */
        uint32_t uuid[4];
        /* + h0018 */
        wchar_t player_name[20];
        /* + h0040 */
        uint32_t props[17];

        GW::Constants::MapID map_id() const
        {
            return static_cast<GW::Constants::MapID>((props[0] >> 16) & 0xffff);
        }

        uint32_t primary() const
        {
            return ((props[2] >> 20) & 0xf);
        }
        uint32_t secondary() const
        {
            return ((props[7] >> 10) & 0xf);
        }

        uint32_t campaign() const
        {
            return (props[7] & 0xf);
        }

        uint32_t level() const
        {
            return ((props[7] >> 4) & 0x3f);
        }

        bool is_pvp() const
        {
            return ((props[7] >> 9) & 0x1) == 0x1;
        }
    };
    static_assert(sizeof(AvailableCharacterInfo) == 0x84);

    GW::Array<GW::AvailableCharacterInfo>* available_chars_ptr = nullptr;

    namespace AccountMgr {
        GW::Array<AvailableCharacterInfo>* GetAvailableChars() {
            if (available_chars_ptr)
                return available_chars_ptr;
            const uintptr_t address = GW::Scanner::Find("\x8b\x35\x00\x00\x00\x00\x57\x69\xF8\x84\x00\x00\x00", "xx????xxxxxxx", 0x2);
			Logger::AssertAddress("GetAvailableChars", address);
            //ASSERT(address);
            available_chars_ptr = *(GW::Array<AvailableCharacterInfo>**)address;
            return available_chars_ptr;
        }

        AvailableCharacterInfo* GetAvailableCharacter(const wchar_t* name) {
            const auto characters = name ? GetAvailableChars() : nullptr;
            if (!characters)
                return nullptr;
            for (auto& ac : *characters) {
                if (wcscmp(ac.player_name, name) == 0)
                    return &ac;
            }
            return nullptr;
        }
    }
}



bool PyPlayer::IsAgentIDValid(int agent_id) {
	const auto agent = GW::Agents::GetAgentByID(agent_id);
	if (!agent) return false;
	return true;
}

bool PyPlayer::InteractAgent(int agent_id, bool call_target) {
    if (agent_id == 0) return false;

    GW::GameThread::Enqueue([agent_id, call_target] {
        if (GW::Agent* a = GW::Agents::GetAgentByID(agent_id)) {
            GW::Agents::InteractAgent(a, call_target);
        }
        });
    return true;
}

bool PyPlayer::ChangeTarget(uint32_t new_target_id) {
    if (new_target_id == 0) return false;

    auto agent = GW::Agents::GetAgentByID(new_target_id);
    if (!agent) return false;

    GW::GameThread::Enqueue([agent, new_target_id] {
        if (GW::Agent* a = GW::Agents::GetAgentByID(new_target_id)) {
            GW::Agents::ChangeTarget(agent);
        }
        });
    return true;
}



void BindPyPlayer(py::module_& m) {
    py::class_<PyPlayer>(m, "PyPlayer")
        .def(py::init<>())  // Bind the constructor
        .def("GetContext", &PyPlayer::GetContext)  // Bind the GetContext method

        // Bind public attributes with snake_case naming convention
        .def_readonly("id", &PyPlayer::id)  // Bind the id attribute
        .def_readonly("agent", &PyPlayer::agent)  // Bind the agent attribute
        .def_readonly("target_id", &PyPlayer::target_id)  // Bind the target_id attribute
        .def_readonly("mouse_over_id", &PyPlayer::mouse_over_id)  // Bind the mouse_over_id attribute
        .def_readonly("observing_id", &PyPlayer::observing_id)  // Bind the observing_id attribute
        .def_readonly("account_name", &PyPlayer::account_name) // Bind the account_name attribute
        .def_readonly("account_email", &PyPlayer::account_email)  // Bind the account_email attribute
        .def_readonly("player_uuid", &PyPlayer::player_uuid)  // Bind the player_uuid attribute
        .def_readonly("wins", &PyPlayer::wins)  // Bind the wins attribute
        .def_readonly("losses", &PyPlayer::losses)  // Bind the losses attribute
        .def_readonly("rating", &PyPlayer::rating)  // Bind the rating attribute
        .def_readonly("qualifier_points", &PyPlayer::qualifier_points)  // Bind the qualifier_points attribute
        .def_readonly("rank", &PyPlayer::rank)  // Bind the rank attribute
        .def_readonly("tournament_reward_points", &PyPlayer::tournament_reward_points)  // Bind the tournament_reward_points attribute
        .def_readonly("morale", &PyPlayer::morale)  // Bind the morale attribute
        .def_readonly("party_morale", &PyPlayer::party_morale)  // Bind the party_morale attribute
        .def_readonly("experience", &PyPlayer::experience)  // Bind the experience attribute
        .def_readonly("level", &PyPlayer::level)  // Bind the level attribute
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
        .def_readonly("total_earned_skill_points", &PyPlayer::total_earned_skill_points)  // Bind the total_earned_skill_points attribute
        .def_readonly("missions_completed", &PyPlayer::missions_completed)  // Bind the missions_completed attribute
        .def_readonly("missions_bonus", &PyPlayer::missions_bonus)  // Bind the missions_bonus attribute
        .def_readonly("missions_completed_hm", &PyPlayer::missions_completed_hm)  // Bind the missions_completed_hm attribute
        .def_readonly("missions_bonus_hm", &PyPlayer::missions_bonus_hm)  // Bind the missions_bonus_hm attribute
        .def_readonly("controlled_minions", &PyPlayer::controlled_minions) // Bind the controlled_minions attribute
        .def_readonly("unlocked_maps", &PyPlayer::unlocked_map)  // Bind the unlocked_maps attribute
        .def_readonly("learnable_character_skills", &PyPlayer::learnable_character_skills)  // Bind the learnable_character_skills attribute
        .def_readonly("unlocked_character_skills", &PyPlayer::unlocked_character_skills)  // Bind the unlocked_character_skills attribute

        .def("SendDialog", &PyPlayer::SendDialog, py::arg("dialog_id"))  // Bind the SendDialog method
        .def("ChangeTarget", &PyPlayer::ChangeTarget, py::arg("target_id"))  // Bind the ChangeTarget method
        .def("InteractAgent", &PyPlayer::InteractAgent, py::arg("agent_id"), py::arg("call_target"))  // Bind the InteractAgent method

        .def("IsAgentIDValid", &PyPlayer::IsAgentIDValid, py::arg("agent_id"))  // Bind the IsAgentIDValid method
        .def("GetChatHistory", &PyPlayer::GetChatHistory)  // Bind the GetChatHistory method
        .def("RequestChatHistory", &PyPlayer::RequestChatHistory)  // Bind the RequestChatHistory method
        .def("IsChatHistoryReady", &PyPlayer::IsChatHistoryReady)  // Bind the IsChatHistoryReady method
		.def("Istyping", &PyPlayer::Istyping)  // Bind the Istyping method
        .def("SendChatCommand", &PyPlayer::SendChatCommand, py::arg("msg"))  // Bind the SendChatCommand method
        .def("SendChat", &PyPlayer::SendChat, py::arg("channel"), py::arg("msg"))  // Bind the SendChat method
        .def("SendWhisper", &PyPlayer::SendWhisper, py::arg("name"), py::arg("msg"))  // Bind the SendWhisper method
        .def("SendFakeChat", &PyPlayer::SendFakeChat, py::arg("channel"), py::arg("message"))  // Bind the SendFakeChat method
        .def("SendFakeChatColored", &PyPlayer::SendFakeChatColored, py::arg("channel"), py::arg("message"), py::arg("r"), py::arg("g"), py::arg("b"))  // Bind the SendFakeChatColored method

		
		;
        
}

PYBIND11_EMBEDDED_MODULE(PyPlayer, m) {
    BindPyPlayer(m);
}

