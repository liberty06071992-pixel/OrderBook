#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include "MemoryPool.hpp"

// Type aliasing for clarity and easy modification (e.g., switching to double)
using Price = int64_t;
using Quantity = uint32_t;
using OrderId = uint64_t;

/**
 * @brief Represents a single limit order in the system.
 */
struct Order {
    OrderId id;
    Price price;
    Quantity quantity;
    Order* next; // Required for the MemoryPool's internal linked list
};

/**
 * @brief High-performance Limit Order Book (LOB).
 * * Optimized for O(log N) insertion using binary search on contiguous memory
 * levels. Uses a custom MemoryPool to eliminate heap jitter during order entry.
 */
class LimitOrderBook {
private:
    struct Level {
        Price price;
        Quantity total_quantity;
        std::vector<Order*> orders; // List of orders at this specific price
    };

    std::vector<Level> bids; // Sorted Descending (Best Bid at front)
    std::vector<Level> asks; // Sorted Ascending (Best Ask at front)
    HFTMemoryPool<Order> pool;

public:
    LimitOrderBook() {
        // HFT Best Practice: Pre-reserve capacity to avoid re-allocations
        // and data copying during the trading session.
        bids.reserve(1024);
        asks.reserve(1024);
    }

    /**
     * @brief Adds a new order to the book and maintains price-time priority.
     * * @param isBid True for Buy, False for Sell.
     * * Implementation: Uses std::lower_bound to perform a cache-friendly binary 
     * search for the price level. If the level doesn't exist, it is inserted
     * while maintaining the sorted order of the vector.
     */
    void addOrder(OrderId id, Price price, Quantity qty, bool isBid) {
        // Fast allocation from our lock-free pool
        Order* newOrder = pool.allocate();
        newOrder->id = id;
        newOrder->price = price;
        newOrder->quantity = qty;

        auto& side = isBid ? bids : asks;
        
        // Binary search for the correct price level position
        auto it = std::lower_bound(side.begin(), side.end(), price, 
            [isBid](const Level& lvl, Price p) {
                return isBid ? lvl.price > p : lvl.price < p; 
            });

        // If level exists, append to the end (Time Priority)
        if (it != side.end() && it->price == price) {
            it->total_quantity += qty;
            it->orders.push_back(newOrder);
        } else {
            // New price level: vector insertion (O(N) worst case, but O(log N) search)
            side.insert(it, Level{price, qty, {newOrder}});
        }
    }
};
