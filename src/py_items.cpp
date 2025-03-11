#include "py_items.h"

// Constructor
ItemExtension::ItemExtension(GW::Item* item) : gw_item(item) {}

// Retrieve modifier based on identifier
GW::ItemModifier* ItemExtension::GetModifier(uint32_t identifier) const {
    for (size_t i = 0; i < gw_item->mod_struct_size; ++i) {
        GW::ItemModifier* mod = &gw_item->mod_struct[i];
        if (mod->identifier() == identifier) {
            return mod;
        }
    }
    return nullptr;
}

// Retrieve item uses
uint32_t ItemExtension::GetUses() const {
    GW::ItemModifier* mod = GetModifier(0x2458);
    return mod ? mod->arg2() : gw_item->quantity;
}

// Check if item is a Tome
bool ItemExtension::IsTome() const {
    const GW::ItemModifier* mod = GetModifier(0x2788);
    const uint32_t use_id = mod ? mod->arg2() : 0;
    return use_id > 15 && use_id < 36;
}

// Check if item is an Identification Kit
bool ItemExtension::IsIdentificationKit() const {
    const GW::ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 1;
}

// Check if item is a Lesser Kit
bool ItemExtension::IsLesserKit() const {
    const GW::ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 3;
}

// Check if item is an Expert Salvage Kit
bool ItemExtension::IsExpertSalvageKit() const {
    const GW::ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 2;
}

// Check if item is a Perfect Salvage Kit
bool ItemExtension::IsPerfectSalvageKit() const {
    const GW::ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 6;
}

// Check if item is a Salvage Kit
bool ItemExtension::IsSalvageKit() const {
    return IsLesserKit() || IsExpertSalvageKit() || IsPerfectSalvageKit();
}

// Check if item is a Rare Material
bool ItemExtension::IsRareMaterial() const {
    const GW::ItemModifier* mod = GetModifier(0x2508);
    return mod && mod->arg1() > 11;
}

// Check if item is in inventory
bool ItemExtension::IsInventoryItem() const {
    return gw_item->bag && (gw_item->bag->IsInventoryBag() || gw_item->bag->bag_type == GW::Constants::BagType::Equipped);
}

// Check if item is in storage
bool ItemExtension::IsStorageItem() const {
    return gw_item->bag && (gw_item->bag->IsStorageBag() || gw_item->bag->IsMaterialStorage());
}

// Get item rarity
GW::Constants::Rarity ItemExtension::GetRarity() const {
    if (IsGreen()) return GW::Constants::Rarity::Green;
    if (IsGold()) return GW::Constants::Rarity::Gold;
    if (IsPurple()) return GW::Constants::Rarity::Purple;
    if (IsBlue()) return GW::Constants::Rarity::Blue;
    return GW::Constants::Rarity::White;
}

// Additional methods
bool ItemExtension::IsSparkly() const { return (gw_item->interaction & 0x2000) == 0; }
bool ItemExtension::GetIsIdentified() const { return (gw_item->interaction & 1) != 0; }
bool ItemExtension::IsPrefixUpgradable() const { return ((gw_item->interaction >> 14) & 1) == 0; }
bool ItemExtension::IsSuffixUpgradable() const { return ((gw_item->interaction >> 15) & 1) == 0; }
bool ItemExtension::IsStackable() const { return (gw_item->interaction & 0x80000) != 0; }
bool ItemExtension::IsUsable() const { return (gw_item->interaction & 0x1000000) != 0; }
bool ItemExtension::IsTradable() const { return (gw_item->interaction & 0x100) == 0; }
bool ItemExtension::IsInscription() const { return (gw_item->interaction & 0x25000000) == 0x25000000; }
bool ItemExtension::IsBlue() const { return gw_item->single_item_name && gw_item->single_item_name[0] == 0xA3F; }
bool ItemExtension::IsPurple() const { return (gw_item->interaction & 0x400000) != 0; }
bool ItemExtension::IsGreen() const { return (gw_item->interaction & 0x10) != 0; }
bool ItemExtension::IsGold() const { return (gw_item->interaction & 0x20000) != 0; }

// Check if item is a Weapon
bool ItemExtension::IsWeapon() {
    switch (static_cast<GW::Constants::ItemType>(gw_item->type)) {
    case GW::Constants::ItemType::Axe:
    case GW::Constants::ItemType::Sword:
    case GW::Constants::ItemType::Shield:
    case GW::Constants::ItemType::Scythe:
    case GW::Constants::ItemType::Bow:
    case GW::Constants::ItemType::Wand:
    case GW::Constants::ItemType::Staff:
    case GW::Constants::ItemType::Offhand:
    case GW::Constants::ItemType::Daggers:
    case GW::Constants::ItemType::Hammer:
    case GW::Constants::ItemType::Spear:
        return true;
    default:
        return false;
    }
}

// Check if item is an Armor
bool ItemExtension::IsArmor() {
    switch (static_cast<GW::Constants::ItemType>(gw_item->type)) {
    case GW::Constants::ItemType::Headpiece:
    case GW::Constants::ItemType::Chestpiece:
    case GW::Constants::ItemType::Leggings:
    case GW::Constants::ItemType::Boots:
    case GW::Constants::ItemType::Gloves:
        return true;
    default:
        return false;
    }
}

