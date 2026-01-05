#include <engine/scene/spawn_system.hpp>
#include <engine/scene/prefab_instance.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/components.hpp>
#include <engine/core/game_events.hpp>
#include <random>
#include <cmath>

namespace engine::scene {

// Static empty vector for invalid queries
std::vector<Entity> SpawnManager::s_empty_entities;

// Random number generation
static std::mt19937& get_rng() {
    static std::random_device rd;
    static std::mt19937 rng(rd());
    return rng;
}

// ============================================================================
// SpawnManager Implementation
// ============================================================================

SpawnManager& SpawnManager::instance() {
    static SpawnManager s_instance;
    return s_instance;
}

core::Vec3 SpawnManager::apply_random_offset(const core::Vec3& position, float radius) {
    if (radius <= 0.0f) {
        return position;
    }

    auto& rng = get_rng();
    std::uniform_real_distribution<float> dist_angle(0.0f, 2.0f * 3.14159265f);
    std::uniform_real_distribution<float> dist_radius(0.0f, radius);

    float angle = dist_angle(rng);
    float r = dist_radius(rng);

    return position + core::Vec3{
        std::cos(angle) * r,
        0.0f,
        std::sin(angle) * r
    };
}

core::Quat SpawnManager::apply_random_yaw(const core::Quat& rotation) {
    auto& rng = get_rng();
    std::uniform_real_distribution<float> dist(0.0f, 2.0f * 3.14159265f);

    float angle = dist(rng);
    core::Quat yaw_rotation = core::Quat{std::cos(angle * 0.5f), 0.0f, std::sin(angle * 0.5f), 0.0f};

    return yaw_rotation * rotation;
}

Entity SpawnManager::do_spawn(World& world, const std::string& prefab_path,
                               const core::Vec3& position, const core::Quat& rotation,
                               const std::string& pool_name, Entity spawn_point,
                               const std::string& spawn_id) {
    Entity entity = NullEntity;

    if (!pool_name.empty()) {
        // Spawn from pool
        entity = pools().acquire(pool_name, position, rotation);
    } else if (!prefab_path.empty()) {
        // Spawn from prefab
        entity = PrefabManager::instance().instantiate(world, prefab_path, position, rotation);
    }

    if (entity == NullEntity) {
        return NullEntity;
    }

    // Emit spawn event
    EntitySpawnedEvent event;
    event.entity = entity;
    event.spawn_point = spawn_point;
    event.prefab_path = prefab_path;
    event.spawn_id = spawn_id;
    event.pool_name = pool_name;
    core::game_events().emit(event);

    // Call spawn callback
    if (m_on_spawn) {
        m_on_spawn(world, entity);
    }

    return entity;
}

// ============================================================================
// Direct Spawning
// ============================================================================

Entity SpawnManager::spawn(World& world, const std::string& prefab_path,
                           const core::Vec3& position) {
    return spawn(world, prefab_path, position, core::Quat{1.0f, 0.0f, 0.0f, 0.0f});
}

Entity SpawnManager::spawn(World& world, const std::string& prefab_path,
                           const core::Vec3& position, const core::Quat& rotation) {
    return do_spawn(world, prefab_path, position, rotation, "", NullEntity, "");
}

Entity SpawnManager::spawn_from_pool(World& world, const std::string& pool_name,
                                     const core::Vec3& position) {
    return spawn_from_pool(world, pool_name, position, core::Quat{1.0f, 0.0f, 0.0f, 0.0f});
}

Entity SpawnManager::spawn_from_pool(World& world, const std::string& pool_name,
                                     const core::Vec3& position, const core::Quat& rotation) {
    return do_spawn(world, "", position, rotation, pool_name, NullEntity, "");
}

// ============================================================================
// Spawn Point Operations
// ============================================================================

Entity SpawnManager::spawn_at_point(World& world, Entity spawn_point) {
    auto* comp = world.try_get<SpawnPointComponent>(spawn_point);
    if (!comp || !comp->can_spawn()) {
        return NullEntity;
    }

    // Get spawn point position
    core::Vec3 position{0.0f};
    core::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};

