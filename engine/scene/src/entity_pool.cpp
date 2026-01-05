#include <engine/scene/entity_pool.hpp>
#include <engine/scene/prefab_instance.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/components.hpp>
#include <algorithm>

namespace engine::scene {

// ============================================================================
// EntityPool Implementation
// ============================================================================

EntityPool::EntityPool(World& world, const PoolConfig& config)
    : m_world(world)
    , m_config(config) {
    if (config.warm_on_init && config.initial_size > 0) {
        warm(config.initial_size);
    }
}

EntityPool::~EntityPool() {
    clear_all();
}

Entity EntityPool::create_pooled_entity() {
    // Instantiate from prefab
    auto& prefab_mgr = PrefabManager::instance();
    Entity entity = prefab_mgr.instantiate(m_world, m_config.prefab_path);

    if (entity == NullEntity) {
        return NullEntity;
    }

    // Add pooled entity marker
    auto& pooled = m_world.emplace<PooledEntity>(entity);
    pooled.pool_name = m_config.pool_name;
    pooled.active = false;
    pooled.recycle_time = 0.0f;
    pooled.acquire_id = 0;

    m_stats.total_created++;

    return entity;
}

void EntityPool::deactivate_entity(Entity entity) {
    if (entity == NullEntity) return;

    // Disable entity
    if (auto* info = m_world.try_get<EntityInfo>(entity)) {
        info->enabled = false;
    }

    // Hide from rendering by moving far away or scaling to zero
    if (auto* transform = m_world.try_get<LocalTransform>(entity)) {
        transform->position = core::Vec3{-10000.0f, -10000.0f, -10000.0f};
    }

    // Update pooled component
    if (auto* pooled = m_world.try_get<PooledEntity>(entity)) {
        pooled->active = false;
    }
}

void EntityPool::activate_entity(Entity entity) {
    if (entity == NullEntity) return;

    // Enable entity
    if (auto* info = m_world.try_get<EntityInfo>(entity)) {
        info->enabled = true;
    }

    // Update pooled component
    if (auto* pooled = m_world.try_get<PooledEntity>(entity)) {
        pooled->active = true;
        pooled->acquire_id = m_next_acquire_id++;
    }
}

bool EntityPool::can_expand() const {
    if (!m_config.auto_expand) {
        return false;
    }

    if (m_config.max_size == 0) {
        return true;  // No limit
    }

    return total_count() < m_config.max_size;
}

void EntityPool::expand(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (m_config.max_size > 0 && total_count() >= m_config.max_size) {
            break;
        }

        Entity entity = create_pooled_entity();
        if (entity != NullEntity) {
            deactivate_entity(entity);
            m_available.push_back(entity);
        }
    }

    m_stats.expand_count++;
}

Entity EntityPool::acquire() {
    m_stats.acquire_count++;

    // First, check available pool
    while (!m_available.empty()) {
        Entity entity = m_available.back();
        m_available.pop_back();

        // Validate entity still exists
        if (!m_world.valid(entity)) {
            continue;
        }

        activate_entity(entity);
        m_active.insert(entity);

        m_stats.currently_active = static_cast<uint32_t>(m_active.size());
        m_stats.currently_pooled = static_cast<uint32_t>(m_available.size());
        m_stats.peak_active = std::max(m_stats.peak_active, m_stats.currently_active);

        if (m_on_acquire) {
            m_on_acquire(m_world, entity);
        }

        return entity;
    }

    // Pool is empty, try to expand
    if (can_expand()) {
        expand(m_config.growth_size);

        // Try again
        if (!m_available.empty()) {
            Entity entity = m_available.back();
            m_available.pop_back();

            activate_entity(entity);
            m_active.insert(entity);

            m_stats.currently_active = static_cast<uint32_t>(m_active.size());
            m_stats.currently_pooled = static_cast<uint32_t>(m_available.size());
            m_stats.peak_active = std::max(m_stats.peak_active, m_stats.currently_active);

            if (m_on_acquire) {
                m_on_acquire(m_world, entity);
            }

            return entity;
        }
    }

    // Pool exhausted
    m_stats.exhausted_count++;
    return NullEntity;
}

Entity EntityPool::acquire(const core::Vec3& position, const core::Quat& rotation) {
    Entity entity = acquire();

    if (entity != NullEntity) {
        if (auto* transform = m_world.try_get<LocalTransform>(entity)) {
            transform->position = position;
            transform->rotation = rotation;
        }
    }

    return entity;
}

void EntityPool::release(Entity entity) {
    if (entity == NullEntity) return;
    if (m_active.find(entity) == m_active.end()) return;

    m_stats.release_count++;

    // Call release callback
    if (m_on_release) {
        m_on_release(m_world, entity);
    }

    // Remove from active
    m_active.erase(entity);

    if (m_config.recycle_delay > 0.0f) {
        // Add to recycling queue
        deactivate_entity(entity);

        if (auto* pooled = m_world.try_get<PooledEntity>(entity)) {
            pooled->recycle_time = m_config.recycle_delay;
        }

        m_recycling.push_back({entity, m_config.recycle_delay});
    } else {
        // Return immediately
        deactivate_entity(entity);
        m_available.push_back(entity);
    }

    m_stats.currently_active = static_cast<uint32_t>(m_active.size());
    m_stats.currently_pooled = static_cast<uint32_t>(m_available.size());
    m_stats.currently_recycling = static_cast<uint32_t>(m_recycling.size());
}