// Check if item is Salvageable
bool ItemExtension::IsSalvagable() {
    if (gw_item->item_formula == 0x5da) return false;
    if (IsUsable() || IsGreen()) return false;
    switch (static_cast<GW::Constants::ItemType>(gw_item->type)) {
    case GW::Constants::ItemType::Trophy:
        return GetRarity() == GW::Constants::Rarity::White && gw_item->info_string && gw_item->is_material_salvageable;
    case GW::Constants::ItemType::Salvage:
    case GW::Constants::ItemType::CC_Shards:
        return true;
    case GW::Constants::ItemType::Materials_Zcoins:
        return gw_item->is_material_salvageable != 0;
    default:
        break;
    }
    if (IsWeapon() || IsArmor()) return true;
    return false;
}

// Check if item is offered in trade
bool ItemExtension::IsOfferedInTrade() const {
    return GW::Trade::IsItemOffered(gw_item->item_id) != nullptr;
}




std::string int_to_binary_string(uint32_t value, size_t bit_count = 32) {
    std::string binary_str;
    for (int i = bit_count - 1; i >= 0; --i) {
        binary_str += (value & (1 << i)) ? '1' : '0';
    }
    return binary_str;
}

SafeItemModifier::SafeItemModifier(uint32_t mod_value) : mod(mod_value) {}

uint32_t SafeItemModifier::get_identifier() const {
    return mod >> 16;
}

uint32_t SafeItemModifier::get_arg1() const {
    return (mod & 0x0000FF00) >> 8;
}

uint32_t SafeItemModifier::get_arg2() const {
    return (mod & 0x000000FF);
}

uint32_t SafeItemModifier::get_arg() const {
    return (mod & 0x0000FFFF);
}

// Validation
bool SafeItemModifier::is_valid() const {
    return mod != 0;
}

// Binary representation methods
std::string SafeItemModifier::get_mod_bits() const {
    return int_to_binary_string(mod, 32);
}

std::string SafeItemModifier::get_identifier_bits() const {
    return int_to_binary_string(get_identifier(), 16);
}

std::string SafeItemModifier::get_arg1_bits() const {
    return int_to_binary_string(get_arg1(), 8);
}

std::string SafeItemModifier::get_arg2_bits() const {
    return int_to_binary_string(get_arg2(), 8);
}

std::string SafeItemModifier::get_arg_bits() const {
    return int_to_binary_string(get_arg(), 16);
}

// Convert to string
std::string SafeItemModifier::to_string() const {
    if (!is_valid()) {
        return "No Modifier";
    }
    return "Modifier ID: " + std::to_string(get_identifier()) +
        " (" + get_identifier_bits() + ")" +
        ", Arg1: " + std::to_string(get_arg1()) +
        " (" + get_arg1_bits() + ")" +
        ", Arg2: " + std::to_string(get_arg2()) +
        " (" + get_arg2_bits() + ")" +
        ", Arg: " + std::to_string(get_arg()) +
        " (" + get_arg_bits() + ")";
}

SafeItemTypeClass::SafeItemTypeClass(int item_type) {
    switch (static_cast<GW::Constants::ItemType>(item_type)) {
    case GW::Constants::ItemType::Salvage: safe_item_type = SafeItemType::Salvage; break;
    case GW::Constants::ItemType::Axe: safe_item_type = SafeItemType::Axe; break;
    case GW::Constants::ItemType::Bag: safe_item_type = SafeItemType::Bag; break;
    case GW::Constants::ItemType::Boots: safe_item_type = SafeItemType::Boots; break;
    case GW::Constants::ItemType::Bow: safe_item_type = SafeItemType::Bow; break;
    case GW::Constants::ItemType::Bundle: safe_item_type = SafeItemType::Bundle; break;
    case GW::Constants::ItemType::Chestpiece: safe_item_type = SafeItemType::Chestpiece; break;
    case GW::Constants::ItemType::Rune_Mod: safe_item_type = SafeItemType::Rune_Mod; break;
    case GW::Constants::ItemType::Usable: safe_item_type = SafeItemType::Usable; break;
    case GW::Constants::ItemType::Dye: safe_item_type = SafeItemType::Dye; break;
    case GW::Constants::ItemType::Materials_Zcoins: safe_item_type = SafeItemType::Materials_Zcoins; break;
    case GW::Constants::ItemType::Offhand: safe_item_type = SafeItemType::Offhand; break;
    case GW::Constants::ItemType::Gloves: safe_item_type = SafeItemType::Gloves; break;
    case GW::Constants::ItemType::Hammer: safe_item_type = SafeItemType::Hammer; break;
    case GW::Constants::ItemType::Headpiece: safe_item_type = SafeItemType::Headpiece; break;
    case GW::Constants::ItemType::CC_Shards: safe_item_type = SafeItemType::CC_Shards; break;
    case GW::Constants::ItemType::Key: safe_item_type = SafeItemType::Key; break;
    case GW::Constants::ItemType::Leggings: safe_item_type = SafeItemType::Leggings; break;
    case GW::Constants::ItemType::Gold_Coin: safe_item_type = SafeItemType::Gold_Coin; break;
    case GW::Constants::ItemType::Quest_Item: safe_item_type = SafeItemType::Quest_Item; break;
    case GW::Constants::ItemType::Wand: safe_item_type = SafeItemType::Wand; break;
    case GW::Constants::ItemType::Shield: safe_item_type = SafeItemType::Shield; break;
    case GW::Constants::ItemType::Staff: safe_item_type = SafeItemType::Staff; break;
    case GW::Constants::ItemType::Sword: safe_item_type = SafeItemType::Sword; break;
    case GW::Constants::ItemType::Kit: safe_item_type = SafeItemType::Kit; break;
    case GW::Constants::ItemType::Trophy: safe_item_type = SafeItemType::Trophy; break;
    case GW::Constants::ItemType::Scroll: safe_item_type = SafeItemType::Scroll; break;
    case GW::Constants::ItemType::Daggers: safe_item_type = SafeItemType::Daggers; break;
    case GW::Constants::ItemType::Present: safe_item_type = SafeItemType::Present; break;
    case GW::Constants::ItemType::Minipet: safe_item_type = SafeItemType::Minipet; break;
    case GW::Constants::ItemType::Scythe: safe_item_type = SafeItemType::Scythe; break;
    case GW::Constants::ItemType::Spear: safe_item_type = SafeItemType::Spear; break;
    case GW::Constants::ItemType::Storybook: safe_item_type = SafeItemType::Storybook; break;
    case GW::Constants::ItemType::Costume: safe_item_type = SafeItemType::Costume; break;
    case GW::Constants::ItemType::Costume_Headpiece: safe_item_type = SafeItemType::Costume_Headpiece; break;
    case GW::Constants::ItemType::Unknown: safe_item_type = SafeItemType::Unknown; break;
    default: safe_item_type = SafeItemType::Unknown; break;
    }
}

