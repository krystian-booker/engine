#include <engine/core/allocators.hpp>
#include <engine/core/log.hpp>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <mutex>
#include <unordered_map>

namespace engine::core {

// Helper to align a pointer
static inline size_t align_up(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

// ============================================================================
// FrameAllocator Implementation
// ============================================================================

FrameAllocator::FrameAllocator(size_t size)
    : m_capacity(size)
    , m_offset(0)
    , m_peak(0)
{
    m_buffer = static_cast<uint8_t*>(aligned_alloc(size, 16));
    if (!m_buffer) {
        log(LogLevel::Fatal, "FrameAllocator: Failed to allocate {} bytes", size);
    }
}

FrameAllocator::~FrameAllocator() {
    if (m_buffer) {
        aligned_free(m_buffer);
    }
}

void* FrameAllocator::allocate(size_t size, size_t alignment) {
    // Align the current offset
    size_t aligned_offset = align_up(m_offset, alignment);

    // Check if we have enough space
    if (aligned_offset + size > m_capacity) {
        log(LogLevel::Error, "FrameAllocator: Out of memory! Requested {} bytes, {} remaining",
            size, m_capacity - m_offset);
        return nullptr;
    }

    void* ptr = m_buffer + aligned_offset;
    m_offset = aligned_offset + size;

    // Track peak usage
    if (m_offset > m_peak) {
        m_peak = m_offset;
    }

    return ptr;
}

void FrameAllocator::reset() {
    m_offset = 0;
}

// Thread-local frame allocators
static thread_local std::unique_ptr<FrameAllocator> tls_frame_allocator;
static std::mutex s_allocators_mutex;
static std::vector<FrameAllocator*> s_all_allocators;

FrameAllocator& get_frame_allocator() {
    if (!tls_frame_allocator) {
        tls_frame_allocator = std::make_unique<FrameAllocator>();

        std::lock_guard<std::mutex> lock(s_allocators_mutex);
        s_all_allocators.push_back(tls_frame_allocator.get());
    }
    return *tls_frame_allocator;
}

void reset_frame_allocators() {
    std::lock_guard<std::mutex> lock(s_allocators_mutex);
    for (auto* alloc : s_all_allocators) {
        alloc->reset();
    }
}

// ============================================================================
// StackAllocator Implementation
// ============================================================================

StackAllocator::StackAllocator(size_t size)
    : m_capacity(size)
    , m_offset(0)
{
    m_buffer = static_cast<uint8_t*>(aligned_alloc(size, 16));
    if (!m_buffer) {
        log(LogLevel::Fatal, "StackAllocator: Failed to allocate {} bytes", size);
    }
}

StackAllocator::~StackAllocator() {
    if (m_buffer) {
        aligned_free(m_buffer);
    }
}

void* StackAllocator::allocate(size_t size, size_t alignment) {
    size_t aligned_offset = align_up(m_offset, alignment);

    if (aligned_offset + size > m_capacity) {
        log(LogLevel::Error, "StackAllocator: Out of memory! Requested {} bytes, {} remaining",
            size, m_capacity - m_offset);
        return nullptr;
    }

    void* ptr = m_buffer + aligned_offset;
    m_offset = aligned_offset + size;

    return ptr;
}

void StackAllocator::free_to_marker(Marker marker) {
    if (marker > m_offset) {
        log(LogLevel::Warn, "StackAllocator: Invalid marker {} (current offset {})",
            marker, m_offset);
        return;
    }
    m_offset = marker;
}

// ============================================================================
// RingBuffer Implementation
// ============================================================================

RingBuffer::RingBuffer(size_t size)
    : m_capacity(size)
    , m_head(0)
{
    m_buffer = static_cast<uint8_t*>(aligned_alloc(size, 16));
    if (!m_buffer) {
        log(LogLevel::Fatal, "RingBuffer: Failed to allocate {} bytes", size);
    }
}

RingBuffer::~RingBuffer() {
    if (m_buffer) {
        aligned_free(m_buffer);
    }
}

void* RingBuffer::allocate(size_t size, size_t alignment) {
    if (size > m_capacity) {
        log(LogLevel::Error, "RingBuffer: Allocation {} exceeds capacity {}", size, m_capacity);
        return nullptr;
    }

    size_t aligned_head = align_up(m_head, alignment);

    // Check if we need to wrap
    if (aligned_head + size > m_capacity) {
        // Wrap to beginning
        aligned_head = 0;
    }

    void* ptr = m_buffer + aligned_head;
    m_head = aligned_head + size;

    // Handle wrap around
    if (m_head >= m_capacity) {
        m_head = 0;
    }

    return ptr;
}

} // namespace engine::core