    if (auto* transform = world.try_get<LocalTransform>(spawn_point)) {
        position = transform->position;
        rotation = transform->rotation;
    }

    // Apply offsets
    position += comp->config.position_offset;
    rotation = comp->config.rotation_offset * rotation;

    // Apply random offset
    position = apply_random_offset(position, comp->config.spawn_radius);

    // Apply random yaw
    if (comp->config.random_yaw) {
        rotation = apply_random_yaw(rotation);
    }

    // Spawn entity
    Entity entity = do_spawn(world,
                             comp->config.prefab_path,
                             position,
                             rotation,
                             comp->config.pool_name,
                             spawn_point,
                             comp->config.spawn_id);

    if (entity != NullEntity) {
        comp->spawn_count++;
        comp->current_cooldown = comp->cooldown;
        comp->spawned_entities.push_back(entity);
    }

    return entity;
}

std::vector<Entity> SpawnManager::spawn_at_point(World& world, Entity spawn_point, int count) {
    std::vector<Entity> entities;
    entities.reserve(count);

    for (int i = 0; i < count; ++i) {
        Entity entity = spawn_at_point(world, spawn_point);
        if (entity != NullEntity) {
            entities.push_back(entity);
        }
    }

    return entities;
}

void SpawnManager::enable_spawn_point(World& world, Entity spawn_point, bool enabled) {
    if (auto* comp = world.try_get<SpawnPointComponent>(spawn_point)) {
        comp->config.enabled = enabled;
    }
}

void SpawnManager::reset_spawn_point(World& world, Entity spawn_point) {
    if (auto* comp = world.try_get<SpawnPointComponent>(spawn_point)) {
        comp->spawn_count = 0;
        comp->current_cooldown = 0.0f;
    }
}

const std::vector<Entity>& SpawnManager::get_spawned_entities(World& world, Entity spawn_point) const {
    const auto* comp = world.try_get<SpawnPointComponent>(spawn_point);
    return comp ? comp->spawned_entities : s_empty_entities;
}

// ============================================================================
// Wave Control
// ============================================================================

void SpawnManager::start_waves(World& world, Entity spawner) {
    auto* comp = world.try_get<WaveSpawnerComponent>(spawner);
    if (!comp || comp->waves.empty()) {
        return;
    }

    comp->active = true;
    comp->current_wave = 0;
    comp->current_entry = 0;
    comp->spawns_remaining = 0;
    comp->all_waves_complete = false;

    // Start with delay
    if (!comp->waves.empty()) {
        comp->wave_delay_timer = comp->waves[0].delay_before;
    }

    // Emit wave started event
    WaveStartedEvent event;
    event.spawner = spawner;
    event.wave_index = 0;
    event.wave_id = comp->waves[0].wave_id;
    core::game_events().emit(event);
}

void SpawnManager::stop_waves(World& world, Entity spawner) {
    if (auto* comp = world.try_get<WaveSpawnerComponent>(spawner)) {
        comp->active = false;
    }
}

void SpawnManager::pause_waves(World& world, Entity spawner) {
    stop_waves(world, spawner);
}

void SpawnManager::resume_waves(World& world, Entity spawner) {
    if (auto* comp = world.try_get<WaveSpawnerComponent>(spawner)) {
        comp->active = true;
    }
}

void SpawnManager::skip_wave(World& world, Entity spawner) {
    auto* comp = world.try_get<WaveSpawnerComponent>(spawner);
    if (!comp || !comp->active || comp->current_wave < 0) {
        return;
    }

    // Emit wave completed event
    if (comp->current_wave < static_cast<int>(comp->waves.size())) {
        WaveCompletedEvent event;
        event.spawner = spawner;
        event.wave_index = comp->current_wave;
        event.wave_id = comp->waves[comp->current_wave].wave_id;
        core::game_events().emit(event);
    }

    // Move to next wave
    comp->current_wave++;
    comp->current_entry = 0;
    comp->spawns_remaining = 0;

    if (comp->current_wave >= static_cast<int>(comp->waves.size())) {
        if (comp->loop_waves) {
            comp->current_wave = 0;
        } else {
            comp->all_waves_complete = true;
            comp->active = false;

            AllWavesCompletedEvent complete_event;
            complete_event.spawner = spawner;
            core::game_events().emit(complete_event);
            return;
        }
    }

    // Start next wave
    comp->wave_delay_timer = comp->waves[comp->current_wave].delay_before;

    WaveStartedEvent start_event;
    start_event.spawner = spawner;
    start_event.wave_index = comp->current_wave;
    start_event.wave_id = comp->waves[comp->current_wave].wave_id;
    core::game_events().emit(start_event);
}