int SafeItemTypeClass::to_int() const {
    GW::Constants::ItemType result;
    switch (safe_item_type) {
    case SafeItemType::Salvage: result = GW::Constants::ItemType::Salvage; break;
    case SafeItemType::Axe: result = GW::Constants::ItemType::Axe; break;
    case SafeItemType::Bag: result = GW::Constants::ItemType::Bag; break;
    case SafeItemType::Boots: result = GW::Constants::ItemType::Boots; break;
    case SafeItemType::Bow: result = GW::Constants::ItemType::Bow; break;
    case SafeItemType::Bundle: result = GW::Constants::ItemType::Bundle; break;
    case SafeItemType::Chestpiece: result = GW::Constants::ItemType::Chestpiece; break;
    case SafeItemType::Rune_Mod: result = GW::Constants::ItemType::Rune_Mod; break;
    case SafeItemType::Usable: result = GW::Constants::ItemType::Usable; break;
    case SafeItemType::Dye: result = GW::Constants::ItemType::Dye; break;
    case SafeItemType::Materials_Zcoins: result = GW::Constants::ItemType::Materials_Zcoins; break;
    case SafeItemType::Offhand: result = GW::Constants::ItemType::Offhand; break;
    case SafeItemType::Gloves: result = GW::Constants::ItemType::Gloves; break;
    case SafeItemType::Hammer: result = GW::Constants::ItemType::Hammer; break;
    case SafeItemType::Headpiece: result = GW::Constants::ItemType::Headpiece; break;
    case SafeItemType::CC_Shards: result = GW::Constants::ItemType::CC_Shards; break;
    case SafeItemType::Key: result = GW::Constants::ItemType::Key; break;
    case SafeItemType::Leggings: result = GW::Constants::ItemType::Leggings; break;
    case SafeItemType::Gold_Coin: result = GW::Constants::ItemType::Gold_Coin; break;
    case SafeItemType::Quest_Item: result = GW::Constants::ItemType::Quest_Item; break;
    case SafeItemType::Wand: result = GW::Constants::ItemType::Wand; break;
    case SafeItemType::Shield: result = GW::Constants::ItemType::Shield; break;
    case SafeItemType::Staff: result = GW::Constants::ItemType::Staff; break;
    case SafeItemType::Sword: result = GW::Constants::ItemType::Sword; break;
    case SafeItemType::Kit: result = GW::Constants::ItemType::Kit; break;
    case SafeItemType::Trophy: result = GW::Constants::ItemType::Trophy; break;
    case SafeItemType::Scroll: result = GW::Constants::ItemType::Scroll; break;
    case SafeItemType::Daggers: result = GW::Constants::ItemType::Daggers; break;
    case SafeItemType::Present: result = GW::Constants::ItemType::Present; break;
    case SafeItemType::Minipet: result = GW::Constants::ItemType::Minipet; break;
    case SafeItemType::Scythe: result = GW::Constants::ItemType::Scythe; break;
    case SafeItemType::Spear: result = GW::Constants::ItemType::Spear; break;
    case SafeItemType::Storybook: result = GW::Constants::ItemType::Storybook; break;
    case SafeItemType::Costume: result = GW::Constants::ItemType::Costume; break;
    case SafeItemType::Costume_Headpiece: result = GW::Constants::ItemType::Costume_Headpiece; break;
    case SafeItemType::Unknown: result = GW::Constants::ItemType::Unknown; break;
    default: result = GW::Constants::ItemType::Unknown; break;
    }
    return static_cast<int>(result);
}

