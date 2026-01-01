#include <engine/scene/prefab_instance.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/log.hpp>
#include <engine/core/filesystem.hpp>
#include <algorithm>

namespace engine::scene {

// ============================================================================
// PrefabInstance
// ============================================================================

bool PrefabInstance::is_overridden(const std::string& component, const std::string& property) const {
    return std::any_of(overrides.begin(), overrides.end(),
        [&](const PropertyOverride& o) {
            return o.component_type == component && o.property_path == property;
        });
}

std::string PrefabInstance::get_override(const std::string& component, const std::string& property) const {
    auto it = std::find_if(overrides.begin(), overrides.end(),
        [&](const PropertyOverride& o) {
            return o.component_type == component && o.property_path == property;
        });

    if (it != overrides.end()) {
        return it->json_value;
    }
    return "";
}

void PrefabInstance::set_override(const std::string& component, const std::string& property, const std::string& value) {
    auto it = std::find_if(overrides.begin(), overrides.end(),
        [&](const PropertyOverride& o) {
            return o.component_type == component && o.property_path == property;
        });

    if (it != overrides.end()) {
        it->json_value = value;
    } else {
        overrides.push_back({component, property, value});
    }
}

void PrefabInstance::remove_override(const std::string& component, const std::string& property) {
    overrides.erase(
        std::remove_if(overrides.begin(), overrides.end(),
            [&](const PropertyOverride& o) {
                return o.component_type == component && o.property_path == property;
            }),
        overrides.end()
    );
}

void PrefabInstance::clear_overrides() {
    overrides.clear();
}

// ============================================================================
// PrefabManager
// ============================================================================

PrefabManager& PrefabManager::instance() {
    static PrefabManager instance;
    return instance;
}

const PrefabData* PrefabManager::load_prefab(const std::string& path) {
    // Check cache first
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        return &it->second;
    }

    // Load from file
    auto content = core::FileSystem::read_text(path);
    if (!content) {
        core::log(core::LogLevel::Error, "PrefabManager: Failed to load prefab '{}'", path);
        return nullptr;
    }

    PrefabData data;
    data.path = path;
    data.json_data = *content;

    // TODO: Parse JSON to extract root_uuid and entity_uuids
    // For now, we'll parse these when instantiating

    m_cache[path] = std::move(data);
    return &m_cache[path];
}

void PrefabManager::unload_prefab(const std::string& path) {
    m_cache.erase(path);
}

void PrefabManager::clear_cache() {
    m_cache.clear();
}

bool PrefabManager::is_loaded(const std::string& path) const {
    return m_cache.find(path) != m_cache.end();
}

Entity PrefabManager::instantiate(World& world, const std::string& prefab_path, Entity parent) {
    const PrefabData* data = load_prefab(prefab_path);
    if (!data || !data->valid()) {
        core::log(core::LogLevel::Error, "PrefabManager: Cannot instantiate invalid prefab '{}'", prefab_path);
        return NullEntity;
    }

    if (!m_serializer) {
        core::log(core::LogLevel::Error, "PrefabManager: No serializer set, cannot instantiate prefab");
        return NullEntity;
    }

    // Deserialize the prefab
    Entity root = m_serializer->deserialize_entity(world, data->json_data, parent);

    if (root == NullEntity) {
        core::log(core::LogLevel::Error, "PrefabManager: Failed to deserialize prefab '{}'", prefab_path);
        return NullEntity;
    }

    // Add PrefabInstance component to root
    world.emplace<PrefabInstance>(root, prefab_path);

    // Mark children as part of this prefab instance
    auto& hierarchy = world.get<Hierarchy>(root);
    std::function<void(Entity)> mark_children = [&](Entity e) {
        for (auto child : get_children(world, e)) {
            if (!world.has<PrefabInstance>(child)) {
                auto& child_instance = world.emplace<PrefabInstance>(child);
                child_instance.prefab_path = prefab_path;
                child_instance.is_root = false;
            }
            mark_children(child);
        }
    };
    mark_children(root);

    core::log(core::LogLevel::Debug, "Instantiated prefab '{}' as entity {}",
              prefab_path, static_cast<uint32_t>(root));

    return root;
}

Entity PrefabManager::instantiate(World& world, const std::string& prefab_path,
                                   const core::Vec3& position,
                                   const core::Quat& rotation,
                                   Entity parent) {
    Entity root = instantiate(world, prefab_path, parent);

    if (root != NullEntity && world.has<LocalTransform>(root)) {
        auto& transform = world.get<LocalTransform>(root);
        transform.position = position;
        transform.rotation = rotation;
    }

    return root;
}

bool PrefabManager::create_prefab(World& world, Entity root, const std::string& save_path) {
    if (!m_serializer) {
        core::log(core::LogLevel::Error, "PrefabManager: No serializer set, cannot create prefab");
        return false;
    }

    if (root == NullEntity || !world.valid(root)) {
        core::log(core::LogLevel::Error, "PrefabManager: Invalid entity for prefab creation");
        return false;
    }

    // Serialize the entity hierarchy
    std::string json = m_serializer->serialize_entity(world, root, true);

    if (json.empty()) {
        core::log(core::LogLevel::Error, "PrefabManager: Failed to serialize entity for prefab");
        return false;
    }

    // Save to file
    if (!core::FileSystem::write_text(save_path, json)) {
        core::log(core::LogLevel::Error, "PrefabManager: Failed to write prefab to '{}'", save_path);
        return false;
    }

    // Update cache
    PrefabData data;
    data.path = save_path;
    data.json_data = json;
    m_cache[save_path] = std::move(data);

    core::log(core::LogLevel::Info, "Created prefab '{}' from entity {}", save_path, static_cast<uint32_t>(root));
    return true;
}

