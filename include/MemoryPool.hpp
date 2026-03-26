#pragma once
#include <atomic>
#include <new>
#include <vector>

/**
 * @brief A high-performance, lock-free memory pool for fixed-size objects.
 * * Uses a Thread-Local Storage (TLS) strategy for zero-contention allocations
 * and a Compare-and-Swap (CAS) lock-free stack for cross-thread deallocations.
 */
template <typename T, std::size_t BlockSize = 4096>
class HFTMemoryPool {
private:
    union Slot {
        T element;
        Slot* next;
    };

    // Fast-path: Each thread manages its own free list to avoid cache contention.
    static inline thread_local Slot* threadFreeList = nullptr;
    
    // Slow-path/Cross-thread: Objects returned by other threads sit here.
    std::atomic<Slot*> globalFreeList{nullptr};
    
    // Tracking blocks for bulk memory cleanup in the destructor.
    std::vector<Slot*> blocks;

    /**
     * @brief Allocates a new contiguous chunk of memory from the OS.
     * * This is called only when both thread-local and global free lists are empty.
     * In a production HFT system, this should be called during the 'warm-up' phase.
     */
    void allocateBlock() {
        Slot* newBlock = static_cast<Slot*>(operator new(BlockSize * sizeof(Slot)));
        blocks.push_back(newBlock);
        
        // Wire the new slots into the thread-local free list.
        for (std::size_t i = 0; i < BlockSize - 1; ++i) {
            newBlock[i].next = &newBlock[i + 1];
        }
        newBlock[BlockSize - 1].next = threadFreeList;
        threadFreeList = newBlock;
    }

public:
    /**
     * @brief Provides a pointer to a pre-allocated T object.
     * @return T* Pointer to the memory slot.
     * * Logic: 
     * 1. Check thread-local list (O(1), no locks).
     * 2. If empty, 'scavenge' everything from globalFreeList using an atomic exchange.
     * 3. If still empty, allocate a new block from the OS.
     */
    T* allocate() {
        if (!threadFreeList) {
            // Atomic exchange effectively "grabs" the entire linked list at once.
            Slot* head = globalFreeList.exchange(nullptr, std::memory_order_acquire);
            if (head) {
                threadFreeList = head;
            } else {
                allocateBlock();
            }
        }
        Slot* slot = threadFreeList;
        threadFreeList = slot->next;
        return reinterpret_cast<T*>(slot);
    }

    /**
     * @brief Thread-safe deallocation using a Lock-Free Stack (Treiber Stack).
     * @param ptr Pointer to the object to return to the pool.
     * * Uses a CAS (Compare-And-Swap) loop to ensure that even if multiple threads
     * return objects simultaneously, the globalFreeList remains consistent.
     */
    void deallocate(T* ptr) {
        Slot* node = reinterpret_cast<Slot*>(ptr);
        Slot* currentHead = globalFreeList.load(std::memory_order_relaxed);
        do {
            node->next = currentHead;
            // memory_order_release ensures 'node->next' is visible to the thread that pops it.
        } while (!globalFreeList.compare_exchange_weak(currentHead, node,
                                                      std::memory_order_release,
                                                      std::memory_order_relaxed));
    }

    ~HFTMemoryPool() {
        for (auto b : blocks) operator delete(b);
    }
};