std::string SafeItemTypeClass::get_name() const {
    switch (safe_item_type) {
    case SafeItemType::Salvage: return "Salvage";
    case SafeItemType::Axe: return "Axe";
    case SafeItemType::Bag: return "Bag";
    case SafeItemType::Boots: return "Boots";
    case SafeItemType::Bow: return "Bow";
    case SafeItemType::Bundle: return "Bundle";
    case SafeItemType::Chestpiece: return "Chestpiece";
    case SafeItemType::Rune_Mod: return "Rune_Mod";
    case SafeItemType::Usable: return "Usable";
    case SafeItemType::Dye: return "Dye";
    case SafeItemType::Materials_Zcoins: return "Materials_Zcoins";
    case SafeItemType::Offhand: return "Offhand";
    case SafeItemType::Gloves: return "Gloves";
    case SafeItemType::Hammer: return "Hammer";
    case SafeItemType::Headpiece: return "Headpiece";
    case SafeItemType::CC_Shards: return "CC_Shards";
    case SafeItemType::Key: return "Key";
    case SafeItemType::Leggings: return "Leggings";
    case SafeItemType::Gold_Coin: return "Gold_Coin";
    case SafeItemType::Quest_Item: return "Quest_Item";
    case SafeItemType::Wand: return "Wand";
    case SafeItemType::Shield: return "Shield";
    case SafeItemType::Staff: return "Staff";
    case SafeItemType::Sword: return "Sword";
    case SafeItemType::Kit: return "Kit";
    case SafeItemType::Trophy: return "Trophy";
    case SafeItemType::Scroll: return "Scroll";
    case SafeItemType::Daggers: return "Daggers";
    case SafeItemType::Present: return "Present";
    case SafeItemType::Minipet: return "Minipet";
    case SafeItemType::Scythe: return "Scythe";
    case SafeItemType::Spear: return "Spear";
    case SafeItemType::Storybook: return "Storybook";
    case SafeItemType::Costume: return "Costume";
    case SafeItemType::Costume_Headpiece: return "Costume_Headpiece";
    case SafeItemType::Unknown: return "Unknown";
    default: return "Invalid";
    }
}


SafeDyeColorClass::SafeDyeColorClass(int dye_color) {
    switch (static_cast<GW::DyeColor>(dye_color)) {
    case GW::DyeColor::None: safe_dye_color = SafeDyeColor::NoColor; break;
    case GW::DyeColor::Blue: safe_dye_color = SafeDyeColor::Blue; break;
    case GW::DyeColor::Green: safe_dye_color = SafeDyeColor::Green; break;
    case GW::DyeColor::Purple: safe_dye_color = SafeDyeColor::Purple; break;
    case GW::DyeColor::Red: safe_dye_color = SafeDyeColor::Red; break;
    case GW::DyeColor::Yellow: safe_dye_color = SafeDyeColor::Yellow; break;
    case GW::DyeColor::Brown: safe_dye_color = SafeDyeColor::Brown; break;
    case GW::DyeColor::Orange: safe_dye_color = SafeDyeColor::Orange; break;
    case GW::DyeColor::Silver: safe_dye_color = SafeDyeColor::Silver; break;
    case GW::DyeColor::Black: safe_dye_color = SafeDyeColor::Black; break;
    case GW::DyeColor::Gray: safe_dye_color = SafeDyeColor::Gray; break;
    case GW::DyeColor::White: safe_dye_color = SafeDyeColor::White; break;
    case GW::DyeColor::Pink: safe_dye_color = SafeDyeColor::Pink; break;
    default: safe_dye_color = SafeDyeColor::NoColor; break;
    }
}

// Convert to int
int SafeDyeColorClass::to_int() const {
    GW::DyeColor result;
    switch (safe_dye_color) {
    case SafeDyeColor::NoColor: result = GW::DyeColor::None; break;
    case SafeDyeColor::Blue: result = GW::DyeColor::Blue; break;
    case SafeDyeColor::Green: result = GW::DyeColor::Green; break;
    case SafeDyeColor::Purple: result = GW::DyeColor::Purple; break;
    case SafeDyeColor::Red: result = GW::DyeColor::Red; break;
    case SafeDyeColor::Yellow: result = GW::DyeColor::Yellow; break;
    case SafeDyeColor::Brown: result = GW::DyeColor::Brown; break;
    case SafeDyeColor::Orange: result = GW::DyeColor::Orange; break;
    case SafeDyeColor::Silver: result = GW::DyeColor::Silver; break;
    case SafeDyeColor::Black: result = GW::DyeColor::Black; break;
    case SafeDyeColor::Gray: result = GW::DyeColor::Gray; break;
    case SafeDyeColor::White: result = GW::DyeColor::White; break;
    case SafeDyeColor::Pink: result = GW::DyeColor::Pink; break;
    default: result = GW::DyeColor::None; break;
    }
    return static_cast<int>(result);
}

// Comparison operators
bool SafeDyeColorClass::operator==(const SafeDyeColorClass& other) const {
    return safe_dye_color == other.safe_dye_color;
}

bool SafeDyeColorClass::operator!=(const SafeDyeColorClass& other) const {
    return safe_dye_color != other.safe_dye_color;
}

// Get color name as string
std::string SafeDyeColorClass::to_string() const {
    switch (safe_dye_color) {
    case SafeDyeColor::NoColor: return "None";
    case SafeDyeColor::Blue: return "Blue";
    case SafeDyeColor::Green: return "Green";
    case SafeDyeColor::Purple: return "Purple";
    case SafeDyeColor::Red: return "Red";
    case SafeDyeColor::Yellow: return "Yellow";
    case SafeDyeColor::Brown: return "Brown";
    case SafeDyeColor::Orange: return "Orange";
    case SafeDyeColor::Silver: return "Silver";
    case SafeDyeColor::Black: return "Black";
    case SafeDyeColor::Gray: return "Gray";
    case SafeDyeColor::White: return "White";
    case SafeDyeColor::Pink: return "Pink";
    default: return "Invalid";
    }
}

// Wrapper class for DyeInfo
SafeDyeInfoClass::SafeDyeInfoClass()
    : dye_tint(0), dye1(0), dye2(0), dye3(0), dye4(0) {}

