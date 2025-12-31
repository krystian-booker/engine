#pragma once

#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>
#include <engine/scene/components.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <fstream>
#include <sstream>

namespace engine::scene {

using namespace engine::core;

// Forward declarations
class World;

// Serialized component data (intermediate representation)
struct SerializedComponent {
    std::string type_name;
    std::string json_data;
};

// Serialized entity data
struct SerializedEntity {
    uint64_t uuid = 0;
    std::string name;
    bool enabled = true;
    uint64_t parent_uuid = 0;  // 0 = no parent
    std::vector<SerializedComponent> components;
};

// Serialized scene data
struct SerializedScene {
    std::string name;
    std::string version = "1.0";
    std::vector<SerializedEntity> entities;
    std::unordered_map<std::string, std::string> metadata;
};

// Asset reference for serialization (maps handles to asset paths)
struct AssetReference {
    std::string path;
    std::string type;  // "mesh", "texture", "material", etc.
};

// Scene serialization configuration
struct SerializerConfig {
    bool pretty_print = true;
    int indent_size = 2;
    bool include_default_values = false;
    bool include_world_transforms = false;  // Usually computed, not serialized
    bool serialize_disabled_entities = true;

    // Asset path resolution
    std::string asset_root_path;  // Base path for relative asset references
};

// Callback types for custom component serialization
using ComponentSerializer = std::function<std::string(const void* component)>;
using ComponentDeserializer = std::function<void(void* component, const std::string& json)>;

// Scene serializer/deserializer
class SceneSerializer {
public:
    SceneSerializer() = default;
    explicit SceneSerializer(const SerializerConfig& config);

    // Configuration
    void set_config(const SerializerConfig& config) { m_config = config; }
    const SerializerConfig& get_config() const { return m_config; }

    // Serialize scene to JSON string
    std::string serialize(World& world) const;

    // Serialize scene to file
    bool serialize_to_file(World& world, const std::string& path) const;

    // Deserialize scene from JSON string
    bool deserialize(World& world, const std::string& json);

    // Deserialize scene from file
    bool deserialize_from_file(World& world, const std::string& path);

    // Serialize individual entity (for prefabs)
    std::string serialize_entity(World& world, Entity entity, bool include_children = true) const;

    // Deserialize entity (for prefabs/instantiation)
    Entity deserialize_entity(World& world, const std::string& json, Entity parent = NullEntity);

    // Register custom component serializer
    template<typename T>
    void register_component(const std::string& type_name,
                           ComponentSerializer serializer,
                           ComponentDeserializer deserializer) {
        m_component_serializers[type_name] = std::move(serializer);
        m_component_deserializers[type_name] = std::move(deserializer);
        // Store type ID for runtime lookup
        m_type_names[typeid(T).hash_code()] = type_name;
    }

    // Asset reference handling
    void set_asset_resolver(std::function<AssetReference(uint32_t)> resolver) {
        m_asset_resolver = std::move(resolver);
    }
    void set_asset_loader(std::function<uint32_t(const AssetReference&)> loader) {
        m_asset_loader = std::move(loader);
    }

    // UUID generation
    static uint64_t generate_uuid();

private:
    // Serialization helpers
    SerializedEntity serialize_entity_internal(World& world, Entity entity) const;
    std::string entity_to_json(const SerializedEntity& entity) const;
    std::string scene_to_json(const SerializedScene& scene) const;

    // Deserialization helpers
    SerializedScene parse_scene_json(const std::string& json);
    SerializedEntity parse_entity_json(const std::string& json);
    Entity create_entity_from_serialized(World& world, const SerializedEntity& data, Entity parent);

    // JSON helpers
    std::string to_json(const Vec3& v) const;
    std::string to_json(const Vec4& v) const;
    std::string to_json(const Quat& q) const;
    std::string to_json(const Mat4& m) const;

    Vec3 parse_vec3(const std::string& json) const;
    Vec4 parse_vec4(const std::string& json) const;
    Quat parse_quat(const std::string& json) const;
    Mat4 parse_mat4(const std::string& json) const;

    // Built-in component serialization
    std::string serialize_transform(const LocalTransform& transform) const;
    std::string serialize_mesh_renderer(const MeshRenderer& renderer) const;
    std::string serialize_camera(const Camera& camera) const;
    std::string serialize_light(const Light& light) const;
    std::string serialize_particle_emitter(const ParticleEmitter& emitter) const;

    void deserialize_transform(LocalTransform& transform, const std::string& json) const;
    void deserialize_mesh_renderer(MeshRenderer& renderer, const std::string& json) const;
    void deserialize_camera(Camera& camera, const std::string& json) const;
    void deserialize_light(Light& light, const std::string& json) const;
    void deserialize_particle_emitter(ParticleEmitter& emitter, const std::string& json) const;

    SerializerConfig m_config;
    std::unordered_map<std::string, ComponentSerializer> m_component_serializers;
    std::unordered_map<std::string, ComponentDeserializer> m_component_deserializers;
    std::unordered_map<size_t, std::string> m_type_names;  // type_id hash -> name

    std::function<AssetReference(uint32_t)> m_asset_resolver;
    std::function<uint32_t(const AssetReference&)> m_asset_loader;
};

// Prefab system - serialized entity templates
class Prefab {
public:
    Prefab() = default;
    explicit Prefab(const std::string& json_data) : m_data(json_data) {}

    // Load prefab from file
    static Prefab load(const std::string& path);

    // Save prefab to file
    bool save(const std::string& path) const;

    // Get raw data
    const std::string& data() const { return m_data; }

    // Instantiate prefab in world
    Entity instantiate(World& world, SceneSerializer& serializer, Entity parent = NullEntity) const;

    // Create prefab from existing entity
    static Prefab create_from_entity(World& world, SceneSerializer& serializer, Entity entity);

    bool valid() const { return !m_data.empty(); }

private:
    std::string m_data;
};

// Scene management helpers
namespace scene_utils {

// Deep clone an entity and its children
Entity clone_entity(World& world, Entity source, Entity new_parent = NullEntity);

// Delete entity and all children
void delete_entity_recursive(World& world, Entity entity);

// Find entity by name (first match)
Entity find_entity_by_name(World& world, const std::string& name);

// Find all entities by name
std::vector<Entity> find_entities_by_name(World& world, const std::string& name);

// Find entity by UUID
Entity find_entity_by_uuid(World& world, uint64_t uuid);

// Get entity path (parent names separated by /)
std::string get_entity_path(World& world, Entity entity);

// Find entity by path
Entity find_entity_by_path(World& world, const std::string& path);

// Count total entities in scene
size_t count_entities(World& world);

// Count entities with specific component
template<typename T>
size_t count_entities_with_component(World& world);

} // namespace scene_utils

} // namespace engine::scene
