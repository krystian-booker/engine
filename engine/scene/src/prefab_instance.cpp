#include <engine/scene/prefab_instance.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/scene_serializer.hpp>
#include <engine/reflect/type_registry.hpp>
#include <engine/core/log.hpp>
#include <engine/core/filesystem.hpp>
#include <engine/core/serialize.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>
#include <regex>
#include <unordered_set>

namespace engine::scene {

namespace {

// Helper to parse a JSON value string into the appropriate entt::meta_any
entt::meta_any parse_json_value_to_any(const std::string& json, entt::meta_type target_type) {
    if (!target_type) return {};

    auto type_id = target_type.id();

    // Handle primitive types
    if (type_id == entt::type_hash<bool>::value()) {
        return entt::meta_any{json == "true"};
    }
    else if (type_id == entt::type_hash<int32_t>::value()) {
        try { return entt::meta_any{std::stoi(json)}; }
        catch (...) { return {}; }
    }
    else if (type_id == entt::type_hash<uint32_t>::value()) {
        try { return entt::meta_any{static_cast<uint32_t>(std::stoul(json))}; }
        catch (...) { return {}; }
    }
    else if (type_id == entt::type_hash<float>::value()) {
        try { return entt::meta_any{std::stof(json)}; }
        catch (...) { return {}; }
    }
    else if (type_id == entt::type_hash<double>::value()) {
        try { return entt::meta_any{std::stod(json)}; }
        catch (...) { return {}; }
    }
    else if (type_id == entt::type_hash<std::string>::value()) {
        // Remove surrounding quotes if present
        if (json.size() >= 2 && json.front() == '"' && json.back() == '"') {
            return entt::meta_any{json.substr(1, json.size() - 2)};
        }
        return entt::meta_any{json};
    }
    // Handle Vec3
    else if (type_id == entt::type_hash<core::Vec3>::value()) {
        core::Vec3 result{0.0f};
        std::regex re(R"([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)");
        auto it = std::sregex_iterator(json.begin(), json.end(), re);
        auto end = std::sregex_iterator();
        int i = 0;
        for (; it != end && i < 3; ++it, ++i) {
            try { (&result.x)[i] = std::stof(it->str()); }
            catch (...) {}
        }
        return entt::meta_any{result};
    }
    // Handle Vec4
    else if (type_id == entt::type_hash<core::Vec4>::value()) {
        core::Vec4 result{0.0f};
        std::regex re(R"([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)");
        auto it = std::sregex_iterator(json.begin(), json.end(), re);
        auto end = std::sregex_iterator();
        int i = 0;
        for (; it != end && i < 4; ++it, ++i) {
            try { (&result.x)[i] = std::stof(it->str()); }
            catch (...) {}
        }
        return entt::meta_any{result};
    }
    // Handle Quat
    else if (type_id == entt::type_hash<core::Quat>::value()) {
        core::Quat result{1.0f, 0.0f, 0.0f, 0.0f};
        std::regex re(R"([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)");
        auto it = std::sregex_iterator(json.begin(), json.end(), re);
        auto end = std::sregex_iterator();
        float vals[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        int i = 0;
        for (; it != end && i < 4; ++it, ++i) {
            try { vals[i] = std::stof(it->str()); }
            catch (...) {}
        }
        result.w = vals[0]; result.x = vals[1]; result.y = vals[2]; result.z = vals[3];
        return entt::meta_any{result};
    }

    return {};
}

// Helper to serialize an entt::meta_any to JSON string
std::string any_to_json_string(const entt::meta_any& value) {
    if (!value) return "null";

    auto type_id = value.type().id();

    if (type_id == entt::type_hash<bool>::value()) {
        return value.cast<bool>() ? "true" : "false";
    }
    else if (type_id == entt::type_hash<int32_t>::value()) {
        return std::to_string(value.cast<int32_t>());
    }
    else if (type_id == entt::type_hash<uint32_t>::value()) {
        return std::to_string(value.cast<uint32_t>());
    }
    else if (type_id == entt::type_hash<float>::value()) {
        std::ostringstream ss;
        ss << std::fixed << value.cast<float>();
        return ss.str();
    }
    else if (type_id == entt::type_hash<double>::value()) {
        std::ostringstream ss;
        ss << std::fixed << value.cast<double>();
        return ss.str();
    }
    else if (type_id == entt::type_hash<std::string>::value()) {
        return "\"" + value.cast<std::string>() + "\"";
    }
    else if (type_id == entt::type_hash<core::Vec3>::value()) {
        auto v = value.cast<core::Vec3>();
        std::ostringstream ss;
        ss << std::fixed << "[" << v.x << ", " << v.y << ", " << v.z << "]";
        return ss.str();
    }
    else if (type_id == entt::type_hash<core::Vec4>::value()) {
        auto v = value.cast<core::Vec4>();
        std::ostringstream ss;
        ss << std::fixed << "[" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << "]";
        return ss.str();
    }
    else if (type_id == entt::type_hash<core::Quat>::value()) {
        auto q = value.cast<core::Quat>();
        std::ostringstream ss;
        ss << std::fixed << "[" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << "]";
        return ss.str();
    }

    return "null";
}

} // anonymous namespace

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
    if (content.empty()) {
        core::log(core::LogLevel::Error, "PrefabManager: Failed to load prefab '{}'", path);
        return nullptr;
    }