// Constructor from GW::DyeInfo
SafeDyeInfoClass::SafeDyeInfoClass(const GW::DyeInfo& dye_info)
    : dye_tint(dye_info.dye_tint),
    dye1(static_cast<int>(dye_info.dye1)),
    dye2(static_cast<int>(dye_info.dye2)),
    dye3(static_cast<int>(dye_info.dye3)),
    dye4(static_cast<int>(dye_info.dye4)) {}



// Convert to string
std::string SafeDyeInfoClass::to_string() const {
    return "DyeInfo { dye_tint: " + std::to_string(dye_tint) +
        ", dye1: " + dye1.to_string() +
        ", dye2: " + dye2.to_string() +
        ", dye3: " + dye3.to_string() +
        ", dye4: " + dye4.to_string() + " }";
}


void SafeItem::GetContext() {
    GW::Item* item = GW::Items::GetItemById(item_id);
    if (!item) return;

    agent_id = item->agent_id;
    agent_item_id = item->item_id;
    

    for (size_t i = 0; i < item->mod_struct_size; i++) {
        GW::ItemModifier* mod = &item->mod_struct[i];
        modifiers.push_back(SafeItemModifier(mod->mod));
    }
    IsCustomized = item->customized != nullptr;
    item_type = SafeItemTypeClass(static_cast<int>(item->type));
    dye_info = SafeDyeInfoClass(item->dye);
    value = item->value;
    interaction = item->interaction;
    model_id = item->model_id;
    item_formula = item->item_formula;
    is_material_salvageable = item->is_material_salvageable;
    quantity = item->quantity;
    equipped = item->equipped;
    profession = item->profession;
    slot = item->slot;
    is_stackable = item->GetIsStackable();
    is_inscribable = item->GetIsInscribable();
    is_material = item->GetIsMaterial();
    is_ZCoin = item->GetIsZcoin();

    ItemExtension item_ext(item);
    rarity = item_ext.GetRarity();
    Uses = item_ext.GetUses();
    IsIDKit = item_ext.IsIdentificationKit();
    IsSalvageKit = item_ext.IsSalvageKit();
    IsTome = item_ext.IsTome();
    IsLesserKit = item_ext.IsLesserKit();
    IsExpertSalvageKit = item_ext.IsExpertSalvageKit();
    IsPerfectSalvageKit = item_ext.IsPerfectSalvageKit();
    IsWeapon = item_ext.IsWeapon();
    IsArmor = item_ext.IsArmor();
    IsSalvagable = item_ext.IsSalvagable();
    IsInventoryItem = item_ext.IsInventoryItem();
    IsStorageItem = item_ext.IsStorageItem();
    IsRareMaterial = item_ext.IsRareMaterial();
    IsOfferedInTrade = item_ext.IsOfferedInTrade();
    //CanOfferToTrade = item_ext.CanOfferToTrade();
    IsSparkly = item_ext.IsSparkly();
    IsIdentified = item_ext.GetIsIdentified();
    IsPrefixUpgradable = item_ext.IsPrefixUpgradable();

}


std::string global_item_name;
bool item_name_ready;


std::string custom_WStringToString(const std::wstring& s)
{
    if (s.empty()) {
        return "Error In Name";
    }

    // Determine required size for UTF-8 conversion
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return "Error In Name";  // Handle failure

    // Perform the actual conversion
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &strTo[0], size_needed, nullptr, nullptr);

    // Remove the null terminator at the end (since WideCharToMultiByte includes it)
    strTo.resize(size_needed - 1);

    std::regex tagPattern(R"(<[^>]*>)");
    return std::regex_replace(strTo, tagPattern, "");

}




// Struct to store item name request status
struct ItemNameData {
    std::string item_name;
    bool name_ready = false;
};

// Global map for storing multiple item name requests
static std::unordered_map<uint32_t, ItemNameData> item_name_map;

void SafeItem::RequestName() {
    if (!item_id) return;  // Ensure item_id is valid
    item_name_map[item_id].name_ready = false;  // Reset flag for this item
    item_name_map[item_id].item_name = "";

    std::thread([item_id = this->item_id]() {
        GW::Item* item = GW::Items::GetItemById(item_id);
        if (!item) {
            // Immediately mark as ready with an empty string to prevent infinite waiting
            item_name_map[item_id].item_name = "No Item";
            item_name_map[item_id].name_ready = true;
            return;
        }

        std::wstring temp_name;
        auto start_time = std::chrono::steady_clock::now();

        GW::GameThread::Enqueue([item, &temp_name]() {
            GW::Items::AsyncGetItemName(item, temp_name);
            });

        while (temp_name.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() >= 1000) {
                item_name_map[item_id].item_name = "Timeout";
                item_name_map[item_id].name_ready = true;
                return;  // Timeout without modifying anything
            }
        }

        // Store the name inside the global map
        item_name_map[item_id].item_name = custom_WStringToString(temp_name);
        item_name_map[item_id].name_ready = true;  // Mark as ready

        }).detach();  // Fully detach the thread so it does not block anything
}

bool SafeItem::IsItemNameReady() {
    return item_name_map[item_id].name_ready;
}

std::string SafeItem::GetName() {
    return item_name_map[item_id].item_name;
}


int SafeBag::GetItemCount() const {
    return static_cast<int>(items.size());
}

std::vector<SafeItem> SafeBag::GetItems() const {
    return items;
}

