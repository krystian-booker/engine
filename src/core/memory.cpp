#include "memory.h"
#include "platform/platform.h"
#include <cstring>

// ============================================================================
// LinearAllocator Implementation
// ============================================================================

LinearAllocator::LinearAllocator()
    : buffer(nullptr)
    , capacity(0)
    , offset(0)
    , highWaterMark(0)
{
}

LinearAllocator::~LinearAllocator() {
    if (buffer) {
        Shutdown();
    }
}

void LinearAllocator::Init(size_t size) {
    ENGINE_ASSERT(buffer == nullptr && "LinearAllocator already initialized");
    ENGINE_ASSERT(size > 0 && "LinearAllocator size must be > 0");

    buffer = Platform::VirtualAlloc(size);
    ENGINE_ASSERT(buffer != nullptr && "Failed to allocate memory for LinearAllocator");

    capacity = size;
    offset = 0;
    highWaterMark = 0;
}

void* LinearAllocator::Alloc(size_t size, size_t align) {
    ENGINE_ASSERT(buffer != nullptr && "LinearAllocator not initialized");
    ENGINE_ASSERT(size > 0 && "Allocation size must be > 0");
    ENGINE_ASSERT((align & (align - 1)) == 0 && "Alignment must be power of 2");

    // Calculate aligned offset
    size_t alignedOffset = ALIGN_UP(offset, align);

    // Check for overflow: return nullptr instead of asserting
    size_t newOffset = alignedOffset + size;
    if (newOffset > capacity) {
        return nullptr;
    }

    // Calculate pointer to aligned memory
    void* ptr = static_cast<char*>(buffer) + alignedOffset;

    // Update offset
    offset = newOffset;

    // Update high-water mark if needed
    if (offset > highWaterMark) {
        highWaterMark = offset;
    }

    return ptr;
}

void LinearAllocator::Reset() {
    ENGINE_ASSERT(buffer != nullptr && "LinearAllocator not initialized");

    // High-water mark is already updated in Alloc()
    offset = 0;
}

void LinearAllocator::Shutdown() {
    ENGINE_ASSERT(buffer != nullptr && "LinearAllocator not initialized");

    Platform::VirtualFree(buffer, capacity);

    buffer = nullptr;
    capacity = 0;
    offset = 0;
    // Keep high-water mark for telemetry even after shutdown
}
