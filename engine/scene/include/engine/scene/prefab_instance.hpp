#pragma once

#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/scene_serializer.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace engine::scene {

// Tracks an individual property override from the original prefab
struct PropertyOverride {
    std::string component_type;  // e.g., "LocalTransform"
    std::string property_path;   // e.g., "position.x" or "color"
    std::string json_value;      // The overridden value as JSON

    bool operator==(const PropertyOverride& other) const {
        return component_type == other.component_type &&
               property_path == other.property_path;
    }
};

// Component for entities instantiated from prefabs
struct PrefabInstance {
    std::string prefab_path;           // Path to source prefab asset
    uint64_t prefab_entity_uuid = 0;   // UUID of entity within prefab (for nested prefabs)
    std::vector<PropertyOverride> overrides;

    // Is this the root of a prefab instance?
    bool is_root = false;

    PrefabInstance() = default;
    explicit PrefabInstance(const std::string& path) : prefab_path(path), is_root(true) {}

    // Check if a property is overridden
    bool is_overridden(const std::string& component, const std::string& property) const;

    // Get override value (returns empty string if not overridden)
    std::string get_override(const std::string& component, const std::string& property) const;

    // Add or update an override
    void set_override(const std::string& component, const std::string& property, const std::string& value);

    // Remove an override
    void remove_override(const std::string& component, const std::string& property);

    // Clear all overrides
    void clear_overrides();

    // Get number of overrides
    size_t override_count() const { return overrides.size(); }
};

// Cached prefab data
struct PrefabData {
    std::string path;
    std::string json_data;
    uint64_t root_uuid = 0;
    std::vector<uint64_t> entity_uuids;  // All entity UUIDs in this prefab

    bool valid() const { return !json_data.empty(); }
};

// Manages prefab assets and instances
class PrefabManager {
public:
    static PrefabManager& instance();

    // Load a prefab asset (cached)
    const PrefabData* load_prefab(const std::string& path);

    // Unload a prefab from cache
    void unload_prefab(const std::string& path);

    // Clear all cached prefabs
    void clear_cache();

    // Check if prefab is loaded
    bool is_loaded(const std::string& path) const;

    // Instantiate a prefab
    // Returns the root entity of the instantiated prefab
    Entity instantiate(World& world, const std::string& prefab_path, Entity parent = NullEntity);

    // Instantiate at a specific position/rotation
    Entity instantiate(World& world, const std::string& prefab_path,
                       const core::Vec3& position,
                       const core::Quat& rotation = core::Quat{1, 0, 0, 0},
                       Entity parent = NullEntity);

    // Create a prefab from an existing entity hierarchy
    bool create_prefab(World& world, Entity root, const std::string& save_path);

    // Apply changes from prefab to all instances
    void update_instances(World& world, const std::string& prefab_path);

    // Revert an instance to match the prefab (clears overrides)
    void revert_instance(World& world, Entity instance_root);

    // Break prefab connection (entity becomes independent)
    void unpack_prefab(World& world, Entity instance_root, bool recursive = false);

    // Check if entity is part of a prefab instance
    bool is_prefab_instance(World& world, Entity entity) const;

    // Get the root of the prefab instance that contains this entity
    Entity get_prefab_root(World& world, Entity entity) const;

    // Get all instances of a prefab in the world
    std::vector<Entity> get_instances(World& world, const std::string& prefab_path) const;

    // Scene serializer access
    void set_serializer(SceneSerializer* serializer) { m_serializer = serializer; }
    SceneSerializer* get_serializer() { return m_serializer; }

private:
    PrefabManager() = default;
    ~PrefabManager() = default;
    PrefabManager(const PrefabManager&) = delete;
    PrefabManager& operator=(const PrefabManager&) = delete;

    // Apply overrides after instantiation
    void apply_overrides(World& world, Entity entity, const PrefabInstance& instance);

    // Collect entity hierarchy for saving
    std::vector<Entity> collect_hierarchy(World& world, Entity root);

    std::unordered_map<std::string, PrefabData> m_cache;
    SceneSerializer* m_serializer = nullptr;
};

// Prefab utilities
namespace prefab_utils {

// Check if two prefab paths refer to the same prefab (handles relative paths)
bool same_prefab(const std::string& path1, const std::string& path2);

// Get just the prefab name from a path
std::string get_prefab_name(const std::string& path);

// Validate prefab data structure
bool validate_prefab(const std::string& json_data);

// Merge overrides from source into target
void merge_overrides(PrefabInstance& target, const PrefabInstance& source);

// Diff two instances to find differences
std::vector<PropertyOverride> diff_instances(World& world, Entity instance1, Entity instance2);

} // namespace prefab_utils

} // namespace engine::scene