SafeItem SafeBag::GetItemByIndex(int index) const {
    if (index < 0 || index >= GetItemCount()) {
        throw std::out_of_range("Item index out of range");
    }
    return items[index];
}

int SafeBag::GetSize() const {
    GW::Bag* gw_bag = GW::Items::GetBag(static_cast<GW::Constants::Bag>(ID));
    if (!gw_bag) return 0;  // Return 0 if no valid bag
    return gw_bag->items.size();  // Return the capacity of the bag
}

SafeItem* SafeBag::FindItemById(int item_id) {
    for (auto& item : items) {
        if (item.item_id == item_id) {
            return &item;
        }
    }
    return nullptr;
}

void SafeBag::GetContext() {
    GW::Bag* gw_bag = GW::Items::GetBag(static_cast<GW::Constants::Bag>(ID));
    if (!gw_bag) return;  // If no valid bag, return

    // Clear the current list of items before populating
    items.clear();

    // Fetch all items within the bag
    for (int i = 0; i < gw_bag->items.size(); ++i) {
        GW::Item* gw_item = gw_bag->items[i];
        if (gw_item) {
            SafeItem safe_item(gw_item->item_id);  // Create a SafeItem using item_id
            safe_item.GetContext();  // Populate additional data for the item
            items.emplace_back(safe_item);  // Add to bag's item list
        }
    }

    // Set other properties for this bag based on GW::Bag structure
    container_item = gw_bag->container_item;
    IsInventoryBag = gw_bag->IsInventoryBag();
    IsStorageBag = gw_bag->IsStorageBag();
    IsMaterialStorage = gw_bag->IsMaterialStorage();
    items_count = gw_bag->items_count;
}

void bind_SafeDyeColor(py::module_& m) {
    py::enum_<SafeDyeColor>(m, "DyeColor")
        .value("NoColor", SafeDyeColor::NoColor)
        .value("Blue", SafeDyeColor::Blue)
        .value("Green", SafeDyeColor::Green)
        .value("Purple", SafeDyeColor::Purple)
        .value("Red", SafeDyeColor::Red)
        .value("Yellow", SafeDyeColor::Yellow)
        .value("Brown", SafeDyeColor::Brown)
        .value("Orange", SafeDyeColor::Orange)
        .value("Silver", SafeDyeColor::Silver)
        .value("Black", SafeDyeColor::Black)
        .value("Gray", SafeDyeColor::Gray)
        .value("White", SafeDyeColor::White)
        .value("Pink", SafeDyeColor::Pink);
}

void bind_SafeItemType(py::module_& m) {
    py::enum_<SafeItemType>(m, "ItemType")
        .value("Salvage", SafeItemType::Salvage)
        .value("Axe", SafeItemType::Axe)
        .value("Bag", SafeItemType::Bag)
        .value("Boots", SafeItemType::Boots)
        .value("Bow", SafeItemType::Bow)
        .value("Bundle", SafeItemType::Bundle)
        .value("Chestpiece", SafeItemType::Chestpiece)
        .value("Rune_Mod", SafeItemType::Rune_Mod)
        .value("Usable", SafeItemType::Usable)
        .value("Dye", SafeItemType::Dye)
        .value("Materials_or_Zcoins", SafeItemType::Materials_Zcoins)
        .value("Offhand", SafeItemType::Offhand)
        .value("Gloves", SafeItemType::Gloves)
        .value("Hammer", SafeItemType::Hammer)
        .value("Headpiece", SafeItemType::Headpiece)
        .value("CC_Shards", SafeItemType::CC_Shards)
        .value("Key", SafeItemType::Key)
        .value("Leggings", SafeItemType::Leggings)
        .value("Gold_Coin", SafeItemType::Gold_Coin)
        .value("Quest_Item", SafeItemType::Quest_Item)
        .value("Wand", SafeItemType::Wand)
        .value("Shield", SafeItemType::Shield)
        .value("Staff", SafeItemType::Staff)
        .value("Sword", SafeItemType::Sword)
        .value("Kit", SafeItemType::Kit)
        .value("Trophy", SafeItemType::Trophy)
        .value("Scroll", SafeItemType::Scroll)
        .value("Daggers", SafeItemType::Daggers)
        .value("Present", SafeItemType::Present)
        .value("Minipet", SafeItemType::Minipet)
        .value("Scythze", SafeItemType::Scythe)
        .value("Spear", SafeItemType::Spear)
        .value("Storybook", SafeItemType::Storybook)
        .value("Costume", SafeItemType::Costume)
        .value("Costume_Headpiece", SafeItemType::Costume_Headpiece)
        .value("Unknown", SafeItemType::Unknown);
}

void bind_SalvageAllType(py::module_& m) {
    py::enum_<SalvageAllType>(m, "SalvageAllType")
        .value("Nothing", SalvageAllType::None)
        .value("White", SalvageAllType::White)
        .value("BlueAndLower", SalvageAllType::BlueAndLower)
        .value("PurpleAndLower", SalvageAllType::PurpleAndLower)
        .value("GoldAndLower", SalvageAllType::GoldAndLower);
}

void bind_IdentifyAllType(py::module_& m) {
    py::enum_<IdentifyAllType>(m, "IdentifyAllType")
        .value("Nothing", IdentifyAllType::None)
        .value("All", IdentifyAllType::All)
        .value("Blue", IdentifyAllType::Blue)
        .value("Purple", IdentifyAllType::Purple)
        .value("Gold", IdentifyAllType::Gold);
}