    PrefabData data;
    data.path = path;
    data.json_data = content;

    try {
        data.entities = serializer().parse_entities_json(content);
        data.entity_uuids.reserve(data.entities.size());
        for (const auto& entity : data.entities) {
            data.entity_uuids.push_back(entity.uuid);
            data.parent_by_uuid[entity.uuid] = entity.parent_uuid;
        }
        if (!data.entity_uuids.empty()) {
            data.root_uuid = data.entity_uuids.front();
        }
    } catch (const std::exception& e) {
        core::log(core::LogLevel::Warn, "PrefabManager: JSON parsing error in '{}': {}", path, e.what());
        return nullptr;
    }

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

    // Deserialize the prefab
    Entity root = serializer().deserialize_entity(world, data->json_data, parent);

    if (root == NullEntity) {
        core::log(core::LogLevel::Error, "PrefabManager: Failed to deserialize prefab '{}'", prefab_path);
        return NullEntity;
    }

    // Add PrefabInstance component to root
    tag_instance_hierarchy(world, root, *data);

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
        sync_world_transform_tree(world, root, true);
    }

    return root;
}

bool PrefabManager::create_prefab(World& world, Entity root, const std::string& save_path) {
    if (root == NullEntity || !world.valid(root)) {
        core::log(core::LogLevel::Error, "PrefabManager: Invalid entity for prefab creation");
        return false;
    }

    // Serialize the entity hierarchy
    std::string json = serializer().serialize_entity(world, root, true);

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
        if (!world.valid(instance_root) || !world.has<PrefabInstance>(instance_root)) continue;
        auto saved_overrides = capture_override_map(world, instance_root, *data);
        if (refresh_instance(world, instance_root, *data, saved_overrides)) {
            core::log(core::LogLevel::Debug, "Updated prefab instance {} from '{}'",
                      static_cast<uint32_t>(instance_root), prefab_path);
        }
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

    const PrefabData* data = load_prefab(instance.prefab_path);
    if (!data || !data->valid()) {
        core::log(core::LogLevel::Warn, "Cannot revert prefab instance {} from missing prefab '{}'",
                  static_cast<uint32_t>(instance_root), instance.prefab_path);
        return;
    }

    std::unordered_map<uint64_t, std::vector<PropertyOverride>> cleared_overrides;
    if (refresh_instance(world, instance_root, *data, cleared_overrides)) {
        core::log(core::LogLevel::Info, "Reverted prefab instance {} to '{}'",
                  static_cast<uint32_t>(instance_root), instance.prefab_path);
    }
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
    auto& registry = world.registry();
    auto& type_registry = reflect::TypeRegistry::instance();

    for (const auto& override : instance.overrides) {
        // Get component from entity
        auto component = type_registry.get_component_any(registry, entity, override.component_type);
        if (!component) {
            core::log(core::LogLevel::Warn, "PrefabManager::apply_overrides: Component '{}' not found on entity",
                      override.component_type);
            continue;
        }

        // Get property info
        const auto* prop_info = type_registry.get_property_info(override.component_type, override.property_path);
        if (!prop_info) {
            core::log(core::LogLevel::Warn, "PrefabManager::apply_overrides: Property '{}' not found in component '{}'",
                      override.property_path, override.component_type);
            continue;
        }

        if (!prop_info->setter) {
            core::log(core::LogLevel::Warn, "PrefabManager::apply_overrides: Property '{}' has no setter",
                      override.property_path);
            continue;
        }

        // Parse the JSON value to the appropriate type
        auto value = parse_json_value_to_any(override.json_value, prop_info->type);
        if (!value) {
            core::log(core::LogLevel::Warn, "PrefabManager::apply_overrides: Failed to parse value '{}' for property '{}'",
                      override.json_value, override.property_path);
            continue;
        }

        // Apply the override
        prop_info->setter(component, value);

        // Write the modified component back to the entity
        type_registry.set_component_any(registry, entity, override.component_type, component);
    }

    core::log(core::LogLevel::Debug, "Applied {} overrides to entity {}",
              instance.overrides.size(), static_cast<uint32_t>(entity));
}