void PrefabManager::update_instances(World& world, const std::string& prefab_path) {
    // Reload prefab
    unload_prefab(prefab_path);
    const PrefabData* data = load_prefab(prefab_path);

    if (!data) {
        return;
    }

    // Find all instances and update them
    auto instances = get_instances(world, prefab_path);

    for (Entity instance_root : instances) {
        // Store current overrides
        auto& prefab_instance = world.get<PrefabInstance>(instance_root);
        std::vector<PropertyOverride> saved_overrides = prefab_instance.overrides;

        // TODO: Re-apply prefab data while preserving overrides
        // This is complex as it requires diffing and merging

        core::log(core::LogLevel::Debug, "Updated prefab instance {} from '{}'",
                  static_cast<uint32_t>(instance_root), prefab_path);
    }
}

void PrefabManager::revert_instance(World& world, Entity instance_root) {
    if (!world.has<PrefabInstance>(instance_root)) {
        return;
    }

    auto& instance = world.get<PrefabInstance>(instance_root);

    if (!instance.is_root) {
        core::log(core::LogLevel::Warn, "Cannot revert non-root prefab instance");
        return;
    }

    // Clear all overrides
    instance.clear_overrides();

    // Re-instantiate from prefab
    // This is a simplified version - a full implementation would need to
    // preserve the entity ID and update in place

    core::log(core::LogLevel::Info, "Reverted prefab instance {} to '{}'",
              static_cast<uint32_t>(instance_root), instance.prefab_path);
}

void PrefabManager::unpack_prefab(World& world, Entity instance_root, bool recursive) {
    if (!world.has<PrefabInstance>(instance_root)) {
        return;
    }

    std::function<void(Entity)> unpack = [&](Entity e) {
        if (world.has<PrefabInstance>(e)) {
            world.remove<PrefabInstance>(e);
        }

        if (recursive) {
            for (auto child : get_children(world, e)) {
                unpack(child);
            }
        }
    };

    unpack(instance_root);

    core::log(core::LogLevel::Info, "Unpacked prefab instance {}",
              static_cast<uint32_t>(instance_root));
}

bool PrefabManager::is_prefab_instance(World& world, Entity entity) const {
    return world.has<PrefabInstance>(entity);
}

Entity PrefabManager::get_prefab_root(World& world, Entity entity) const {
    if (!world.has<PrefabInstance>(entity)) {
        return NullEntity;
    }

    const auto& instance = world.get<PrefabInstance>(entity);
    if (instance.is_root) {
        return entity;
    }

    // Walk up hierarchy to find root
    Entity current = entity;
    while (current != NullEntity) {
        if (world.has<Hierarchy>(current)) {
            Entity parent = world.get<Hierarchy>(current).parent;
            if (parent == NullEntity) {
                break;
            }

            if (world.has<PrefabInstance>(parent)) {
                const auto& parent_instance = world.get<PrefabInstance>(parent);
                if (parent_instance.is_root) {
                    return parent;
                }
            }
            current = parent;
        } else {
            break;
        }
    }

    return NullEntity;
}

std::vector<Entity> PrefabManager::get_instances(World& world, const std::string& prefab_path) const {
    std::vector<Entity> instances;

    auto view = world.view<PrefabInstance>();
    for (auto entity : view) {
        const auto& instance = view.get<PrefabInstance>(entity);
        if (instance.is_root && instance.prefab_path == prefab_path) {
            instances.push_back(entity);
        }
    }

    return instances;
}

void PrefabManager::apply_overrides(World& world, Entity entity, const PrefabInstance& instance) {
    // TODO: Apply overrides using reflection system
    // This would iterate through overrides and use TypeRegistry to set properties
}

std::vector<Entity> PrefabManager::collect_hierarchy(World& world, Entity root) {
    std::vector<Entity> entities;
    entities.push_back(root);

    std::function<void(Entity)> collect = [&](Entity e) {
        for (auto child : get_children(world, e)) {
            entities.push_back(child);
            collect(child);
        }
    };

    collect(root);
    return entities;
}

// ============================================================================
// Prefab Utilities
// ============================================================================

namespace prefab_utils {

bool same_prefab(const std::string& path1, const std::string& path2) {
    // Simple comparison for now - could be expanded to handle relative paths
    return path1 == path2;
}

std::string get_prefab_name(const std::string& path) {
    size_t last_slash = path.find_last_of("/\\");
    size_t last_dot = path.find_last_of('.');

    std::string name;
    if (last_slash != std::string::npos) {
        name = path.substr(last_slash + 1);
    } else {
        name = path;
    }

    if (last_dot != std::string::npos && last_dot > last_slash) {
        size_t name_start = (last_slash != std::string::npos) ? last_slash + 1 : 0;
        return path.substr(name_start, last_dot - name_start);
    }

    return name;
}

bool validate_prefab(const std::string& json_data) {
    // Basic validation - check for required fields
    return !json_data.empty() && json_data.find("uuid") != std::string::npos;
}

void merge_overrides(PrefabInstance& target, const PrefabInstance& source) {
    for (const auto& override : source.overrides) {
        target.set_override(override.component_type, override.property_path, override.json_value);
    }
}

std::vector<PropertyOverride> diff_instances(World& world, Entity instance1, Entity instance2) {
    std::vector<PropertyOverride> diffs;
    // TODO: Compare components using reflection
    return diffs;
}

} // namespace prefab_utils

} // namespace engine::scene