void bind_rarity(py::module_& m) {
    // Bind the Rarity enum
    py::enum_<GW::Constants::Rarity>(m, "Rarity")
        .value("White", GW::Constants::Rarity::White)
        .value("Blue", GW::Constants::Rarity::Blue)
        .value("Purple", GW::Constants::Rarity::Purple)
        .value("Gold", GW::Constants::Rarity::Gold)
        .value("Green", GW::Constants::Rarity::Green);
}


void bind_SafeItemModifier(pybind11::module_& m) {
    pybind11::class_<SafeItemModifier>(m, "ItemModifier")
        .def(pybind11::init<uint32_t>())
        .def("GetIdentifier", &SafeItemModifier::get_identifier)
        .def("GetArg1", &SafeItemModifier::get_arg1)
        .def("GetArg2", &SafeItemModifier::get_arg2)
        .def("GetArg", &SafeItemModifier::get_arg)
        .def("IsValid", &SafeItemModifier::is_valid)
        .def("GetModBits", &SafeItemModifier::get_mod_bits)
        .def("ToString", &SafeItemModifier::to_string);
}

void bind_SafeItemTypeClass(py::module_& m) {
    py::class_<SafeItemTypeClass>(m, "ItemTypeClass")
        .def(py::init<int>())  // Constructor
        .def("ToInt", &SafeItemTypeClass::to_int)
        .def("GetName", &SafeItemTypeClass::get_name)
        .def("__eq__", &SafeItemTypeClass::operator==)  // Equality operator
        .def("__ne__", &SafeItemTypeClass::operator!=);  // Inequality operator
}

void bind_SafeDyeColorClass(py::module_& m) {
    py::class_<SafeDyeColorClass>(m, "DyeColorClass")
        .def(py::init<int>())  // Constructor
        .def("ToInt", &SafeDyeColorClass::to_int)
        .def("ToString", &SafeDyeColorClass::to_string)
        .def("__eq__", &SafeDyeColorClass::operator==)  // Equality operator
        .def("__ne__", &SafeDyeColorClass::operator!=);  // Inequality operator
}

void bind_SafeDyeInfoClass(py::module_& m) {
    py::class_<SafeDyeInfoClass>(m, "DyeInfo")
        .def(py::init<>())  // Constructor
        .def(py::init<const GW::DyeInfo&>())  // Constructor
        .def_readonly("dye_tint", &SafeDyeInfoClass::dye_tint)
        .def_readonly("dye1", &SafeDyeInfoClass::dye1)
        .def_readonly("dye2", &SafeDyeInfoClass::dye2)
        .def_readonly("dye3", &SafeDyeInfoClass::dye3)
        .def_readonly("dye4", &SafeDyeInfoClass::dye4)
        .def("ToString", &SafeDyeInfoClass::to_string)
        .def("__eq__", &SafeDyeInfoClass::operator==)  // Equality operator
        .def("__ne__", &SafeDyeInfoClass::operator!=);  // Inequality operator
}

void bind_SafeItem(py::module_& m) {
    py::class_<SafeItem>(m, "PyItem")
        .def(py::init<int>())  // Constructor with item_id
        .def("GetContext", &SafeItem::GetContext)  // GetContext method
		.def("RequestName", &SafeItem::RequestName)
		.def("IsItemNameReady", &SafeItem::IsItemNameReady)
		.def("GetName", &SafeItem::GetName)
        .def_readonly("item_id", &SafeItem::item_id)
        .def_readonly("agent_id", &SafeItem::agent_id)
        .def_readonly("agent_item_id", &SafeItem::agent_item_id)
        .def_readonly("name", &SafeItem::name)
        .def_readonly("modifiers", &SafeItem::modifiers)
        .def_readonly("is_customized", &SafeItem::IsCustomized)
        .def_readonly("item_type", &SafeItem::item_type)
        .def_readonly("dye_info", &SafeItem::dye_info)
        .def_readonly("value", &SafeItem::value)
        .def_readonly("interaction", &SafeItem::interaction)
        .def_readonly("model_id", &SafeItem::model_id)
        .def_readonly("item_formula", &SafeItem::item_formula)
        .def_readonly("is_material_salvageable", &SafeItem::is_material_salvageable)
        .def_readonly("quantity", &SafeItem::quantity)
        .def_readonly("equipped", &SafeItem::equipped)
        .def_readonly("profession", &SafeItem::profession)
        .def_readonly("slot", &SafeItem::slot)
        .def_readonly("is_stackable", &SafeItem::is_stackable)
        .def_readonly("is_inscribable", &SafeItem::is_inscribable)
        .def_readonly("is_material", &SafeItem::is_material)
        .def_readonly("is_zcoin", &SafeItem::is_ZCoin)
        .def_readonly("rarity", &SafeItem::rarity)
        .def_readonly("uses", &SafeItem::Uses)
        .def_readonly("is_id_kit", &SafeItem::IsIDKit)
        .def_readonly("is_salvage_kit", &SafeItem::IsSalvageKit)
        .def_readonly("is_tome", &SafeItem::IsTome)
        .def_readonly("is_lesser_kit", &SafeItem::IsLesserKit)
        .def_readonly("is_expert_salvage_kit", &SafeItem::IsExpertSalvageKit)
        .def_readonly("is_perfect_salvage_kit", &SafeItem::IsPerfectSalvageKit)
        .def_readonly("is_weapon", &SafeItem::IsWeapon)
        .def_readonly("is_armor", &SafeItem::IsArmor)
        .def_readonly("is_salvageable", &SafeItem::IsSalvagable)
        .def_readonly("is_inventory_item", &SafeItem::IsInventoryItem)
        .def_readonly("is_storage_item", &SafeItem::IsStorageItem)
        .def_readonly("is_rare_material", &SafeItem::IsRareMaterial)
        .def_readonly("is_offered_in_trade", &SafeItem::IsOfferedInTrade)
        .def_readonly("is_sparkly", &SafeItem::IsSparkly)
        .def_readonly("is_identified", &SafeItem::IsIdentified)
        .def_readonly("is_prefix_upgradable", &SafeItem::IsPrefixUpgradable)
        .def_readonly("is_suffix_upgradable", &SafeItem::IsSuffixUpgradable)
        .def_readonly("is_stackable", &SafeItem::IsStackable)
        .def_readonly("is_usable", &SafeItem::IsUsable)
        .def_readonly("is_tradable", &SafeItem::IsTradable)
        .def_readonly("is_inscription", &SafeItem::IsInscription)
        .def_readonly("is_rarity_blue", &SafeItem::IsRarityBlue)
        .def_readonly("is_rarity_purple", &SafeItem::IsRarityPurple)
        .def_readonly("is_rarity_green", &SafeItem::IsRarityGreen)
        .def_readonly("is_rarity_gold", &SafeItem::IsRarityGold);
}