void SpawnManager::reset_waves(World& world, Entity spawner) {
    if (auto* comp = world.try_get<WaveSpawnerComponent>(spawner)) {
        comp->active = false;
        comp->current_wave = -1;
        comp->current_entry = 0;
        comp->spawns_remaining = 0;
        comp->wave_delay_timer = 0.0f;
        comp->spawn_interval_timer = 0.0f;
        comp->all_waves_complete = false;
    }
}

// ============================================================================
// Wave Queries
// ============================================================================

int SpawnManager::get_current_wave(World& world, Entity spawner) const {
    const auto* comp = world.try_get<WaveSpawnerComponent>(spawner);
    return comp ? comp->current_wave : -1;
}

int SpawnManager::get_wave_count(World& world, Entity spawner) const {
    const auto* comp = world.try_get<WaveSpawnerComponent>(spawner);
    return comp ? static_cast<int>(comp->waves.size()) : 0;
}

int SpawnManager::get_active_spawn_count(World& world, Entity spawner) const {
    const auto* comp = world.try_get<WaveSpawnerComponent>(spawner);
    return comp ? comp->active_entity_count() : 0;
}

bool SpawnManager::are_all_waves_complete(World& world, Entity spawner) const {
    const auto* comp = world.try_get<WaveSpawnerComponent>(spawner);
    return comp ? comp->all_waves_complete : false;
}

bool SpawnManager::is_wave_in_progress(World& world, Entity spawner) const {
    const auto* comp = world.try_get<WaveSpawnerComponent>(spawner);
    return comp ? comp->is_wave_in_progress() : false;
}

// ============================================================================
// Cleanup
// ============================================================================

void SpawnManager::despawn(World& world, Entity entity) {
    if (entity == NullEntity || !world.valid(entity)) {
        return;
    }

    // Call despawn callback
    if (m_on_despawn) {
        m_on_despawn(world, entity);
    }

    // Check if pooled
    if (auto* pooled = world.try_get<PooledEntity>(entity)) {
        pools().release(world, entity);
    } else {
        world.destroy(entity);
    }
}

void SpawnManager::despawn_all_from_point(World& world, Entity spawn_point) {
    auto* comp = world.try_get<SpawnPointComponent>(spawn_point);
    if (!comp) return;

    for (Entity entity : comp->spawned_entities) {
        if (world.valid(entity)) {
            despawn(world, entity);
        }
    }
    comp->spawned_entities.clear();
}

void SpawnManager::despawn_all_from_waves(World& world, Entity spawner) {
    auto* comp = world.try_get<WaveSpawnerComponent>(spawner);
    if (!comp) return;

    for (Entity entity : comp->active_entities) {
        if (world.valid(entity)) {
            despawn(world, entity);
        }
    }
    comp->active_entities.clear();
}

// ============================================================================
// Update
// ============================================================================

void SpawnManager::update(World& world, float dt) {
    update_spawn_points(world, dt);
    update_wave_spawners(world, dt);
    cleanup_dead_entities(world);
}

void SpawnManager::update_spawn_points(World& world, float dt) {
    auto view = world.view<SpawnPointComponent>();
    for (auto entity : view) {
        auto& comp = view.get<SpawnPointComponent>(entity);

        // Update cooldown
        if (comp.current_cooldown > 0.0f) {
            comp.current_cooldown -= dt;
            if (comp.current_cooldown < 0.0f) {
                comp.current_cooldown = 0.0f;
            }
        }
    }
}

