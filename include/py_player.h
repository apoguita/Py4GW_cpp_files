#pragma once
#include "Headers.h"

class PyPlayer {
public:
    int id = 0;  
    PyAgent agent = 0;
    int target_id = 0;
    int mouse_over_id = 0;
    int observing_id = 0;

	std::string account_name = "";
	std::string account_email = "";
    uint32_t wins = 0;
    uint32_t losses = 0;
    uint32_t rating = 0;
    uint32_t qualifier_points = 0;
    uint32_t rank = 0;
    uint32_t tournament_reward_points = 0;

	uint32_t morale = 0;
	uint32_t experience = 0;
	uint32_t current_kurzick = 0;
	uint32_t total_earned_kurzick = 0;
    uint32_t max_kurzick;
	uint32_t current_luxon = 0;
    uint32_t total_earned_luxon;
	uint32_t max_luxon;
    uint32_t current_imperial;
    uint32_t total_earned_imperial;
    uint32_t max_imperial;
    uint32_t current_balth;
    uint32_t total_earned_balth;
    uint32_t max_balth;
    uint32_t current_skill_points;
    uint32_t total_earned_skill_points;
    std::vector<std::pair<int, int>> party_morale;




    PyPlayer();
    void GetContext();
	void ResetContext();
    std::vector<int> GetAgentArray();
    std::vector<int> GetAllyArray();
    std::vector<int> GetNeutralArray();
    std::vector<int> GetEnemyArray();
    std::vector<int> GetSpiritPetArray();
    std::vector<int> GetMinionArray();
    std::vector<int> GetNPCMinipetArray();
    std::vector<int> GetItemArray();
    std::vector<int> GetOwnedItemArray(int owner_agent_id);
    std::vector<int> GetGadgetArray();
	std::vector<std::string> GetChatHistory();
    bool Istyping();
	void RequestChatHistory();
	bool IsChatHistoryReady();
    void SendChatCommand(std::string msg);
    void SendChat(char channel, std::string msg);
    void SendWhisper(std::string name, std::string msg);
    void SendFakeChat(int channel, std::string message);
    void SendFakeChatColored(int channel, std::string message, int r, int g, int b);
	std::string FormatChatMessage(const std::string message, int r, int g, int b);
    bool ChangeTarget(uint32_t target_id);
    bool Move(float x, float y, int zplane);
    bool Move(float x, float y);
    bool InteractAgent(int agent_id, bool call_target);
    bool OpenLockedChest(bool use_key);
    void SendDialog(uint32_t dialog_id);

	bool SetActiveTitle(uint32_t title_id);
    bool RemoveActiveTitle();
    bool DepositFaction(uint32_t allegiance);
    uint32_t GetActiveTitleId();
	bool IsAgentIDValid(int agent_id);

    
};