PYBIND11_EMBEDDED_MODULE(PyItem, m) {
    bind_SafeDyeColor(m);
    bind_SafeItemType(m);
    bind_SalvageAllType(m);
    bind_IdentifyAllType(m);
    bind_rarity(m);
    bind_SafeItemModifier(m);
    bind_SafeItemTypeClass(m);
    bind_SafeDyeColorClass(m);
    bind_SafeDyeInfoClass(m);
    bind_SafeItem(m);
}

void bind_SafeBag(py::module_& m) {
    py::class_<SafeBag>(m, "Bag")
        .def(py::init<int, std::string>())  // Constructor with ID and name
        .def("GetItems", &SafeBag::GetItems)
        .def("GetItemByIndex", &SafeBag::GetItemByIndex)
        .def("GetItemCount", &SafeBag::GetItemCount)
        .def("GetSize", &SafeBag::GetSize)
        .def("FindItemById", &SafeBag::FindItemById, py::return_value_policy::reference)  // Returning a pointer
        .def("GetContext", &SafeBag::GetContext)

        // Exposing member variables as read-only
        .def_readonly("id", &SafeBag::ID)
        .def_readonly("name", &SafeBag::Name)
        .def_readonly("container_item", &SafeBag::container_item)
        .def_readonly("items_count", &SafeBag::items_count)
        .def_readonly("is_inventory_bag", &SafeBag::IsInventoryBag)
        .def_readonly("is_storage_bag", &SafeBag::IsStorageBag)
        .def_readonly("is_material_storage", &SafeBag::IsMaterialStorage);
}

void bind_Inventory(py::module_& m) {
    py::class_<Inventory>(m, "PyInventory")
        .def(py::init<>())  // Default constructor

        // Methods for interacting with the inventory
        .def("OpenXunlaiWindow", &Inventory::OpenXunlaiWindow)  // Open Xunlai storage window
        .def("GetIsStorageOpen", &Inventory::GetIsStorageOpen)  // Check if storage is open

        // Item actions: Pick up, drop, equip, use, destroy
        .def("PickUpItem", &Inventory::PickUpItem, py::arg("item_id"), py::arg("call_target") = false)  // Pick up an item
        .def("DropItem", &Inventory::DropItem, py::arg("item_id"), py::arg("quantity") = 1)  // Drop an item
        .def("EquipItem", &Inventory::EquipItem, py::arg("item_id"), py::arg("agent_id"))  // Equip an item
        .def("UseItem", &Inventory::UseItem, py::arg("item_id"))  // Use an item
        .def("DestroyItem", &Inventory::DestroyItem, py::arg("item_id"))  // Destroy an item
        .def("IdentifyItem", &Inventory::IdentifyItem, py::arg("id_kit_id"), py::arg("item_id"))  // Identify an item

        // Get item info
        .def("GetHoveredItemID", &Inventory::GetHoveredItemId)  // Get hovered item ID
        .def("GetGoldAmount", &Inventory::GetGoldAmount)  // Get amount of gold on character
        .def("GetGoldAmountInStorage", &Inventory::GetGoldAmountInStorage)  // Get gold amount in storage

        // Gold manipulation
        .def("DepositGold", &Inventory::DepositGold, py::arg("amount"))  // Deposit gold into storage
        .def("WithdrawGold", &Inventory::WithdrawGold, py::arg("amount"))  // Withdraw gold from storage
        .def("DropGold", &Inventory::DropGold, py::arg("amount"))  // Drop a certain amount of gold

        // Item manipulation within bags
        .def("MoveItem", &Inventory::MoveItem, py::arg("item_id"), py::arg("bag_id"), py::arg("slot"), py::arg("quantity") = 1)  // Move an item within a bag

        // Salvage methods
        .def("Salvage", &Inventory::Salvage, py::arg("salv_kit_id"), py::arg("item_id"))  // Start salvage process
        .def("AcceptSalvageWindow", &Inventory::AcceptSalvageWindow);  // Accept the salvage window
}



PYBIND11_EMBEDDED_MODULE(PyInventory, m) {
    bind_SafeBag(m);
    bind_Inventory(m);
}

