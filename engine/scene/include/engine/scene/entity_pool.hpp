#pragma once

#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <cstdint>
#include <functional>
#include <mutex>

namespace engine::scene {

// ============================================================================
// PoolConfig - Configuration for entity pools
// ============================================================================

struct PoolConfig {
    std::string pool_name;              // Unique name for this pool
    std::string prefab_path;            // Prefab to instantiate for pool entities
    uint32_t initial_size = 10;         // Pre-spawn count on creation
    uint32_t max_size = 100;            // Hard limit (0 = unlimited)
    uint32_t growth_size = 5;           // Expand by this many when empty
    float recycle_delay = 0.0f;         // Delay before entity becomes available again
    bool auto_expand = true;            // Grow when pool is exhausted
    bool warm_on_init = true;           // Pre-spawn on initialization
};

// ============================================================================
// PoolStats - Runtime statistics for a pool
// ============================================================================

struct PoolStats {
    uint32_t total_created = 0;         // Total entities ever created
    uint32_t currently_active = 0;      // Currently in use
    uint32_t currently_pooled = 0;      // Available in pool
    uint32_t currently_recycling = 0;   // Waiting for recycle delay
    uint32_t peak_active = 0;           // Maximum simultaneous active
    uint32_t acquire_count = 0;         // Times acquire() was called
    uint32_t release_count = 0;         // Times release() was called
    uint32_t expand_count = 0;          // Times pool was expanded
    uint32_t exhausted_count = 0;       // Times pool was exhausted (couldn't acquire)
};

// ============================================================================
// PooledEntity - Component marking entities as pooled
// ============================================================================

struct PooledEntity {
    std::string pool_name;
    bool active = false;
    float recycle_time = 0.0f;          // When entity was released (for recycle delay)
    uint64_t acquire_id = 0;            // Unique ID for this acquisition (for validation)
};

// ============================================================================
// EntityPool - Manages a pool of reusable entities
// ============================================================================

class EntityPool {
public:
    using ResetCallback = std::function<void(World&, Entity)>;

    EntityPool(World& world, const PoolConfig& config);
    ~EntityPool();

    // Non-copyable
    EntityPool(const EntityPool&) = delete;
    EntityPool& operator=(const EntityPool&) = delete;

    // ========================================================================
    // Acquire/Release
    // ========================================================================

    // Acquire an entity from the pool
    // Returns NullEntity if pool is exhausted and can't expand
    Entity acquire();

    // Acquire at a specific position/rotation
    Entity acquire(const core::Vec3& position,
                   const core::Quat& rotation = core::Quat{1.0f, 0.0f, 0.0f, 0.0f});

    // Release entity back to pool (respects recycle_delay)
    void release(Entity entity);

    // Immediate release (no delay)
    void release_immediate(Entity entity);

    // Check if entity belongs to this pool
    bool owns(Entity entity) const;

    // ========================================================================
    // Pool Management
    // ========================================================================

    // Pre-warm the pool with additional entities
    void warm(uint32_t count);

    // Clear all pooled entities (active entities are not affected)
    void clear_pooled();

    // Clear everything (destroys all entities, active and pooled)
    void clear_all();

    // Update pool (processes recycle delays)
    void update(float dt);

    // ========================================================================
    // Configuration
    // ========================================================================

    const PoolConfig& get_config() const { return m_config; }
    void set_max_size(uint32_t max_size) { m_config.max_size = max_size; }
    void set_recycle_delay(float delay) { m_config.recycle_delay = delay; }

    // ========================================================================
    // Callbacks
    // ========================================================================

    // Called when entity is acquired (use to reset state)
    void set_on_acquire(ResetCallback callback) { m_on_acquire = std::move(callback); }

    // Called when entity is released (use to cleanup)
    void set_on_release(ResetCallback callback) { m_on_release = std::move(callback); }

    // ========================================================================
    // Statistics
    // ========================================================================

    const PoolStats& get_stats() const { return m_stats; }

    uint32_t available_count() const {
        return static_cast<uint32_t>(m_available.size());
    }

    uint32_t active_count() const {
        return static_cast<uint32_t>(m_active.size());
    }

    uint32_t total_count() const {
        return static_cast<uint32_t>(m_available.size() + m_recycling.size() + m_active.size());
    }

private:
    Entity create_pooled_entity();
    void deactivate_entity(Entity entity);
    void activate_entity(Entity entity);
    bool can_expand() const;
    void expand(uint32_t count);

    World& m_world;
    PoolConfig m_config;
    PoolStats m_stats;

    std::vector<Entity> m_available;
    std::vector<std::pair<Entity, float>> m_recycling;  // Entity + time remaining
    std::unordered_set<Entity> m_active;

    ResetCallback m_on_acquire;
    ResetCallback m_on_release;

    uint64_t m_next_acquire_id = 1;
};

// ============================================================================
// PoolManager - Manages multiple entity pools
// ============================================================================

class PoolManager {
public:
    // Singleton access
    static PoolManager& instance();

    // Delete copy/move
    PoolManager(const PoolManager&) = delete;
    PoolManager& operator=(const PoolManager&) = delete;
    PoolManager(PoolManager&&) = delete;
    PoolManager& operator=(PoolManager&&) = delete;

    // ========================================================================
    // Pool Management
    // ========================================================================

    // Create a new pool
    EntityPool& create_pool(World& world, const PoolConfig& config);

    // Get pool by name
    EntityPool* get_pool(const std::string& name);
    const EntityPool* get_pool(const std::string& name) const;

    // Check if pool exists
    bool has_pool(const std::string& name) const;

    // Destroy a pool (all entities in pool are destroyed)
    void destroy_pool(const std::string& name);

    // Clear all pools
    void clear_all();

    // ========================================================================
    // Convenience Methods
    // ========================================================================

    // Acquire from named pool
    Entity acquire(const std::string& pool_name);
    Entity acquire(const std::string& pool_name, const core::Vec3& position);
    Entity acquire(const std::string& pool_name, const core::Vec3& position, const core::Quat& rotation);

    // Release entity (auto-detects pool from PooledEntity component)
    void release(World& world, Entity entity);

    // Release with immediate return to pool
    void release_immediate(World& world, Entity entity);

    // ========================================================================
    // Update
    // ========================================================================

    // Update all pools (processes recycle delays)
    void update(float dt);

    // ========================================================================
    // Statistics
    // ========================================================================

    struct GlobalStats {
        size_t pool_count = 0;
        size_t total_entities = 0;
        size_t total_active = 0;
        size_t total_pooled = 0;
    };

    GlobalStats get_global_stats() const;

    // Get all pool names
    std::vector<std::string> get_pool_names() const;

    // Get stats for specific pool
    const PoolStats* get_pool_stats(const std::string& name) const;

private:
    PoolManager() = default;
    ~PoolManager() = default;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::unique_ptr<EntityPool>> m_pools;
};

// ============================================================================
// Global Access
// ============================================================================

inline PoolManager& pools() { return PoolManager::instance(); }

} // namespace engine::scene
