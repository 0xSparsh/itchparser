#pragma once

// Fixed-capacity, single-threaded object pool.
//
// The pool allocates one block of raw memory and manages unused
// slots using an intrusive singly-linked free list.
//
// MemoryPool does NOT construct or destroy T.
// The caller is responsible for placement-new construction
// and explicitly calling the destructor when needed.

#include "Types.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace itch {

template <typename T>

class MemoryPool {

    // Each free slot stores a pointer to the next free slot.
    // Therefore, T must be large enough to hold a pointer.
    static_assert(
        sizeof(T) >= sizeof(void*),
        "MemoryPool<T> requires sizeof(T) >= sizeof(void*)"
    );

public:

    // Create a pool containing 'capacity' slots.
    explicit MemoryPool(std::size_t capacity)
        : capacity_(capacity)

        // Allocate one contiguous block of raw memory.
        // The memory is aligned correctly for T.
        , storage_(static_cast<std::byte*>(
            ::operator new[](
                capacity * sizeof(T),
                std::align_val_t{alignof(T)}
            )))

        // Initially, the free-list head is empty.
        , head_(nullptr)
    {
        // Build the free list.
        // Every slot will point to the previously added slot.
        FreeNode* prev = nullptr;

        // Visit every slot in the allocated memory.
        for (std::size_t i = 0; i < capacity; ++i) {

            // Calculate the address of slot i.
            FreeNode* node = reinterpret_cast<FreeNode*>(
                storage_ + i * sizeof(T)
            );

            // Link this slot to the previous free slot.
            node->next = prev;

            // This slot becomes the new head.
            prev = node;
        }

        // The last slot we processed is now the first free slot.
        head_ = prev;
    }

    // Get one unused slot from the pool.
    // Returns nullptr when the pool is full.
    [[nodiscard]] T* acquire() noexcept {

        // Get the first free slot.
        FreeNode* node = head_;

        // No free slots remain.
        if (!node) [[unlikely]]
            return nullptr;

        // Remove the slot from the free list.
        head_ = node->next;

        // Treat the raw memory as a T*.
        // No T object is constructed here.
        return reinterpret_cast<T*>(node);
    }

    // Return a slot back to the pool.
    // The caller must destroy the T object first if necessary.
    void release(T* p) noexcept {

        // Nothing to release.
        if (!p) [[unlikely]]
            return;

        // Treat the slot as a free-list node.
        FreeNode* node = reinterpret_cast<FreeNode*>(p);

        // Put this slot at the front of the free list.
        node->next = head_;

        // This slot is now the first free slot.
        head_ = node;
    }

    // Return the total number of slots in the pool.
    std::size_t capacity() const noexcept {
        return capacity_;
    }

    // Count how many slots are currently unused.
    std::size_t free_count() const noexcept {

        std::size_t n = 0;

        // Walk through the free list and count its nodes.
        for (FreeNode* p = head_; p; p = p->next)
            ++n;

        return n;
    }

private:

    // A free slot uses the beginning of its memory to store
    // a pointer to the next free slot.
    // This structure exists only while the slot is unused.
    struct FreeNode {
        FreeNode* next;
    };

    // Total number of slots in the pool.
    const std::size_t capacity_;

    // Beginning of the raw memory block.
    std::byte* storage_;

    // First unused slot in the free list.
    FreeNode* head_;
};

} // namespace itch
