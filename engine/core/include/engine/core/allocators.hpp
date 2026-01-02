#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <new>

namespace engine::core {

// Linear allocator that resets each frame - extremely fast O(1) allocations
class FrameAllocator {
public:
    explicit FrameAllocator(size_t size = 4 * 1024 * 1024);  // 4MB default
    ~FrameAllocator();

    // Non-copyable, non-movable
    FrameAllocator(const FrameAllocator&) = delete;
    FrameAllocator& operator=(const FrameAllocator&) = delete;
    FrameAllocator(FrameAllocator&&) = delete;
    FrameAllocator& operator=(FrameAllocator&&) = delete;

    // Allocate memory (very fast, O(1))
    void* allocate(size_t size, size_t alignment = 16);

    // Allocate and construct object
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        if (ptr) {
            return new (ptr) T(std::forward<Args>(args)...);
        }
        return nullptr;
    }

    // Allocate array (elements are default constructed)
    template<typename T>
    T* create_array(size_t count) {
        if (count == 0) return nullptr;
        void* ptr = allocate(sizeof(T) * count, alignof(T));
        if (ptr) {
            T* arr = static_cast<T*>(ptr);
            for (size_t i = 0; i < count; ++i) {
                new (&arr[i]) T();
            }
            return arr;
        }
        return nullptr;
    }

    // Reset for new frame (very fast, just resets pointer)
    // WARNING: Does NOT call destructors - only use for POD types or types
    // where destruction is not needed
    void reset();

    // Stats
    size_t used() const { return m_offset; }
    size_t capacity() const { return m_capacity; }
    size_t peak_used() const { return m_peak; }
    size_t remaining() const { return m_capacity - m_offset; }

private:
    uint8_t* m_buffer;
    size_t m_capacity;
    size_t m_offset;
    size_t m_peak;
};

// Get per-thread frame allocator
FrameAllocator& get_frame_allocator();

// Reset all thread frame allocators (call at frame end)
void reset_frame_allocators();


// Pool allocator for fixed-size objects - O(1) alloc and free
template<typename T>
class PoolAllocator {
public:
    explicit PoolAllocator(size_t pool_size = 1024)
        : m_capacity(pool_size)
        , m_allocated(0)
    {
        // Ensure each slot can hold at least a pointer for the free list
        static_assert(sizeof(T) >= sizeof(void*),
            "Pool element must be at least pointer-sized for free list");

        m_pool = static_cast<T*>(::operator new(sizeof(T) * pool_size));
        m_free_list = nullptr;

        // Build free list (in reverse so first allocation gets slot 0)
        for (size_t i = pool_size; i > 0; --i) {
            FreeNode* node = reinterpret_cast<FreeNode*>(&m_pool[i - 1]);
            node->next = m_free_list;
            m_free_list = node;
        }
    }

    ~PoolAllocator() {
        ::operator delete(m_pool);
    }

    // Non-copyable
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    // Allocate single object slot (does not construct)
    T* allocate() {
        if (!m_free_list) {
            return nullptr;  // Pool exhausted
        }

        FreeNode* node = m_free_list;
        m_free_list = node->next;
        ++m_allocated;

        return reinterpret_cast<T*>(node);
    }

    // Deallocate object slot (does not destruct)
    void deallocate(T* ptr) {
        if (!ptr) return;

        FreeNode* node = reinterpret_cast<FreeNode*>(ptr);
        node->next = m_free_list;
        m_free_list = node;
        --m_allocated;
    }

    // Construct object in-place
    template<typename... Args>
    T* create(Args&&... args) {
        T* ptr = allocate();
        if (ptr) {
            new (ptr) T(std::forward<Args>(args)...);
        }
        return ptr;
    }

    // Destroy and deallocate
    void destroy(T* ptr) {
        if (ptr) {
            ptr->~T();
            deallocate(ptr);
        }
    }

    // Stats
    size_t allocated_count() const { return m_allocated; }
    size_t free_count() const { return m_capacity - m_allocated; }
    size_t capacity() const { return m_capacity; }

    // Reset all (destructors NOT called!)
    void clear() {
        m_free_list = nullptr;
        for (size_t i = m_capacity; i > 0; --i) {
            FreeNode* node = reinterpret_cast<FreeNode*>(&m_pool[i - 1]);
            node->next = m_free_list;
            m_free_list = node;
        }
        m_allocated = 0;
    }

private:
    struct FreeNode {
        FreeNode* next;
    };

    T* m_pool;
    size_t m_capacity;
    FreeNode* m_free_list;
    size_t m_allocated;
};


// Stack allocator with scoped markers for LIFO allocations
class StackAllocator {
public:
    explicit StackAllocator(size_t size);
    ~StackAllocator();

    // Non-copyable
    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;

    // Allocate memory
    void* allocate(size_t size, size_t alignment = 16);

    // Allocate and construct
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        if (ptr) {
            return new (ptr) T(std::forward<Args>(args)...);
        }
        return nullptr;
    }

    // Marker for scoped allocations
    using Marker = size_t;
    Marker get_marker() const { return m_offset; }

    // Free back to marker (destructors NOT called!)
    void free_to_marker(Marker marker);

    // Stats
    size_t used() const { return m_offset; }
    size_t capacity() const { return m_capacity; }

private:
    uint8_t* m_buffer;
    size_t m_capacity;
    size_t m_offset;
};


// RAII scope for stack allocator
class StackScope {
public:
    explicit StackScope(StackAllocator& alloc)
        : m_allocator(alloc)
        , m_marker(alloc.get_marker())
    {}

    ~StackScope() {
        m_allocator.free_to_marker(m_marker);
    }

    // Non-copyable
    StackScope(const StackScope&) = delete;
    StackScope& operator=(const StackScope&) = delete;

    // Allow moving
    StackScope(StackScope&& other) noexcept
        : m_allocator(other.m_allocator)
        , m_marker(other.m_marker)
    {
        other.m_marker = m_allocator.get_marker();  // Prevent double-free
    }

private:
    StackAllocator& m_allocator;
    StackAllocator::Marker m_marker;
};


// Aligned allocation helper
inline void* aligned_alloc(size_t size, size_t alignment) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void* ptr = nullptr;
    posix_memalign(&ptr, alignment, size);
    return ptr;
#endif
}

inline void aligned_free(void* ptr) {
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}


// Simple ring buffer for temporary allocations
class RingBuffer {
public:
    explicit RingBuffer(size_t size);
    ~RingBuffer();

    // Allocate from ring buffer (wraps around)
    void* allocate(size_t size, size_t alignment = 16);

    // Reset write position (for frame boundaries)
    void reset() { m_head = 0; }

    size_t capacity() const { return m_capacity; }

private:
    uint8_t* m_buffer;
    size_t m_capacity;
    size_t m_head;
};

} // namespace engine::core
