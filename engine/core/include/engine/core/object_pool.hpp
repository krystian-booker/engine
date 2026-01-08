#pragma once

#include <vector>
#include <memory>
#include <unordered_set>
#include <functional>
#include <cstddef>
#include <cassert>

namespace engine::core {

// ============================================================================
// ObjectPool - Generic object pool for efficient resource reuse
// ============================================================================

template<typename T>
class ObjectPool {
public:
    struct Config {
        size_t initial_size = 16;       // Pre-allocate this many objects
        size_t max_size = 1024;         // Hard limit (0 = unlimited)
        size_t growth_count = 16;       // Expand by this many when exhausted
        bool auto_expand = true;        // Automatically grow when empty
    };

    explicit ObjectPool(Config config = {})
        : m_config(config)
    {
        if (m_config.initial_size > 0) {
            expand(m_config.initial_size);
        }
    }

    ~ObjectPool() = default;

    // Non-copyable
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // Movable
    ObjectPool(ObjectPool&&) noexcept = default;
    ObjectPool& operator=(ObjectPool&&) noexcept = default;

    // ========================================================================
    // Acquire/Release
    // ========================================================================

    // Acquire an object from the pool
    // Returns nullptr if pool is exhausted and can't expand
    T* acquire() {
        if (m_available.empty()) {
            if (!m_config.auto_expand) {
                ++m_stats_exhausted;
                return nullptr;
            }
            if (m_config.max_size > 0 && m_storage.size() >= m_config.max_size) {
                ++m_stats_exhausted;
                return nullptr;
            }
            expand(m_config.growth_count);
        }

        T* obj = m_available.back();
        m_available.pop_back();
        m_active.insert(obj);

        ++m_stats_acquire_count;
        if (m_active.size() > m_stats_peak_active) {
            m_stats_peak_active = m_active.size();
        }

        if (m_on_acquire) {
            m_on_acquire(*obj);
        }

        return obj;
    }

    // Acquire with in-place construction arguments
    template<typename... Args>
    T* acquire(Args&&... args) {
        T* obj = acquire();
        if (obj) {
            *obj = T(std::forward<Args>(args)...);
        }
        return obj;
    }

    // Release an object back to the pool
    void release(T* obj) {
        if (!obj) return;

        auto it = m_active.find(obj);
        if (it == m_active.end()) {
            // Object not from this pool or already released
            assert(false && "Releasing object not owned by this pool");
            return;
        }

        m_active.erase(it);

        if (m_on_release) {
            m_on_release(*obj);
        }

        m_available.push_back(obj);
        ++m_stats_release_count;
    }

    // ========================================================================
    // Pool Management
    // ========================================================================

    // Pre-warm the pool with additional objects
    void warm(size_t count) {
        expand(count);
    }

    // Clear all pooled objects (WARNING: invalidates active pointers!)
    void clear() {
        m_available.clear();
        m_active.clear();
        m_storage.clear();
        m_stats_acquire_count = 0;
        m_stats_release_count = 0;
        m_stats_peak_active = 0;
        m_stats_exhausted = 0;
    }

    // ========================================================================
    // Callbacks
    // ========================================================================

    using ResetCallback = std::function<void(T&)>;

    // Called when an object is acquired (for initialization/reset)
    void set_on_acquire(ResetCallback callback) {
        m_on_acquire = std::move(callback);
    }

    // Called when an object is released (for cleanup)
    void set_on_release(ResetCallback callback) {
        m_on_release = std::move(callback);
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    size_t available_count() const { return m_available.size(); }
    size_t active_count() const { return m_active.size(); }
    size_t total_count() const { return m_storage.size(); }
    size_t peak_active() const { return m_stats_peak_active; }
    size_t acquire_count() const { return m_stats_acquire_count; }
    size_t release_count() const { return m_stats_release_count; }
    size_t exhausted_count() const { return m_stats_exhausted; }

    const Config& config() const { return m_config; }

private:
    void expand(size_t count) {
        size_t target = m_storage.size() + count;

        // Respect max size limit
        if (m_config.max_size > 0 && target > m_config.max_size) {
            target = m_config.max_size;
        }

        size_t actual_count = target - m_storage.size();
        if (actual_count == 0) return;

        m_storage.reserve(target);
        m_available.reserve(m_available.size() + actual_count);

        for (size_t i = 0; i < actual_count; ++i) {
            m_storage.push_back(std::make_unique<T>());
            m_available.push_back(m_storage.back().get());
        }

        ++m_stats_expand_count;
    }

    Config m_config;

    // Storage owns all objects
    std::vector<std::unique_ptr<T>> m_storage;

    // Free list of available objects
    std::vector<T*> m_available;

    // Set of currently active objects
    std::unordered_set<T*> m_active;

    // Callbacks
    ResetCallback m_on_acquire;
    ResetCallback m_on_release;

    // Statistics
    size_t m_stats_peak_active = 0;
    size_t m_stats_acquire_count = 0;
    size_t m_stats_release_count = 0;
    size_t m_stats_expand_count = 0;
    size_t m_stats_exhausted = 0;
};

// ============================================================================
// PooledHandle - RAII wrapper for automatic release
// ============================================================================

template<typename T>
class PooledHandle {
public:
    PooledHandle() : m_pool(nullptr), m_obj(nullptr) {}

    PooledHandle(ObjectPool<T>& pool, T* obj)
        : m_pool(&pool), m_obj(obj) {}

    ~PooledHandle() {
        if (m_obj && m_pool) {
            m_pool->release(m_obj);
        }
    }

    // Non-copyable
    PooledHandle(const PooledHandle&) = delete;
    PooledHandle& operator=(const PooledHandle&) = delete;

    // Movable
    PooledHandle(PooledHandle&& other) noexcept
        : m_pool(other.m_pool), m_obj(other.m_obj)
    {
        other.m_pool = nullptr;
        other.m_obj = nullptr;
    }

    PooledHandle& operator=(PooledHandle&& other) noexcept {
        if (this != &other) {
            if (m_obj && m_pool) {
                m_pool->release(m_obj);
            }
            m_pool = other.m_pool;
            m_obj = other.m_obj;
            other.m_pool = nullptr;
            other.m_obj = nullptr;
        }
        return *this;
    }

    // Access
    T* get() { return m_obj; }
    const T* get() const { return m_obj; }
    T* operator->() { return m_obj; }
    const T* operator->() const { return m_obj; }
    T& operator*() { return *m_obj; }
    const T& operator*() const { return *m_obj; }

    // Boolean conversion
    explicit operator bool() const { return m_obj != nullptr; }

    // Release ownership without returning to pool
    T* release() {
        T* tmp = m_obj;
        m_obj = nullptr;
        return tmp;
    }

    // Explicitly return to pool
    void reset() {
        if (m_obj && m_pool) {
            m_pool->release(m_obj);
            m_obj = nullptr;
        }
    }

private:
    ObjectPool<T>* m_pool;
    T* m_obj;
};

// ============================================================================
// Helper: Create a PooledHandle from pool acquisition
// ============================================================================

template<typename T>
PooledHandle<T> make_pooled(ObjectPool<T>& pool) {
    return PooledHandle<T>(pool, pool.acquire());
}

template<typename T, typename... Args>
PooledHandle<T> make_pooled(ObjectPool<T>& pool, Args&&... args) {
    return PooledHandle<T>(pool, pool.acquire(std::forward<Args>(args)...));
}

} // namespace engine::core