std::vector<Entity> PrefabManager::collect_hierarchy(World& world, Entity root) const {
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

void PrefabManager::tag_instance_hierarchy(World& world, Entity root, const PrefabData& data,
                                           const std::unordered_map<uint64_t, std::vector<PropertyOverride>>* overrides_by_uuid) {
    auto entities = collect_hierarchy(world, root);
    const size_t count = std::min(entities.size(), data.entities.size());

    for (size_t i = 0; i < count; ++i) {
        const uint64_t prefab_uuid = data.entities[i].uuid;
        auto& instance = world.has<PrefabInstance>(entities[i])
            ? world.get<PrefabInstance>(entities[i])
            : world.emplace<PrefabInstance>(entities[i]);
        instance.prefab_path = data.path;
        instance.prefab_entity_uuid = prefab_uuid;
        instance.is_root = (prefab_uuid == data.root_uuid);
        if (overrides_by_uuid) {
            if (auto it = overrides_by_uuid->find(prefab_uuid); it != overrides_by_uuid->end()) {
                instance.overrides = it->second;
            } else {
                instance.overrides.clear();
            }
        }
    }
}

std::unordered_map<uint64_t, Entity> PrefabManager::build_instance_entity_map(World& world, Entity root, const PrefabData& data) const {
    std::unordered_map<uint64_t, Entity> result;
    auto entities = collect_hierarchy(world, root);
    std::unordered_set<Entity> mapped_entities;

    for (Entity entity : entities) {
        if (const auto* instance = world.try_get<PrefabInstance>(entity);
            instance && instance->prefab_entity_uuid != 0) {
            result[instance->prefab_entity_uuid] = entity;
            mapped_entities.insert(entity);
        }
    }

    const size_t count = std::min(entities.size(), data.entities.size());
    for (size_t i = 0; i < count; ++i) {
        if (!result.contains(data.entities[i].uuid) && !mapped_entities.contains(entities[i])) {
            result[data.entities[i].uuid] = entities[i];
        }
    }

    return result;
}

std::unordered_map<uint64_t, std::vector<PropertyOverride>> PrefabManager::capture_override_map(World& world, Entity root, const PrefabData& data) const {
    std::unordered_map<uint64_t, std::vector<PropertyOverride>> overrides_by_uuid;
    auto entities = collect_hierarchy(world, root);
    std::unordered_set<Entity> mapped_entities;

    for (Entity entity : entities) {
        if (const auto* instance = world.try_get<PrefabInstance>(entity);
            instance && instance->prefab_entity_uuid != 0) {
            overrides_by_uuid[instance->prefab_entity_uuid] = instance->overrides;
            mapped_entities.insert(entity);
        }
    }

    const size_t count = std::min(entities.size(), data.entities.size());
    for (size_t i = 0; i < count; ++i) {
        if (!overrides_by_uuid.contains(data.entities[i].uuid) && !mapped_entities.contains(entities[i])) {
            if (const auto* instance = world.try_get<PrefabInstance>(entities[i])) {
                overrides_by_uuid[data.entities[i].uuid] = instance->overrides;
            }
        }
    }

    return overrides_by_uuid;
}

bool PrefabManager::refresh_instance(World& world, Entity instance_root, const PrefabData& data,
                                     const std::unordered_map<uint64_t, std::vector<PropertyOverride>>& overrides_by_uuid) {
    if (!world.valid(instance_root) || data.entities.empty()) {
        return false;
    }

    const Entity external_parent = world.has<Hierarchy>(instance_root)
        ? world.get<Hierarchy>(instance_root).parent
        : NullEntity;

    const auto old_entities = collect_hierarchy(world, instance_root);
    auto existing_map = build_instance_entity_map(world, instance_root, data);
    std::unordered_map<uint64_t, Entity> final_map;
    final_map.reserve(data.entities.size());

    for (const auto& serialized : data.entities) {
        Entity target = NullEntity;
        if (auto it = existing_map.find(serialized.uuid); it != existing_map.end() && world.valid(it->second)) {
            target = it->second;
        } else {
            target = world.create(serialized.name);
        }
        final_map[serialized.uuid] = target;
    }

    EntityResolutionContext entity_ctx;
    entity_ctx.entity_to_uuid = [&world](entt::entity e) -> uint64_t {
        if (world.has<EntityInfo>(e)) {
            return world.get<EntityInfo>(e).uuid;
        }
        return EntityResolutionContext::NullUUID;
    };
    entity_ctx.uuid_to_entity = [&final_map](uint64_t uuid) -> entt::entity {
        auto it = final_map.find(uuid);
        return (it != final_map.end()) ? it->second : entt::null;
    };

    for (const auto& serialized : data.entities) {
        serializer().apply_entity(world, final_map.at(serialized.uuid), serialized, &entity_ctx, true);
    }

    for (const auto& serialized : data.entities) {
        Entity target = final_map.at(serialized.uuid);
        Entity desired_parent = NullEntity;
        if (serialized.parent_uuid != 0) {
            desired_parent = final_map.at(serialized.parent_uuid);
        } else if (serialized.uuid == data.root_uuid) {
            desired_parent = external_parent;
        }
        set_parent(world, target, desired_parent, NullEntity);
    }

    tag_instance_hierarchy(world, final_map.at(data.root_uuid), data, &overrides_by_uuid);

    for (const auto& serialized : data.entities) {
        Entity target = final_map.at(serialized.uuid);
        if (world.has<PrefabInstance>(target)) {
            apply_overrides(world, target, world.get<PrefabInstance>(target));
        }
    }

    sync_world_transform_tree(world, final_map.at(data.root_uuid), true);

    std::unordered_set<Entity> live_entities;
    for (const auto& [uuid, entity] : final_map) {
        (void)uuid;
        live_entities.insert(entity);
    }

    for (auto it = old_entities.rbegin(); it != old_entities.rend(); ++it) {
        if (live_entities.contains(*it) || !world.valid(*it)) {
            continue;
        }
        world.destroy(*it);
    }

    return true;
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
    auto& registry = world.registry();
    auto& type_registry = reflect::TypeRegistry::instance();

    // Iterate through all registered component types
    for (const auto& type_name : type_registry.get_all_component_names()) {
        // Get components from both entities
        auto comp1 = type_registry.get_component_any(registry, instance1, type_name);
        auto comp2 = type_registry.get_component_any(registry, instance2, type_name);

        // Skip if either entity doesn't have this component
        if (!comp1 || !comp2) continue;

        // Get type info to iterate properties
        const auto* type_info = type_registry.get_type_info(type_name);
        if (!type_info) continue;

        // Compare each property
        for (const auto& prop : type_info->properties) {
            if (!prop.getter) continue;

            auto val1 = prop.getter(comp1);
            auto val2 = prop.getter(comp2);

            if (!val1 || !val2) continue;

            // Convert to JSON strings for comparison
            std::string json1 = any_to_json_string(val1);
            std::string json2 = any_to_json_string(val2);

            // If values differ, add to diffs (using instance1's value)
            if (json1 != json2) {
                PropertyOverride diff;
                diff.component_type = type_name;
                diff.property_path = prop.name;
                diff.json_value = json1;
                diffs.push_back(diff);
            }
        }
    }

    return diffs;
}

} // namespace prefab_utils

} // namespace engine::scene