void SpawnManager::update_wave_spawners(World& world, float dt) {
    auto view = world.view<WaveSpawnerComponent>();
    for (auto entity : view) {
        auto& comp = view.get<WaveSpawnerComponent>(entity);

        if (!comp.active || comp.current_wave < 0 || comp.all_waves_complete) {
            continue;
        }

        if (comp.current_wave >= static_cast<int>(comp.waves.size())) {
            continue;
        }

        auto& wave = comp.waves[comp.current_wave];

        // Wait for wave delay
        if (comp.wave_delay_timer > 0.0f) {
            comp.wave_delay_timer -= dt;
            continue;
        }

        // Wait for clear if required
        if (wave.wait_for_clear && !comp.active_entities.empty()) {
            continue;
        }

        // Get spawn position from entity
        core::Vec3 position{0.0f};
        core::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        if (auto* transform = world.try_get<LocalTransform>(entity)) {
            position = transform->position;
            rotation = transform->rotation;
        }

        // Process spawn interval
        if (comp.spawn_interval_timer > 0.0f) {
            comp.spawn_interval_timer -= dt;
            if (comp.spawn_interval_timer > 0.0f) {
                continue;
            }
        }

        // Initialize spawns remaining for current entry
        if (comp.spawns_remaining <= 0 && comp.current_entry < static_cast<int>(wave.entries.size())) {
            comp.spawns_remaining = wave.entries[comp.current_entry].count;
        }

        // Spawn entity
        if (comp.spawns_remaining > 0 && comp.current_entry < static_cast<int>(wave.entries.size())) {
            auto& entry = wave.entries[comp.current_entry];

            Entity spawned = do_spawn(world, entry.prefab_path, position, rotation,
                                      entry.pool_name, entity, wave.wave_id);

            if (spawned != NullEntity) {
                comp.active_entities.push_back(spawned);
                comp.spawns_remaining--;
                comp.spawn_interval_timer = wave.spawn_interval;
            }

            // Move to next entry if done
            if (comp.spawns_remaining <= 0) {
                comp.current_entry++;
            }
        }

        // Check if wave is complete
        if (comp.current_entry >= static_cast<int>(wave.entries.size()) && comp.spawns_remaining <= 0) {
            // Emit wave completed event
            WaveCompletedEvent event;
            event.spawner = entity;
            event.wave_index = comp.current_wave;
            event.wave_id = wave.wave_id;
            core::game_events().emit(event);

            // Move to next wave
            comp.current_wave++;
            comp.current_entry = 0;

            if (comp.current_wave >= static_cast<int>(comp.waves.size())) {
                if (comp.loop_waves) {
                    comp.current_wave = 0;
                } else {
                    comp.all_waves_complete = true;
                    comp.active = false;

                    AllWavesCompletedEvent complete_event;
                    complete_event.spawner = entity;
                    core::game_events().emit(complete_event);
                    continue;
                }
            }

            // Start next wave
            comp.wave_delay_timer = comp.waves[comp.current_wave].delay_before;

            WaveStartedEvent start_event;
            start_event.spawner = entity;
            start_event.wave_index = comp.current_wave;
            start_event.wave_id = comp.waves[comp.current_wave].wave_id;
            core::game_events().emit(start_event);
        }
    }
}

void SpawnManager::cleanup_dead_entities(World& world) {
    // Clean up spawn point entity lists
    auto spawn_view = world.view<SpawnPointComponent>();
    for (auto entity : spawn_view) {
        auto& comp = spawn_view.get<SpawnPointComponent>(entity);
        comp.spawned_entities.erase(
            std::remove_if(comp.spawned_entities.begin(), comp.spawned_entities.end(),
                [&world](Entity e) { return !world.valid(e); }),
            comp.spawned_entities.end()
        );
    }

    // Clean up wave spawner entity lists
    auto wave_view = world.view<WaveSpawnerComponent>();
    for (auto entity : wave_view) {
        auto& comp = wave_view.get<WaveSpawnerComponent>(entity);
        comp.active_entities.erase(
            std::remove_if(comp.active_entities.begin(), comp.active_entities.end(),
                [&world](Entity e) { return !world.valid(e); }),
            comp.active_entities.end()
        );
    }
}

// ============================================================================
// ECS System Function
// ============================================================================

void spawn_system(World& world, double dt) {
    SpawnManager::instance().update(world, static_cast<float>(dt));
}

} // namespace engine::scene