void EntityPool::release_immediate(Entity entity) {
    if (entity == NullEntity) return;
    if (m_active.find(entity) == m_active.end()) return;

    m_stats.release_count++;

    // Call release callback
    if (m_on_release) {
        m_on_release(m_world, entity);
    }

    // Remove from active
    m_active.erase(entity);

    // Return immediately
    deactivate_entity(entity);
    m_available.push_back(entity);

    m_stats.currently_active = static_cast<uint32_t>(m_active.size());
    m_stats.currently_pooled = static_cast<uint32_t>(m_available.size());
}

bool EntityPool::owns(Entity entity) const {
    if (entity == NullEntity) return false;

    auto* pooled = m_world.try_get<PooledEntity>(entity);
    return pooled && pooled->pool_name == m_config.pool_name;
}

void EntityPool::warm(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (m_config.max_size > 0 && total_count() >= m_config.max_size) {
            break;
        }

        Entity entity = create_pooled_entity();
        if (entity != NullEntity) {
            deactivate_entity(entity);
            m_available.push_back(entity);
        }
    }

    m_stats.currently_pooled = static_cast<uint32_t>(m_available.size());
}

void EntityPool::clear_pooled() {
    for (Entity entity : m_available) {
        if (m_world.valid(entity)) {
            m_world.destroy(entity);
        }
    }
    m_available.clear();

    for (auto& [entity, time] : m_recycling) {
        if (m_world.valid(entity)) {
            m_world.destroy(entity);
        }
    }
    m_recycling.clear();

    m_stats.currently_pooled = 0;
    m_stats.currently_recycling = 0;
}

void EntityPool::clear_all() {
    clear_pooled();

    for (Entity entity : m_active) {
        if (m_world.valid(entity)) {
            m_world.destroy(entity);
        }
    }
    m_active.clear();

    m_stats.currently_active = 0;
}

void EntityPool::update(float dt) {
    // Process recycling queue
    for (auto it = m_recycling.begin(); it != m_recycling.end();) {
        it->second -= dt;

        if (it->second <= 0.0f) {
            // Ready to return to pool
            if (m_world.valid(it->first)) {
                m_available.push_back(it->first);
            }
            it = m_recycling.erase(it);
        } else {
            ++it;
        }
    }

    m_stats.currently_pooled = static_cast<uint32_t>(m_available.size());
    m_stats.currently_recycling = static_cast<uint32_t>(m_recycling.size());
}

// ============================================================================
// PoolManager Implementation
// ============================================================================

PoolManager& PoolManager::instance() {
    static PoolManager s_instance;
    return s_instance;
}

EntityPool& PoolManager::create_pool(World& world, const PoolConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto pool = std::make_unique<EntityPool>(world, config);
    auto* ptr = pool.get();
    m_pools[config.pool_name] = std::move(pool);

    return *ptr;
}

EntityPool* PoolManager::get_pool(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pools.find(name);
    return it != m_pools.end() ? it->second.get() : nullptr;
}

const EntityPool* PoolManager::get_pool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pools.find(name);
    return it != m_pools.end() ? it->second.get() : nullptr;
}

bool PoolManager::has_pool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pools.find(name) != m_pools.end();
}

void PoolManager::destroy_pool(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pools.erase(name);
}

void PoolManager::clear_all() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pools.clear();
}

Entity PoolManager::acquire(const std::string& pool_name) {
    if (auto* pool = get_pool(pool_name)) {
        return pool->acquire();
    }
    return NullEntity;
}

Entity PoolManager::acquire(const std::string& pool_name, const core::Vec3& position) {
    if (auto* pool = get_pool(pool_name)) {
        return pool->acquire(position);
    }
    return NullEntity;
}

Entity PoolManager::acquire(const std::string& pool_name, const core::Vec3& position, const core::Quat& rotation) {
    if (auto* pool = get_pool(pool_name)) {
        return pool->acquire(position, rotation);
    }
    return NullEntity;
}

void PoolManager::release(World& world, Entity entity) {
    if (entity == NullEntity) return;

    auto* pooled = world.try_get<PooledEntity>(entity);
    if (!pooled) return;

    if (auto* pool = get_pool(pooled->pool_name)) {
        pool->release(entity);
    }
}

void PoolManager::release_immediate(World& world, Entity entity) {
    if (entity == NullEntity) return;

    auto* pooled = world.try_get<PooledEntity>(entity);
    if (!pooled) return;

    if (auto* pool = get_pool(pooled->pool_name)) {
        pool->release_immediate(entity);
    }
}

void PoolManager::update(float dt) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [name, pool] : m_pools) {
        pool->update(dt);
    }
}

PoolManager::GlobalStats PoolManager::get_global_stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    GlobalStats stats;
    stats.pool_count = m_pools.size();

    for (const auto& [name, pool] : m_pools) {
        stats.total_entities += pool->total_count();
        stats.total_active += pool->active_count();
        stats.total_pooled += pool->available_count();
    }

    return stats;
}

std::vector<std::string> PoolManager::get_pool_names() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> names;
    names.reserve(m_pools.size());
    for (const auto& [name, pool] : m_pools) {
        names.push_back(name);
    }

    return names;
}

const PoolStats* PoolManager::get_pool_stats(const std::string& name) const {
    if (const auto* pool = get_pool(name)) {
        return &pool->get_stats();
    }
    return nullptr;
}

} // namespace engine::scene
