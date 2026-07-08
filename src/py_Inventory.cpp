#pragma once
#include "py_inventory.h"


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

void SafeBag::ResetContext() {
    ID = 0;
    Name = "";
    items.clear();
    container_item = 0;
    items_count = 0;
    IsInventoryBag = false;
    IsStorageBag = false;
    IsMaterialStorage = false;

}

void SafeBag::GetContext() {
    auto instance_type = GW::Map::GetInstanceType();
    bool is_map_ready = (GW::Map::GetIsMapLoaded()) && (!GW::Map::GetIsObserving()) && (instance_type != GW::Constants::InstanceType::Loading);

    if (!is_map_ready) {
        ResetContext();
        return;
    }

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
        .def("GetInventoryIDFromAgent", &Inventory::GetInventoryIDFromAgent, py::arg("agent_id"))
        .def("IsInventoryIDValid", &Inventory::IsInventoryIDValid, py::arg("inventory_id"))
        .def("GetEquippedItemID", &Inventory::GetEquippedItemID, py::arg("inventory_id"), py::arg("equip_slot"))
        .def("GetUpgradeSlot", &Inventory::GetUpgradeSlot, py::arg("upgrade_item_id"))
        .def("ValidateUpgrade", &Inventory::ValidateUpgrade, py::arg("target_item_id"), py::arg("upgrade_item_id"))
        .def("ApplyUpgrade", &Inventory::ApplyUpgrade, py::arg("inventory_id"), py::arg("target_item_id"), py::arg("upgrade_item_id"), py::arg("upgrade_slot") = -1)

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
