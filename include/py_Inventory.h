#pragma once
#include "Headers.h"


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
    void ResetContext();
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

    int GetInventoryIDFromAgent(int agent_id) {
        return static_cast<int>(GW::Items::GetInventoryIDFromAgent(static_cast<uint32_t>(agent_id)));
    }

    bool IsInventoryIDValid(int inventory_id) {
        return GW::Items::IsInventoryIDValid(static_cast<uint32_t>(inventory_id));
    }

    int GetEquippedItemID(int inventory_id, int equip_slot) {
        return static_cast<int>(GW::Items::GetEquippedItemID(
            static_cast<uint32_t>(inventory_id),
            static_cast<uint32_t>(equip_slot)));
    }

    int GetUpgradeSlot(int upgrade_item_id) {
        GW::Item* item = GW::Items::GetItemById(static_cast<uint32_t>(upgrade_item_id));
        return static_cast<int>(GW::Items::GetUpgradeSlot(item));
    }

    bool ValidateUpgrade(int target_item_id, int upgrade_item_id) {
        if (target_item_id == upgrade_item_id)
            return false;
        return GW::Items::ValidateUpgrade(
            static_cast<uint32_t>(target_item_id),
            static_cast<uint32_t>(upgrade_item_id));
    }

    bool ApplyUpgrade(int inventory_id, int target_item_id, int upgrade_item_id, int upgrade_slot = -1) {
        const uint32_t target_inventory_id = static_cast<uint32_t>(inventory_id);
        const uint32_t target_item_id_u32 = static_cast<uint32_t>(target_item_id);
        const uint32_t upgrade_item_id_u32 = static_cast<uint32_t>(upgrade_item_id);
        const bool derive_upgrade_slot = upgrade_slot < 0;
        uint32_t upgrade_slot_u32 = derive_upgrade_slot ? 0xffffffff : static_cast<uint32_t>(upgrade_slot);

        if (!(target_inventory_id && target_item_id_u32 && upgrade_item_id_u32))
            return false;
        if (target_item_id_u32 == upgrade_item_id_u32)
            return false;
        if (!GW::Items::IsInventoryIDValid(target_inventory_id))
            return false;
        if (derive_upgrade_slot) {
            const auto upgrade_item = GW::Items::GetItemById(upgrade_item_id_u32);
            upgrade_slot_u32 = GW::Items::GetUpgradeSlot(upgrade_item);
            if (!upgrade_slot_u32)
                return false;
        }
        if (!GW::Items::ValidateUpgrade(target_item_id_u32, upgrade_item_id_u32))
            return false;

        return GW::Items::ApplyUpgrade(
            target_inventory_id,
            target_item_id_u32,
            upgrade_item_id_u32,
            upgrade_slot_u32);
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
