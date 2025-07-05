#pragma once
#include "Headers.h"

namespace py = pybind11;

// Class to handle merchant operations from Python
class Merchant {
public:

    Merchant() {
    }

    bool MerchantBuyItem(uint32_t item_id, uint32_t cost) {
        GW::Item* item = GW::Items::GetItemById(item_id);
        uint32_t count = 1;
        if (item)
        {
			transaction_complete = false; // Reset transaction completion status
            cost = cost * count;  // Adjust the total cost by multiplying with the count

            GW::Merchant::TransactionInfo give, recv;
            give.item_count = 0;
            give.item_ids = nullptr;
            give.item_quantities = nullptr;

            recv.item_count = count;  // Number of items to receive
            recv.item_ids = &item->item_id;  // Set the ID of the item to buy
            recv.item_quantities = nullptr;  // No specific quantities are defined

            // Perform the transaction to buy the item
            GW::GameThread::Enqueue([this, cost, give, recv] {
                GW::Merchant::TransactItems(GW::Merchant::TransactionType::MerchantBuy, cost, give, 0, recv);
                });

            return true;
        }

        return false;  // Return false if the item could not be found
    }

    bool TraderBuyItem(uint32_t item_id, uint32_t cost) {
        GW::Item* item = GW::Items::GetItemById(item_id);
        uint32_t count = 1;
        if (item) 
        {
            transaction_complete = false; // Reset transaction completion status
            cost = cost * count;  // Adjust the total cost by multiplying with the count

            GW::Merchant::TransactionInfo give, recv;
            give.item_count = 0;
            give.item_ids = nullptr;
            give.item_quantities = nullptr;

            recv.item_count = count;  // Number of items to receive
            recv.item_ids = &item->item_id;  // Set the ID of the item to buy
            recv.item_quantities = nullptr;  // No specific quantities are defined

            // Perform the transaction to buy the item
            GW::GameThread::Enqueue([this, cost, give, recv] {
                GW::Merchant::TransactItems(GW::Merchant::TransactionType::TraderBuy, cost, give, 0, recv);
                });
            
            return true;
        }

        return false;  // Return false if the item could not be found
    }

    bool CrafterBuyItems(uint32_t item_id, uint32_t cost,
        const std::vector<uint32_t>& give_item_ids,
        const std::vector<uint32_t>& give_item_quantities) {
        // Ensure input vectors are valid and of the same size.
        if (give_item_ids.size() != give_item_quantities.size()) {
            return false; // Or handle the error appropriately.
        }
        transaction_complete = false; // Reset transaction completion status

        // Convert vectors to raw pointers.
        uint32_t* item_ids_ptr = give_item_ids.empty() ? nullptr : const_cast<uint32_t*>(give_item_ids.data());
        uint32_t* item_quantities_ptr = give_item_quantities.empty() ? nullptr : const_cast<uint32_t*>(give_item_quantities.data());

        // Create TransactionInfo structures.
        GW::Merchant::TransactionInfo give_info;
        give_info.item_count = static_cast<uint32_t>(give_item_ids.size());
        give_info.item_ids = item_ids_ptr;
        give_info.item_quantities = item_quantities_ptr;

        // TransactionInfo for receiving items (adjust as necessary).
        GW::Merchant::TransactionInfo recv_info;
        recv_info.item_count = 1; // Assuming one item being received.
        recv_info.item_ids = &item_id;
        recv_info.item_quantities = nullptr; // No specific quantities.

        // Call the original function.
        return TransactItems(GW::Merchant::TransactionType::CrafterBuy, cost, give_info, 0, recv_info);
    }

    bool CollectorBuyItems(uint32_t item_id, uint32_t cost,
        const std::vector<uint32_t>& give_item_ids,
        const std::vector<uint32_t>& give_item_quantities) {
        // Ensure input vectors are valid and of the same size.
        if (give_item_ids.size() != give_item_quantities.size()) {
            return false; // Or handle the error appropriately.
        }

        // Convert vectors to raw pointers.
        uint32_t* item_ids_ptr = give_item_ids.empty() ? nullptr : const_cast<uint32_t*>(give_item_ids.data());
        uint32_t* item_quantities_ptr = give_item_quantities.empty() ? nullptr : const_cast<uint32_t*>(give_item_quantities.data());

        transaction_complete = false; // Reset transaction completion status

        // Create TransactionInfo structures.
        GW::Merchant::TransactionInfo give_info;
        give_info.item_count = static_cast<uint32_t>(give_item_ids.size());
        give_info.item_ids = item_ids_ptr;
        give_info.item_quantities = item_quantities_ptr;

        // TransactionInfo for receiving items (adjust as necessary).
        GW::Merchant::TransactionInfo recv_info;
        recv_info.item_count = 1; // Assuming one item being received.
        recv_info.item_ids = &item_id;
        recv_info.item_quantities = nullptr; // No specific quantities.

        // Call the original function.
        return TransactItems(GW::Merchant::TransactionType::CollectorBuy, cost, give_info, 0, recv_info);
    }

