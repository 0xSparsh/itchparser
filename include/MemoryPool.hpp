#pragma once

// Fixed-capacity, single-threaded object pool with lazy memory commitment.
//
// The pool reserves 'capacity * sizeof(T)' of virtual address space up front
// with mmap(MAP_NORESERVE). Pages only become resident when a slot is first
// handed out, so an empty pool costs one mmap reservation and no resident
// pages - generous capacities are effectively free (see the pool constants
// in Types.hpp).
//
// Slots are handed out from a bump pointer until it reaches capacity; every
// released slot is recycled through an intrusive free list (LIFO). This keeps
// steady-state resident memory at roughly the peak live-slot footprint.
//
// MemoryPool does NOT construct or destroy T.
// The caller is responsible for placement-new construction
// and explicitly calling the destructor when needed.

#include "Types.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

#include <sys/mman.h>

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

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;

    ~MemoryPool() {
        if (storage_ != nullptr) {
            ::munmap(storage_, map_bytes_);
        }
    }

    // Create a pool with room for 'capacity' slots.
    // If the mmap reservation fails the pool stays empty and every
    // acquire() returns nullptr - callers already treat that as pool
    // exhaustion, so the failure is counted and reported, never fatal.
    explicit MemoryPool(std::size_t capacity)
        : capacity_(capacity)
        , map_bytes_(page_round(capacity * sizeof(T)))
    {
        if (map_bytes_ == 0) {
            return;   // zero capacity: acquire() always returns nullptr
        }

        // Reserve the address space, touch nothing. MAP_NORESERVE waives
        // swap/commit accounting for the pages we never write.
        void* p = ::mmap(nullptr, map_bytes_,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                         -1, 0);
        if (p == MAP_FAILED) {
            storage_ = nullptr;
            map_bytes_ = 0;
            capacity_ = 0;
            return;
        }

        // mmap memory is page-aligned, which satisfies any alignof(T).
        storage_ = static_cast<std::byte*>(p);
    }

    // Get one unused slot from the pool.
    // Returns nullptr when the pool is full.
    [[nodiscard]] T* acquire() noexcept {
        // Recycle the most recently released slot (LIFO - hot in cache).
        if (free_head_ != nullptr) [[likely]] {
            FreeNode* node = free_head_;
            free_head_ = node->next;
            ++used_;
            return reinterpret_cast<T*>(node);
        }

        // Fresh slot: hand out the next never-used slot. The first write
        // to its page faults it in; untouched pages stay unresided.
        if (bump_ < capacity_) {
            T* p = reinterpret_cast<T*>(storage_ + (bump_++ * sizeof(T)));
            ++used_;
            return p;
        }

        return nullptr;   // exhausted
    }

    // Return a slot back to the pool.
    // The caller must destroy the T object first if necessary.
    void release(T* p) noexcept {
        // Nothing to release.
        if (p == nullptr) [[unlikely]]
            return;

        --used_;

        // Treat the slot as a free-list node and push it to the front.
        FreeNode* node = reinterpret_cast<FreeNode*>(p);
        node->next  = free_head_;
        free_head_  = node;
    }

    // Return the total number of slots in the pool.
    [[nodiscard]] std::size_t capacity() const noexcept {
        return capacity_;
    }

    // Number of slots handed out from the bump region at least once.
    // The lazily-resident extent of the pool: with recycling, this stays
    // near the peak live count instead of growing with total traffic.
    [[nodiscard]] std::size_t resident_slots() const noexcept {
        return bump_;
    }

    // Number of slots currently unused. O(1): every acquire/release pair
    // is balanced by the used_ counter, so free = capacity - used.
    [[nodiscard]] std::size_t free_count() const noexcept {
        return capacity_ - used_;
    }

private:

    // A free slot uses the beginning of its memory to store
    // a pointer to the next free slot.
    // This structure exists only while the slot is unused.
    struct FreeNode {
        FreeNode* next;
    };

    // Round a byte count up to a whole page (mmap granularity).
    static std::size_t page_round(std::size_t bytes) noexcept {
        constexpr std::size_t kPage = 4096;
        return (bytes + kPage - 1) & ~(kPage - 1);
    }

    // Total number of slots in the pool.
    std::size_t capacity_;

    // Byte size of the mmap reservation (page-rounded).
    std::size_t map_bytes_;

    // Beginning of the reserved raw memory block (nullptr on mmap failure).
    std::byte* storage_ = nullptr;

    // Next never-handed-out slot index. Lazy allocation frontier: everything
    // below it has been handed out at least once.
    std::size_t bump_ = 0;

    // Slots currently handed out (acquire without matching release).
    // Capacity - used = free slots, maintained in O(1).
    std::size_t used_ = 0;

    // Intrusive free list head over released slots (LIFO reuse order).
    FreeNode* free_head_ = nullptr;
};

} // namespace itch
