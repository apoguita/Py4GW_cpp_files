#pragma once
#include "Headers.h"

/////////////////////////////////////////////////////




//GW::Item* GetNextUnsalvagedItem(const Item* salvage_kit = nullptr, const Item* start_after_item = nullptr);
//GW::Item* GetNextUnidentifiedItem(const Item* start_after_item = nullptr);


class ItemExtension {
private:
    GW::Item* gw_item;  // Stores the pointer to the GW::Item object

    // Retrieve modifier based on identifier
    GW::ItemModifier* GetModifier(uint32_t identifier) const;

public:
    // Constructor
    ItemExtension(GW::Item* item);

    // Retrieve item uses
    uint32_t GetUses() const;

    // Item checks
    bool IsTome() const;
    bool IsIdentificationKit() const;
    bool IsLesserKit() const;
    bool IsExpertSalvageKit() const;
    bool IsPerfectSalvageKit() const;
    bool IsSalvageKit() const;
    bool IsRareMaterial() const;
    bool IsInventoryItem() const;
    bool IsStorageItem() const;

    // Item rarity
    GW::Constants::Rarity GetRarity() const;

    // Additional methods based on interaction flags
    bool IsSparkly() const;
    bool GetIsIdentified() const;
    bool IsPrefixUpgradable() const;
    bool IsSuffixUpgradable() const;
    bool IsStackable() const;
    bool IsUsable() const;
    bool IsTradable() const;
    bool IsInscription() const;
    bool IsBlue() const;
    bool IsPurple() const;
    bool IsGreen() const;
    bool IsGold() const;

    // Item type checks
    bool IsWeapon();
    bool IsArmor();
    bool IsSalvagable();
    bool IsOfferedInTrade() const;
};


// SafeItemModifier class
class SafeItemModifier {
private:
    uint32_t mod;

public:
    // Constructor
    SafeItemModifier(uint32_t mod_value);

    // Getter methods
    uint32_t get_identifier() const;
    uint32_t get_arg1() const;
    uint32_t get_arg2() const;
    uint32_t get_arg() const;

    // Validation
    bool is_valid() const;

    // Binary representation methods
    std::string get_mod_bits() const;
    std::string get_identifier_bits() const;
    std::string get_arg1_bits() const;
    std::string get_arg2_bits() const;
    std::string get_arg_bits() const;

    // Convert to string
    std::string to_string() const;
};


class SafeItemTypeClass {
private:
    SafeItemType safe_item_type;

public:
    // Constructor
    SafeItemTypeClass(int item_type);

    // Convert to int
    int to_int() const;

    // Comparison operators
    bool SafeItemTypeClass::operator==(const SafeItemTypeClass& other) const { return safe_item_type == other.safe_item_type; }
    bool SafeItemTypeClass::operator!=(const SafeItemTypeClass& other) const { return safe_item_type != other.safe_item_type; }

    // Get item name as string
    std::string get_name() const;
};

// Wrapper class for DyeColor
class SafeDyeColorClass {
private:
    SafeDyeColor safe_dye_color;

public:
    // Constructor
    SafeDyeColorClass(int dye_color);

    // Convert to int
    int to_int() const;

    // Comparison operators
    bool operator==(const SafeDyeColorClass& other) const;
    bool operator!=(const SafeDyeColorClass& other) const;

    // Get color name as string
    std::string to_string() const;
};

// Wrapper class for DyeInfo
class SafeDyeInfoClass {
public:
    uint8_t dye_tint;
    SafeDyeColorClass dye1;
    SafeDyeColorClass dye2;
    SafeDyeColorClass dye3;
    SafeDyeColorClass dye4;

public:
    // Default constructor
    SafeDyeInfoClass();

    // Constructor from GW::DyeInfo
    explicit SafeDyeInfoClass(const GW::DyeInfo& dye_info);

    // Equality operator
    bool operator==(const SafeDyeInfoClass& other) const {
        return dye_tint == other.dye_tint &&
            dye1 == other.dye1 &&
            dye2 == other.dye2 &&
            dye3 == other.dye3 &&
            dye4 == other.dye4;
    }

    // Inequality operator
    bool operator!=(const SafeDyeInfoClass& other) const {
        return !(*this == other);
    }

    // Convert to string
    std::string to_string() const;
};


std::string item_WStringToString(const std::wstring& s)
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

// SafeItem class
class SafeItem {
public:
    int item_id = 0;
    int agent_id;
    int agent_item_id;