    bool MerchantSellItem(uint32_t item_id, uint32_t price) {
        GW::Item* item = GW::Items::GetItemById(item_id);
        uint32_t count = 1;
        if (item)
        {
            transaction_complete = false; // Reset transaction completion status
            price = price * count;  // Calculate the total price based on quantity

            GW::Merchant::TransactionInfo give, recv;
            give.item_count = count;  // The number of items being sold
            give.item_ids = &item->item_id;  // Set the ID of the item to sell
            //give.item_ids = &item_id;  // Set the ID of the item to sell
            give.item_quantities = nullptr;  // No specific quantities are defined

            recv.item_count = 0;  // No items are being received
            recv.item_ids = nullptr;
            recv.item_quantities = nullptr;

            // Perform the transaction to sell the item
            GW::GameThread::Enqueue([this, price, give, recv] {
                GW::Merchant::TransactItems(GW::Merchant::TransactionType::MerchantSell, 0, give, price, recv);
                });

            return true;
        }

        return false;  // Return false if the item could not be found
    }

    bool TraderSellItem(uint32_t item_id, uint32_t price) {
        GW::Item* item = GW::Items::GetItemById(item_id);
        uint32_t count = 1;
        if (item) 
        {
            transaction_complete = false; // Reset transaction completion status
            price = price * count;  // Calculate the total price based on quantity

            GW::Merchant::TransactionInfo give, recv;
            give.item_count = count;  // The number of items being sold
            give.item_ids = &item->item_id;  // Set the ID of the item to sell
            //give.item_ids = &item_id;  // Set the ID of the item to sell
            give.item_quantities = nullptr;  // No specific quantities are defined

            recv.item_count = 0;  // No items are being received
            recv.item_ids = nullptr;
            recv.item_quantities = nullptr;

            // Perform the transaction to sell the item
            GW::GameThread::Enqueue([this, price, give, recv] {
                GW::Merchant::TransactItems(GW::Merchant::TransactionType::TraderSell, 0, give, price, recv);
                });
            
            return true;
        }

        return false;  // Return false if the item could not be found
    }


    // Perform a request quote for the item based on its ID
    bool TraderRequestQuote(uint32_t item_id) {

        GW::Item* item = GW::Items::GetItemById(item_id);

        if (item) 
        {
            quoted_value = -1; // Reset the quoted value
            
            GW::Merchant::QuoteInfo give, recv;
            give.unknown = 0;
            give.item_count = 0;
            give.item_ids = nullptr;
            recv.unknown = 0;
            recv.item_count = 1;
            recv.item_ids = &item->item_id;
            //recv.item_ids = &item_id;

            GW::GameThread::Enqueue([this, give, recv] {
                GW::Merchant::RequestQuote(GW::Merchant::TransactionType::TraderBuy, give, recv);
                });
            
            return true;
        }
        return false;
    }

    bool TraderRequestSellQuote(uint32_t item_id) {
        GW::Item* item = GW::Items::GetItemById(item_id);

        if (item)
        {
            quoted_value = -1; // Reset the quoted value
            
            GW::Merchant::QuoteInfo give, recv;
            recv.unknown = 0;
            recv.item_count = 0;
            recv.item_ids = nullptr;
            give.unknown = 0;
            give.item_count = 1;
            give.item_ids = &item->item_id;
            //recv.item_ids = &item_id;

            GW::GameThread::Enqueue([this, give, recv] {
                GW::Merchant::RequestQuote(GW::Merchant::TransactionType::TraderSell, give, recv);
                });

            return true;
        }
        return false;
    }

    // Get the last quoted price
    int GetQuotedValue() const {
        return quoted_value;
    }

    uint32_t GetQuotedItemID() const {
        return quoted_item_id;
    }

    // Check if a transaction is complete
    bool IsTransactionComplete() const {
        return transaction_complete;
    }

    // Get the list of merchant items queried
    std::vector<uint32_t> GetTraderItems() const {
        return merch_items;
    }

    std::vector<uint32_t> GetTraderItems2() const {
        return merch_items2;
    }

    std::vector<uint32_t> GetMerchantItems() const {
        return merchant_window_items;
    }

    // Update method to be called from Python to check status
    void Update() {
        // This could be used to monitor transaction progress or reset state if necessary
    }

private:

};

