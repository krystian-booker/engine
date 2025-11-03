#pragma once

#include "types.h"
#include <cstddef>

// ============================================================================
// LinearAllocator - Per-Frame/Stack Allocator
// ============================================================================
// Fast linear allocation for temporary per-frame data.
// Supports aligned allocations and tracks high-water mark for telemetry.
// Must be reset every frame. Not thread-safe.

class LinearAllocator {
    void* buffer;
    size_t capacity;
    size_t offset;
    size_t highWaterMark;

public:
    LinearAllocator();
    ~LinearAllocator();

    /// Initialize the allocator with a backing buffer of the specified size
    /// Uses Platform::VirtualAlloc for memory allocation
    void Init(size_t size);

    /// Allocate memory with specified size and alignment
    /// Returns nullptr if allocation would overflow capacity
    /// Asserts in debug builds on overflow
    void* Alloc(size_t size, size_t align = 16);

    /// Reset the allocator offset to 0 for next frame
    /// Updates high-water mark before resetting
    void Reset();

    /// Free the backing buffer
    void Shutdown();

    /// Get the peak memory usage since initialization
    size_t GetHighWaterMark() const { return highWaterMark; }

    /// Get current offset (current usage)
    size_t GetCurrentOffset() const { return offset; }

    /// Get total capacity
    size_t GetCapacity() const { return capacity; }
};

// ============================================================================
// PoolAllocator - Fixed-Size Component Pool
// ============================================================================
// Efficient allocator for fixed-size objects (e.g., game components).
// Grows dynamically by allocating new blocks as needed.
// Uses bitmask-based freelist for O(1) allocation/deallocation.
// Includes generation counters for safe handle invalidation.

template<typename T, size_t BlockSize = 64>
class PoolAllocator {
    struct Block {
        T items[BlockSize];
        u64 freelist_bitmask;  // 1 = free, 0 = allocated
        Block* next;

        Block() : next(nullptr) {
            // All slots start as free (all bits set to 1)
            static_assert(BlockSize <= 64, "BlockSize must be <= 64 for u64 bitmask");

            // Set only the bits corresponding to actual slots
            if constexpr (BlockSize == 64) {
                freelist_bitmask = ~0ULL;
            } else {
                freelist_bitmask = (1ULL << BlockSize) - 1;
            }
        }
    };

    Block* head;
    u32 generation;

public:
    PoolAllocator() : head(nullptr), generation(0) {}

    ~PoolAllocator() {
        // Free all blocks
        Block* current = head;
        while (current) {
            Block* next = current->next;
            delete current;
            current = next;
        }
    }

    /// Allocate a single item from the pool
    /// Automatically grows by allocating new blocks if needed
    /// Returns nullptr on failure (should be rare)
    T* Alloc() {
        // Try to find a free slot in existing blocks
        Block* current = head;
        while (current) {
            if (current->freelist_bitmask != 0) {
                // Found a block with free slots
                // Find first free bit using count trailing zeros
                u64 mask = current->freelist_bitmask;
                u32 index = CountTrailingZeros(mask);

                ENGINE_ASSERT(index < BlockSize);

                // Mark slot as allocated (clear bit)
                current->freelist_bitmask &= ~(1ULL << index);

                return &current->items[index];
            }
            current = current->next;
        }

        // No free slots found, allocate a new block
        Block* newBlock = new Block();
        if (!newBlock) {
            return nullptr;
        }

        // Insert at head
        newBlock->next = head;
        head = newBlock;

        // Allocate from first slot
        newBlock->freelist_bitmask &= ~(1ULL << 0);
        return &newBlock->items[0];
    }

    /// Free an item back to the pool
    /// Increments generation counter for handle invalidation
    void Free(T* item) {
        if (!item) return;

        // Find which block contains this item
        Block* current = head;
        while (current) {
            // Check if item is within this block's memory range
            uintptr_t blockStart = reinterpret_cast<uintptr_t>(current->items);
            uintptr_t blockEnd = blockStart + sizeof(T) * BlockSize;
            uintptr_t itemAddr = reinterpret_cast<uintptr_t>(item);

            if (itemAddr >= blockStart && itemAddr < blockEnd) {
                // Calculate index within block
                size_t index = (itemAddr - blockStart) / sizeof(T);
                ENGINE_ASSERT(index < BlockSize);

                // Mark slot as free (set bit)
                current->freelist_bitmask |= (1ULL << index);

                // Increment generation counter
                generation++;
                return;
            }

            current = current->next;
        }

        // Item not found in any block - this is a bug
        ENGINE_ASSERT(false && "Attempted to free item not owned by this pool");
    }

    /// Get the current generation counter
    u32 GetGeneration() const { return generation; }

private:
    /// Count trailing zeros in a 64-bit value
    /// Used to find the first free slot in the bitmask
    static u32 CountTrailingZeros(u64 value) {
        if (value == 0) return 64;

        u32 count = 0;
        while ((value & 1) == 0) {
            value >>= 1;
            count++;
        }
        return count;
    }
};