    std::string name;
    std::vector<SafeItemModifier> modifiers;
    bool IsCustomized = false;
    SafeItemTypeClass item_type = 0;
    SafeDyeInfoClass dye_info;
    int value = 0;
    uint32_t interaction;
    uint32_t model_id;
    int item_formula = 0;
    int is_material_salvageable = 0;
    int quantity = 0;
    int equipped = 0;
    int profession = 0;
    int slot = 0;
    bool is_stackable = false;
    bool is_inscribable = false;
    bool is_material = false;
    bool is_ZCoin = false;
    GW::Constants::Rarity rarity;
    int Uses = 0;
    bool IsIDKit = false;
    bool IsSalvageKit = false;
    bool IsTome = false;
    bool IsLesserKit = false;
    bool IsExpertSalvageKit = false;
    bool IsPerfectSalvageKit = false;
    bool IsWeapon = false;
    bool IsArmor = false;
    bool IsSalvagable = false;
    bool IsInventoryItem = false;
    bool IsStorageItem = false;
    bool IsRareMaterial = false;
    bool IsOfferedInTrade = false;
    bool CanOfferToTrade = false;
    bool IsSparkly = false;
    bool IsIdentified = false;
    bool IsPrefixUpgradable = false;
    bool IsSuffixUpgradable = false;
    bool IsStackable = false;
    bool IsUsable = false;
    bool IsTradable = false;
    bool IsInscription = false;
    bool IsRarityBlue = false;
    bool IsRarityPurple = false;
    bool IsRarityGreen = false;
    bool IsRarityGold = false;

    SafeItemModifier* GetModifier(uint32_t identifier) {
        for (auto& mod : modifiers) {
            if (mod.get_identifier() == identifier) {
                return &mod;
            }
        }
        return nullptr;  // Return nullptr if no matching modifier found
    }

    SafeItem(int item_id) : item_id(item_id) {
        GetContext();
    }

    void GetContext();
	void RequestName();
    bool IsItemNameReady();
	std::string GetName();
	bool IsItemValid(uint32_t item_id);

};

/////// INVENTORY MODULE ///////

class SafeBag {
public:
    int ID = 0;
    std::string Name = "";
    std::vector<SafeItem> items;
    int container_item = 0;
    int items_count = 0;
    bool IsInventoryBag = false;
    bool IsStorageBag = false;
    bool IsMaterialStorage = false;

    // Constructor
    SafeBag::SafeBag(int bag_id, std::string bag_name) : ID(bag_id), Name(bag_name) { GetContext(); }

    // Methods
    int GetItemCount() const;
    std::vector<SafeItem> GetItems() const;
    SafeItem GetItemByIndex(int index) const;
    int GetSize() const;
    SafeItem* FindItemById(int item_id);
    void GetContext();
};

class Inventory {
public:

    void Salvage(int salv_kit_id, int item_id) {

        GW::Item* item = GW::Items::GetItemById(item_id);
        if (!item) return;

        GW::Item* kit = GW::Items::GetItemById(salv_kit_id);

        SafeItem Unsalv(item_id);
        SafeItem SalvKit(salv_kit_id);

        if ((item && kit) && (SalvKit.IsSalvageKit && Unsalv.IsSalvagable))
        {
            GW::Items::SalvageStart(kit->item_id, item->item_id);
        }
        return;

    }

	void AcceptSalvageWindow()
	{
        // Auto accept "you can only salvage materials with a lesser salvage kit"
        GW::GameThread::Enqueue([] {
            GW::UI::ButtonClick(GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Game"), { 0x6, 0x62, 0x6 }));
            });
	}

    void OpenXunlaiWindow() {
        GW::GameThread::Enqueue([] {
            GW::Items::OpenXunlaiWindow();
            });
    }

    bool GetIsStorageOpen() {
        return GW::Items::GetIsStorageOpen();
    }

    bool PickUpItem(int item_id, bool call_target) {
        GW::Item* item = GW::Items::GetItemById(item_id);
        if (!item) return false;
        return GW::Items::PickUpItem(item, call_target);
    }

    bool DropItem(int item_id, int quantity) {
        GW::Item* item = GW::Items::GetItemById(item_id);
        if (!item) return false;
        return GW::Items::DropItem(item, quantity);
    }

    bool EquipItem(int item_id, int agent_id) {
        GW::Item* item = GW::Items::GetItemById(item_id);
        if (!item) return false;
        return GW::Items::EquipItem(item, agent_id);
    }

    bool UseItem(int item_id) {
        GW::Item* item = GW::Items::GetItemById(item_id);
        if (!item) return false;
        return GW::Items::UseItem(item);
    }

    int GetHoveredItemId() {
        GW::Item* item = GW::Items::GetHoveredItem();
        if (!item) return 0;
        return item->item_id;
    }

    bool DropGold(int amount) {
        return GW::Items::DropGold(amount);
    }

    bool DestroyItem(int item_id) {
        return GW::Items::DestroyItem(item_id);
    }

    bool IdentifyItem(int id_kit_id, int item_id) {
        bool result = false;
        GW::GameThread::Enqueue([id_kit_id, item_id, &result] {
            result = GW::Items::IdentifyItem(id_kit_id, item_id);
            });
        return result;
    }


    int GetGoldAmount() {
        return GW::Items::GetGoldAmountOnCharacter();
    }

    int GetGoldAmountInStorage() {
        return GW::Items::GetGoldAmountInStorage();
    }

    int DepositGold(int amount) {
        return GW::Items::DepositGold(amount);
    }

    int WithdrawGold(int amount) {
        return GW::Items::WithdrawGold(amount);
    }

    bool MoveItem(int item_id, int bag_id, int slot, int quantity) {
        GW::Item* item = GW::Items::GetItemById(item_id);
        if (!item) return false;
        return GW::Items::MoveItem(item, static_cast<GW::Constants::Bag>(bag_id), slot, quantity);
    }

  

};



