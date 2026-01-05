#pragma once

#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/entity_pool.hpp>
#include <engine/core/math.hpp>
#include <engine/core/game_events.hpp>
#include <string>
#include <vector>
#include <functional>

namespace engine::scene {

// ============================================================================
// SpawnPointConfig - Configuration for spawn points
// ============================================================================

struct SpawnPointConfig {
    std::string spawn_id;               // Unique identifier for this spawn point
    std::string prefab_path;            // Prefab to spawn (if not using pool)
    std::string pool_name;              // Pool to use (if empty, uses prefab directly)
    core::Vec3 position_offset{0.0f};   // Offset from entity position
    core::Quat rotation_offset{1.0f, 0.0f, 0.0f, 0.0f};
    float spawn_radius = 0.0f;          // Random offset radius
    bool random_yaw = false;            // Randomize Y rotation
    bool enabled = true;
};

// ============================================================================
// SpawnPointComponent - ECS component for spawn point entities
// ============================================================================

struct SpawnPointComponent {
    SpawnPointConfig config;

    // Runtime state
    int spawn_count = 0;                // Total entities spawned
    int max_spawns = -1;                // -1 = unlimited
    float cooldown = 0.0f;              // Minimum time between spawns
    float current_cooldown = 0.0f;      // Time until next spawn allowed
    std::vector<Entity> spawned_entities;  // Entities spawned from this point

    // Check if can spawn
    bool can_spawn() const {
        if (!config.enabled) return false;
        if (current_cooldown > 0.0f) return false;
        if (max_spawns >= 0 && spawn_count >= max_spawns) return false;
        return true;
    }
};

// ============================================================================
// SpawnWave - Configuration for a wave of spawns
// ============================================================================

struct SpawnWaveEntry {
    std::string prefab_path;            // Prefab to spawn
    std::string pool_name;              // Or use pool (if not empty)
    int count = 1;                      // Number to spawn
};

struct SpawnWave {
    std::string wave_id;                // Identifier for this wave
    std::vector<SpawnWaveEntry> entries;
    float delay_before = 0.0f;          // Delay before wave starts
    float spawn_interval = 0.0f;        // Time between individual spawns
    bool wait_for_clear = false;        // Wait for previous wave to die
};

// ============================================================================
// WaveSpawnerComponent - ECS component for wave-based spawning
// ============================================================================

struct WaveSpawnerComponent {
    std::vector<SpawnWave> waves;

    // Runtime state
    int current_wave = -1;              // -1 = not started
    int current_entry = 0;              // Index in current wave entries
    int spawns_remaining = 0;           // Spawns remaining in current entry
    float wave_delay_timer = 0.0f;
    float spawn_interval_timer = 0.0f;
    bool active = false;
    bool loop_waves = false;
    bool all_waves_complete = false;
    std::vector<Entity> active_entities;

    // Query helpers
    bool is_wave_in_progress() const { return current_wave >= 0 && !all_waves_complete; }
    int active_entity_count() const { return static_cast<int>(active_entities.size()); }
};

// ============================================================================
// Spawn Events
// ============================================================================

struct EntitySpawnedEvent {
    Entity entity;
    Entity spawn_point;                 // NullEntity if direct spawn
    std::string prefab_path;
    std::string spawn_id;
    std::string pool_name;
};

struct WaveStartedEvent {
    Entity spawner;
    int wave_index;
    std::string wave_id;
};

struct WaveCompletedEvent {
    Entity spawner;
    int wave_index;
    std::string wave_id;
};

struct AllWavesCompletedEvent {
    Entity spawner;
};

// ============================================================================
// SpawnManager - Manages entity spawning
// ============================================================================

class SpawnManager {
public:
    // Singleton access
    static SpawnManager& instance();

    // Delete copy/move
    SpawnManager(const SpawnManager&) = delete;
    SpawnManager& operator=(const SpawnManager&) = delete;
    SpawnManager(SpawnManager&&) = delete;
    SpawnManager& operator=(SpawnManager&&) = delete;

