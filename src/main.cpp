#include <iostream>
#include <chrono>
#include "OrderBook.hpp"

int main() {
    LimitOrderBook lob;

    auto start = std::chrono::high_resolution_clock::now();

    // Simulate high-volume traffic
    for (int i = 0; i < 100000; ++i) {
        lob.addOrder(i, 15000 + (i % 10), 10, i % 2 == 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::nano> elapsed = end - start;

    std::cout << "Processed 100k orders in: " << elapsed.count() << " ns\n";
    std::cout << "Average latency per order: " << elapsed.count() / 100000 << " ns\n";

    return 0;
}