    // ========================================================================
    // Direct Spawning
    // ========================================================================

    // Spawn entity from prefab
    Entity spawn(World& world, const std::string& prefab_path, const core::Vec3& position);
    Entity spawn(World& world, const std::string& prefab_path,
                 const core::Vec3& position, const core::Quat& rotation);

    // Spawn from pool
    Entity spawn_from_pool(World& world, const std::string& pool_name, const core::Vec3& position);
    Entity spawn_from_pool(World& world, const std::string& pool_name,
                           const core::Vec3& position, const core::Quat& rotation);

    // ========================================================================
    // Spawn Point Operations
    // ========================================================================

    // Spawn at a spawn point
    Entity spawn_at_point(World& world, Entity spawn_point);

    // Spawn multiple at a spawn point
    std::vector<Entity> spawn_at_point(World& world, Entity spawn_point, int count);

    // Enable/disable spawn point
    void enable_spawn_point(World& world, Entity spawn_point, bool enabled);

    // Reset spawn point (clear count and cooldown)
    void reset_spawn_point(World& world, Entity spawn_point);

    // Get entities spawned from a spawn point
    const std::vector<Entity>& get_spawned_entities(World& world, Entity spawn_point) const;

    // ========================================================================
    // Wave Control
    // ========================================================================

    // Start wave spawning
    void start_waves(World& world, Entity spawner);

    // Stop wave spawning (in progress spawns may continue)
    void stop_waves(World& world, Entity spawner);

    // Pause/resume wave spawning
    void pause_waves(World& world, Entity spawner);
    void resume_waves(World& world, Entity spawner);

    // Skip to next wave
    void skip_wave(World& world, Entity spawner);

    // Reset wave spawner
    void reset_waves(World& world, Entity spawner);

    // ========================================================================
    // Wave Queries
    // ========================================================================

    int get_current_wave(World& world, Entity spawner) const;
    int get_wave_count(World& world, Entity spawner) const;
    int get_active_spawn_count(World& world, Entity spawner) const;
    bool are_all_waves_complete(World& world, Entity spawner) const;
    bool is_wave_in_progress(World& world, Entity spawner) const;

    // ========================================================================
    // Cleanup
    // ========================================================================

    // Despawn entity (releases to pool if pooled, destroys otherwise)
    void despawn(World& world, Entity entity);

    // Despawn all entities from a spawn point
    void despawn_all_from_point(World& world, Entity spawn_point);

    // Despawn all entities from a wave spawner
    void despawn_all_from_waves(World& world, Entity spawner);

    // ========================================================================
    // Update
    // ========================================================================

    // Update spawn system - call once per frame in Update phase
    void update(World& world, float dt);

    // ========================================================================
    // Callbacks
    // ========================================================================

    using SpawnCallback = std::function<void(World&, Entity)>;

    // Called on every spawn
    void set_on_spawn(SpawnCallback callback) { m_on_spawn = std::move(callback); }

    // Called on every despawn
    void set_on_despawn(SpawnCallback callback) { m_on_despawn = std::move(callback); }

private:
    SpawnManager() = default;
    ~SpawnManager() = default;

    Entity do_spawn(World& world, const std::string& prefab_path,
                    const core::Vec3& position, const core::Quat& rotation,
                    const std::string& pool_name, Entity spawn_point,
                    const std::string& spawn_id);

    void update_spawn_points(World& world, float dt);
    void update_wave_spawners(World& world, float dt);
    void cleanup_dead_entities(World& world);

    core::Vec3 apply_random_offset(const core::Vec3& position, float radius);
    core::Quat apply_random_yaw(const core::Quat& rotation);

    SpawnCallback m_on_spawn;
    SpawnCallback m_on_despawn;
    static std::vector<Entity> s_empty_entities;
};

// ============================================================================
// ECS System Function
// ============================================================================

void spawn_system(World& world, double dt);

// ============================================================================
// Global Access
// ============================================================================

inline SpawnManager& spawns() { return SpawnManager::instance(); }

} // namespace engine::scene
