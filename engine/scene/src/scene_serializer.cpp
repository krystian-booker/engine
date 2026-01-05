#include <engine/scene/scene_serializer.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/reflect/type_registry.hpp>
#include <engine/core/log.hpp>
#include <random>
#include <chrono>
#include <iomanip>
#include <regex>

namespace engine::scene {

using namespace engine::core;

// Safe number conversion helpers - return default on parse failure instead of throwing
namespace {

float safe_stof(const std::string& str, float default_val = 0.0f) {
    try {
        return std::stof(str);
    } catch (const std::exception&) {
        log(LogLevel::Warn, "Failed to parse float: '{}'", str);
        return default_val;
    }
}

unsigned long safe_stoul(const std::string& str, unsigned long default_val = 0) {
    try {
        return std::stoul(str);
    } catch (const std::exception&) {
        log(LogLevel::Warn, "Failed to parse unsigned long: '{}'", str);
        return default_val;
    }
}

uint64_t safe_stoull(const std::string& str, uint64_t default_val = 0) {
    try {
        return std::stoull(str);
    } catch (const std::exception&) {
        log(LogLevel::Warn, "Failed to parse uint64: '{}'", str);
        return default_val;
    }
}

} // anonymous namespace

// UUID generation using random + timestamp
uint64_t SceneSerializer::generate_uuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;

    // Combine random bits with timestamp for better uniqueness
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
    uint64_t timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());

    return dist(gen) ^ (timestamp << 32) ^ (timestamp >> 32);
}

SceneSerializer::SceneSerializer(const SerializerConfig& config)
    : m_config(config) {
}

std::string SceneSerializer::serialize(World& world) const {
    SerializedScene scene;
    scene.name = world.get_scene_name();

    // Get all root entities
    auto roots = get_root_entities(world);

    // Recursive serialization helper
    std::function<void(Entity)> serialize_recursive = [&](Entity entity) {
        scene.entities.push_back(serialize_entity_internal(world, entity));

        // Serialize children
        if (world.has<Hierarchy>(entity)) {
            const auto& hier = world.get<Hierarchy>(entity);
            Entity child = hier.first_child;
            while (child != NullEntity) {
                serialize_recursive(child);
                if (world.has<Hierarchy>(child)) {
                    child = world.get<Hierarchy>(child).next_sibling;
                } else {
                    break;
                }
            }
        }
    };

    for (Entity root : roots) {
        serialize_recursive(root);
    }

    return scene_to_json(scene);
}

bool SceneSerializer::serialize_to_file(World& world, const std::string& path) const {
    std::string json = serialize(world);

    std::ofstream file(path);
    if (!file.is_open()) {
        log(LogLevel::Error, "Failed to open file for writing: {}", path);
        return false;
    }

    file << json;
    if (!file.good()) {
        log(LogLevel::Error, "Failed to write scene data to: {}", path);
        return false;
    }
    file.close();

    log(LogLevel::Info, "Scene serialized to: {}", path);
    return true;
}

bool SceneSerializer::deserialize(World& world, const std::string& json) {
    try {
        SerializedScene scene = parse_scene_json(json);

        // Store scene metadata
        world.set_scene_name(scene.name);
        for (const auto& [key, value] : scene.metadata) {
            world.get_scene_metadata()[key] = value;
        }

        // Build UUID to entity mapping for parent resolution
        std::unordered_map<uint64_t, Entity> uuid_to_entity;

        // First pass: create all entities
        for (const auto& entity_data : scene.entities) {
            Entity entity = world.create();

            // Add EntityInfo
            EntityInfo& info = world.emplace<EntityInfo>(entity);
            info.name = entity_data.name;
            info.uuid = entity_data.uuid;
            info.enabled = entity_data.enabled;

            uuid_to_entity[entity_data.uuid] = entity;
        }

        // Create entity resolution context for resolving entity references
        EntityResolutionContext entity_ctx;
        entity_ctx.entity_to_uuid = [&world](entt::entity e) -> uint64_t {
            if (world.has<EntityInfo>(e)) {
                return world.get<EntityInfo>(e).uuid;
            }
            return EntityResolutionContext::NullUUID;
        };
        entity_ctx.uuid_to_entity = [&uuid_to_entity](uint64_t uuid) -> entt::entity {
            auto it = uuid_to_entity.find(uuid);
            return (it != uuid_to_entity.end()) ? it->second : entt::null;
        };

        // Second pass: set up hierarchy and components
        for (const auto& entity_data : scene.entities) {
            auto it = uuid_to_entity.find(entity_data.uuid);
            if (it == uuid_to_entity.end()) continue;
            Entity entity = it->second;

            // Set parent
            if (entity_data.parent_uuid != 0) {
                auto parent_it = uuid_to_entity.find(entity_data.parent_uuid);
                if (parent_it != uuid_to_entity.end()) {
                    set_parent(world, entity, parent_it->second);
                }
            }

            // Deserialize components
            for (const auto& comp : entity_data.components) {
                if (comp.type_name == "LocalTransform") {
                    LocalTransform* existing = world.try_get<LocalTransform>(entity);
                    LocalTransform& transform = existing ? *existing : world.emplace<LocalTransform>(entity);
                    deserialize_transform(transform, comp.json_data);
                } else if (comp.type_name == "MeshRenderer") {
                    MeshRenderer& renderer = world.emplace_or_replace<MeshRenderer>(entity);
                    deserialize_mesh_renderer(renderer, comp.json_data);
                } else if (comp.type_name == "Camera") {
                    Camera& camera = world.emplace_or_replace<Camera>(entity);
                    deserialize_camera(camera, comp.json_data);
                } else if (comp.type_name == "Light") {
                    Light& light = world.emplace_or_replace<Light>(entity);
                    deserialize_light(light, comp.json_data);
                } else if (comp.type_name == "ParticleEmitter") {
                    ParticleEmitter& emitter = world.emplace_or_replace<ParticleEmitter>(entity);
                    deserialize_particle_emitter(emitter, comp.json_data);
                } else {
                    // Try custom deserializer first
                    auto deserializer_it = m_component_deserializers.find(comp.type_name);
                    if (deserializer_it != m_component_deserializers.end()) {
                        // Use registered custom deserializer
                        deserializer_it->second(nullptr, comp.json_data);
                    } else {
                        // Use reflection system to dynamically deserialize (with entity context)
                        if (!deserialize_custom_component(world, entity, comp.type_name, comp.json_data, &entity_ctx)) {
                            log(LogLevel::Warn, "Unknown component type '{}' - skipping", comp.type_name);
                        }
                    }
                }
            }
        }

        log(LogLevel::Info, "Scene deserialized: {} entities", scene.entities.size());
        return true;

    } catch (const std::exception& e) {
        log(LogLevel::Error, "Failed to deserialize scene: {}", e.what());
        return false;
    }
}

bool SceneSerializer::deserialize_from_file(World& world, const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        log(LogLevel::Error, "Failed to open file for reading: {}", path);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return deserialize(world, buffer.str());
}

std::string SceneSerializer::serialize_entity(World& world, Entity entity, bool include_children) const {
    std::vector<SerializedEntity> entities;

    std::function<void(Entity)> serialize_recursive = [&](Entity e) {
        entities.push_back(serialize_entity_internal(world, e));

        if (include_children && world.has<Hierarchy>(e)) {
            const auto& hier = world.get<Hierarchy>(e);
            Entity child = hier.first_child;
            while (child != NullEntity) {
                serialize_recursive(child);
                if (world.has<Hierarchy>(child)) {
                    child = world.get<Hierarchy>(child).next_sibling;
                } else {
                    break;
                }
            }
        }
    };

    serialize_recursive(entity);

    // Build JSON array
    std::ostringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < entities.size(); ++i) {
        ss << entity_to_json(entities[i]);
        if (i < entities.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "]";

    return ss.str();
}

Entity SceneSerializer::deserialize_entity(World& world, const std::string& json, Entity parent) {
    // Parse as array of entities
    // For now, just create single entity (simplified)
    SerializedEntity data = parse_entity_json(json);
    return create_entity_from_serialized(world, data, parent);
}

SerializedEntity SceneSerializer::serialize_entity_internal(World& world, Entity entity) const {
    SerializedEntity data;

    // Get or generate UUID
    if (world.has<EntityInfo>(entity)) {
        const auto& info = world.get<EntityInfo>(entity);
        data.uuid = info.uuid;
        data.name = info.name;
        data.enabled = info.enabled;
    } else {
        data.uuid = generate_uuid();
        data.name = "Entity";
        data.enabled = true;
    }

    // Get parent UUID
    if (world.has<Hierarchy>(entity)) {
        const auto& hier = world.get<Hierarchy>(entity);
        if (hier.parent != NullEntity && world.has<EntityInfo>(hier.parent)) {
            data.parent_uuid = world.get<EntityInfo>(hier.parent).uuid;
        }
    }

    // Serialize components
    if (world.has<LocalTransform>(entity)) {
        SerializedComponent comp;
        comp.type_name = "LocalTransform";
        comp.json_data = serialize_transform(world.get<LocalTransform>(entity));
        data.components.push_back(comp);
    }

    if (world.has<MeshRenderer>(entity)) {
        SerializedComponent comp;
        comp.type_name = "MeshRenderer";
        comp.json_data = serialize_mesh_renderer(world.get<MeshRenderer>(entity));
        data.components.push_back(comp);
    }

    if (world.has<Camera>(entity)) {
        SerializedComponent comp;
        comp.type_name = "Camera";
        comp.json_data = serialize_camera(world.get<Camera>(entity));
        data.components.push_back(comp);
    }

    if (world.has<Light>(entity)) {
        SerializedComponent comp;
        comp.type_name = "Light";
        comp.json_data = serialize_light(world.get<Light>(entity));
        data.components.push_back(comp);
    }

    if (world.has<ParticleEmitter>(entity)) {
        SerializedComponent comp;
        comp.type_name = "ParticleEmitter";
        comp.json_data = serialize_particle_emitter(world.get<ParticleEmitter>(entity));
        data.components.push_back(comp);
    }

    return data;
}

// JSON helpers
std::string SceneSerializer::to_json(const Vec3& v) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "[" << v.x << ", " << v.y << ", " << v.z << "]";
    return ss.str();
}

std::string SceneSerializer::to_json(const Vec4& v) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "[" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << "]";
    return ss.str();
}

std::string SceneSerializer::to_json(const Quat& q) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "[" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << "]";
    return ss.str();
}

std::string SceneSerializer::to_json(const Mat4& m) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "[";
    for (int i = 0; i < 16; ++i) {
        ss << (&m[0][0])[i];
        if (i < 15) ss << ", ";
    }
    ss << "]";
    return ss.str();
}

Vec3 SceneSerializer::parse_vec3(const std::string& json) const {
    Vec3 result{0.0f};
    std::regex re(R"(\[\s*([+-]?\d*\.?\d+)\s*,\s*([+-]?\d*\.?\d+)\s*,\s*([+-]?\d*\.?\d+)\s*\])");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() == 4) {
        result.x = safe_stof(match[1].str());
        result.y = safe_stof(match[2].str());
        result.z = safe_stof(match[3].str());
    }
    return result;
}

Vec4 SceneSerializer::parse_vec4(const std::string& json) const {
    Vec4 result{0.0f};
    std::regex re(R"(\[\s*([+-]?\d*\.?\d+)\s*,\s*([+-]?\d*\.?\d+)\s*,\s*([+-]?\d*\.?\d+)\s*,\s*([+-]?\d*\.?\d+)\s*\])");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() == 5) {
        result.x = safe_stof(match[1].str());
        result.y = safe_stof(match[2].str());
        result.z = safe_stof(match[3].str());
        result.w = safe_stof(match[4].str());
    }
    return result;
}

Quat SceneSerializer::parse_quat(const std::string& json) const {
    Quat result{1.0f, 0.0f, 0.0f, 0.0f};
    std::regex re(R"(\[\s*([+-]?\d*\.?\d+)\s*,\s*([+-]?\d*\.?\d+)\s*,\s*([+-]?\d*\.?\d+)\s*,\s*([+-]?\d*\.?\d+)\s*\])");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() == 5) {
        result.w = safe_stof(match[1].str());
        result.x = safe_stof(match[2].str());
        result.y = safe_stof(match[3].str());
        result.z = safe_stof(match[4].str());
    }
    return result;
}

Mat4 SceneSerializer::parse_mat4(const std::string& json) const {
    Mat4 result{1.0f};
    // Parse array of 16 floats from JSON format: [f0, f1, f2, ... f15]
    std::regex num_re(R"([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)");
    auto nums_begin = std::sregex_iterator(json.begin(), json.end(), num_re);
    auto nums_end = std::sregex_iterator();

    int i = 0;
    for (auto it = nums_begin; it != nums_end && i < 16; ++it, ++i) {
        (&result[0][0])[i] = safe_stof(it->str());
    }

    if (i < 16 && i > 0) {
        log(LogLevel::Warn, "parse_mat4: Expected 16 values, got {}", i);
    }

    return result;
}

// Component serialization
std::string SceneSerializer::serialize_transform(const LocalTransform& transform) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"position\": " << to_json(transform.position) << ",\n";
    ss << "  \"rotation\": " << to_json(transform.rotation) << ",\n";
    ss << "  \"scale\": " << to_json(transform.scale) << "\n";
    ss << "}";
    return ss.str();
}

std::string SceneSerializer::serialize_mesh_renderer(const MeshRenderer& renderer) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"mesh\": " << serialize_asset_handle(renderer.mesh, "mesh") << ",\n";
    ss << "  \"material\": " << serialize_asset_handle(renderer.material, "material") << ",\n";
    ss << "  \"render_layer\": " << static_cast<int>(renderer.render_layer) << ",\n";
    ss << "  \"visible\": " << (renderer.visible ? "true" : "false") << ",\n";
    ss << "  \"cast_shadows\": " << (renderer.cast_shadows ? "true" : "false") << ",\n";
    ss << "  \"receive_shadows\": " << (renderer.receive_shadows ? "true" : "false") << "\n";
    ss << "}";
    return ss.str();
}

std::string SceneSerializer::serialize_camera(const Camera& camera) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\n";
    ss << "  \"fov\": " << camera.fov << ",\n";
    ss << "  \"near_plane\": " << camera.near_plane << ",\n";
    ss << "  \"far_plane\": " << camera.far_plane << ",\n";
    ss << "  \"aspect_ratio\": " << camera.aspect_ratio << ",\n";
    ss << "  \"priority\": " << static_cast<int>(camera.priority) << ",\n";
    ss << "  \"active\": " << (camera.active ? "true" : "false") << ",\n";
    ss << "  \"orthographic\": " << (camera.orthographic ? "true" : "false") << ",\n";
    ss << "  \"ortho_size\": " << camera.ortho_size << "\n";
    ss << "}";
    return ss.str();
}

std::string SceneSerializer::serialize_light(const Light& light) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\n";
    ss << "  \"type\": " << static_cast<int>(light.type) << ",\n";
    ss << "  \"color\": " << to_json(light.color) << ",\n";
    ss << "  \"intensity\": " << light.intensity << ",\n";
    ss << "  \"range\": " << light.range << ",\n";
    ss << "  \"spot_inner_angle\": " << light.spot_inner_angle << ",\n";
    ss << "  \"spot_outer_angle\": " << light.spot_outer_angle << ",\n";
    ss << "  \"cast_shadows\": " << (light.cast_shadows ? "true" : "false") << ",\n";
    ss << "  \"enabled\": " << (light.enabled ? "true" : "false") << "\n";
    ss << "}";
    return ss.str();
}

std::string SceneSerializer::serialize_particle_emitter(const ParticleEmitter& emitter) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\n";
    ss << "  \"max_particles\": " << emitter.max_particles << ",\n";
    ss << "  \"emission_rate\": " << emitter.emission_rate << ",\n";
    ss << "  \"lifetime\": " << emitter.lifetime << ",\n";
    ss << "  \"initial_speed\": " << emitter.initial_speed << ",\n";
    ss << "  \"initial_velocity_variance\": " << to_json(emitter.initial_velocity_variance) << ",\n";
    ss << "  \"start_color\": " << to_json(emitter.start_color) << ",\n";
    ss << "  \"end_color\": " << to_json(emitter.end_color) << ",\n";
    ss << "  \"start_size\": " << emitter.start_size << ",\n";
    ss << "  \"end_size\": " << emitter.end_size << ",\n";
    ss << "  \"gravity\": " << to_json(emitter.gravity) << ",\n";
    ss << "  \"enabled\": " << (emitter.enabled ? "true" : "false") << "\n";
    ss << "}";
    return ss.str();
}

// Component deserialization
void SceneSerializer::deserialize_transform(LocalTransform& transform, const std::string& json) const {
    // Simple regex-based parsing
    std::regex pos_re(R"("position"\s*:\s*(\[[^\]]+\]))");
    std::regex rot_re(R"("rotation"\s*:\s*(\[[^\]]+\]))");
    std::regex scale_re(R"("scale"\s*:\s*(\[[^\]]+\]))");

    std::smatch match;
    if (std::regex_search(json, match, pos_re)) {
        transform.position = parse_vec3(match[1].str());
    }
    if (std::regex_search(json, match, rot_re)) {
        transform.rotation = parse_quat(match[1].str());
    }
    if (std::regex_search(json, match, scale_re)) {
        transform.scale = parse_vec3(match[1].str());
    }
}

void SceneSerializer::deserialize_mesh_renderer(MeshRenderer& renderer, const std::string& json) const {
    std::regex layer_re(R"("render_layer"\s*:\s*(\d+))");
    std::regex visible_re(R"("visible"\s*:\s*(true|false))");
    std::regex cast_re(R"("cast_shadows"\s*:\s*(true|false))");
    std::regex recv_re(R"("receive_shadows"\s*:\s*(true|false))");

    std::smatch match;
    
    // Deserialize asset handles (supports both raw IDs and path-validated handles)
    std::regex mesh_re(R"("mesh"\s*:\s*(\{[^}]+\}|\d+))");  
    if (std::regex_search(json, match, mesh_re)) {
        renderer.mesh = deserialize_asset_handle<MeshHandle>(match[1].str(), "mesh");
    }
    
    std::regex mat_re(R"("material"\s*:\s*(\{[^}]+\}|\d+))");
    if (std::regex_search(json, match, mat_re)) {
        renderer.material = deserialize_asset_handle<MaterialHandle>(match[1].str(), "material");
    }
    
    if (std::regex_search(json, match, layer_re)) {
        renderer.render_layer = static_cast<uint8_t>(safe_stoul(match[1].str()));
    }
    if (std::regex_search(json, match, visible_re)) {
        renderer.visible = (match[1].str() == "true");
    }
    if (std::regex_search(json, match, cast_re)) {
        renderer.cast_shadows = (match[1].str() == "true");
    }
    if (std::regex_search(json, match, recv_re)) {
        renderer.receive_shadows = (match[1].str() == "true");
    }
}

void SceneSerializer::deserialize_camera(Camera& camera, const std::string& json) const {
    std::regex fov_re(R"("fov"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex near_re(R"("near_plane"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex far_re(R"("far_plane"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex aspect_re(R"("aspect_ratio"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex priority_re(R"("priority"\s*:\s*(\d+))");
    std::regex active_re(R"("active"\s*:\s*(true|false))");
    std::regex ortho_re(R"("orthographic"\s*:\s*(true|false))");
    std::regex ortho_size_re(R"("ortho_size"\s*:\s*([+-]?\d*\.?\d+))");

    std::smatch match;
    if (std::regex_search(json, match, fov_re)) {
        camera.fov = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, near_re)) {
        camera.near_plane = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, far_re)) {
        camera.far_plane = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, aspect_re)) {
        camera.aspect_ratio = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, priority_re)) {
        camera.priority = static_cast<uint8_t>(safe_stoul(match[1].str()));
    }
    if (std::regex_search(json, match, active_re)) {
        camera.active = (match[1].str() == "true");
    }
    if (std::regex_search(json, match, ortho_re)) {
        camera.orthographic = (match[1].str() == "true");
    }
    if (std::regex_search(json, match, ortho_size_re)) {
        camera.ortho_size = safe_stof(match[1].str());
    }
}

void SceneSerializer::deserialize_light(Light& light, const std::string& json) const {
    std::regex type_re(R"("type"\s*:\s*(\d+))");
    std::regex color_re(R"("color"\s*:\s*(\[[^\]]+\]))");
    std::regex intensity_re(R"("intensity"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex range_re(R"("range"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex inner_re(R"("spot_inner_angle"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex outer_re(R"("spot_outer_angle"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex shadows_re(R"("cast_shadows"\s*:\s*(true|false))");
    std::regex enabled_re(R"("enabled"\s*:\s*(true|false))");

    std::smatch match;
    if (std::regex_search(json, match, type_re)) {
        auto type_val = safe_stoul(match[1].str());
        if (type_val <= static_cast<unsigned long>(LightType::Spot)) {
            light.type = static_cast<LightType>(type_val);
        }
    }
    if (std::regex_search(json, match, color_re)) {
        light.color = parse_vec3(match[1].str());
    }
    if (std::regex_search(json, match, intensity_re)) {
        light.intensity = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, range_re)) {
        light.range = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, inner_re)) {
        light.spot_inner_angle = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, outer_re)) {
        light.spot_outer_angle = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, shadows_re)) {
        light.cast_shadows = (match[1].str() == "true");
    }
    if (std::regex_search(json, match, enabled_re)) {
        light.enabled = (match[1].str() == "true");
    }
}

void SceneSerializer::deserialize_particle_emitter(ParticleEmitter& emitter, const std::string& json) const {
    std::regex max_re(R"("max_particles"\s*:\s*(\d+))");
    std::regex rate_re(R"("emission_rate"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex life_re(R"("lifetime"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex speed_re(R"("initial_speed"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex variance_re(R"("initial_velocity_variance"\s*:\s*(\[[^\]]+\]))");
    std::regex start_color_re(R"("start_color"\s*:\s*(\[[^\]]+\]))");
    std::regex end_color_re(R"("end_color"\s*:\s*(\[[^\]]+\]))");
    std::regex start_size_re(R"("start_size"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex end_size_re(R"("end_size"\s*:\s*([+-]?\d*\.?\d+))");
    std::regex gravity_re(R"("gravity"\s*:\s*(\[[^\]]+\]))");
    std::regex enabled_re(R"("enabled"\s*:\s*(true|false))");

    std::smatch match;
    if (std::regex_search(json, match, max_re)) {
        emitter.max_particles = safe_stoul(match[1].str());
    }
    if (std::regex_search(json, match, rate_re)) {
        emitter.emission_rate = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, life_re)) {
        emitter.lifetime = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, speed_re)) {
        emitter.initial_speed = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, variance_re)) {
        emitter.initial_velocity_variance = parse_vec3(match[1].str());
    }
    if (std::regex_search(json, match, start_color_re)) {
        emitter.start_color = parse_vec4(match[1].str());
    }
    if (std::regex_search(json, match, end_color_re)) {
        emitter.end_color = parse_vec4(match[1].str());
    }
    if (std::regex_search(json, match, start_size_re)) {
        emitter.start_size = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, end_size_re)) {
        emitter.end_size = safe_stof(match[1].str());
    }
    if (std::regex_search(json, match, gravity_re)) {
        emitter.gravity = parse_vec3(match[1].str());
    }
    if (std::regex_search(json, match, enabled_re)) {
        emitter.enabled = (match[1].str() == "true");
    }
}

std::string SceneSerializer::entity_to_json(const SerializedEntity& entity) const {
    std::ostringstream ss;
    std::string indent = m_config.pretty_print ? "  " : "";
    std::string newline = m_config.pretty_print ? "\n" : "";

    ss << "{" << newline;
    ss << indent << "\"uuid\": " << entity.uuid << "," << newline;
    ss << indent << "\"name\": \"" << entity.name << "\"," << newline;
    ss << indent << "\"enabled\": " << (entity.enabled ? "true" : "false") << "," << newline;
    ss << indent << "\"parent_uuid\": " << entity.parent_uuid << "," << newline;
    ss << indent << "\"components\": [" << newline;

    for (size_t i = 0; i < entity.components.size(); ++i) {
        const auto& comp = entity.components[i];
        ss << indent << indent << "{" << newline;
        ss << indent << indent << indent << "\"type\": \"" << comp.type_name << "\"," << newline;
        ss << indent << indent << indent << "\"data\": " << comp.json_data << newline;
        ss << indent << indent << "}";
        if (i < entity.components.size() - 1) ss << ",";
        ss << newline;
    }

    ss << indent << "]" << newline;
    ss << "}";

    return ss.str();
}

std::string SceneSerializer::scene_to_json(const SerializedScene& scene) const {
    std::ostringstream ss;
    std::string indent = m_config.pretty_print ? "  " : "";
    std::string newline = m_config.pretty_print ? "\n" : "";

    ss << "{" << newline;
    ss << indent << "\"name\": \"" << scene.name << "\"," << newline;
    ss << indent << "\"version\": \"" << scene.version << "\"," << newline;
    ss << indent << "\"entities\": [" << newline;

    for (size_t i = 0; i < scene.entities.size(); ++i) {
        // Indent each line of entity JSON
        std::string entity_json = entity_to_json(scene.entities[i]);
        std::istringstream iss(entity_json);
        std::string line;
        while (std::getline(iss, line)) {
            ss << indent << indent << line << newline;
        }
        if (i < scene.entities.size() - 1) ss << ",";
    }

    ss << indent << "]" << newline;
    ss << "}";

    return ss.str();
}

SerializedScene SceneSerializer::parse_scene_json(const std::string& json) {
    SerializedScene scene;

    // Parse name
    std::regex name_re(R"REGEX("name"\s*:\s*"([^"]*)")REGEX");
    std::smatch match;
    if (std::regex_search(json, match, name_re)) {
        scene.name = match[1].str();
    }

    // Parse version
    std::regex version_re(R"REGEX("version"\s*:\s*"([^"]*)")REGEX");
    if (std::regex_search(json, match, version_re)) {
        scene.version = match[1].str();
    }

    // Parse entities array - this is simplified, real implementation would use proper JSON parser
    // For now, we rely on well-formed JSON from our own serializer

    return scene;
}

SerializedEntity SceneSerializer::parse_entity_json(const std::string& json) {
    SerializedEntity entity;

    std::regex uuid_re(R"("uuid"\s*:\s*(\d+))");
    std::regex name_re(R"REGEX("name"\s*:\s*"([^"]*)")REGEX");
    std::regex enabled_re(R"("enabled"\s*:\s*(true|false))");
    std::regex parent_re(R"("parent_uuid"\s*:\s*(\d+))");

    std::smatch match;
    if (std::regex_search(json, match, uuid_re)) {
        entity.uuid = safe_stoull(match[1].str());
    }
    if (std::regex_search(json, match, name_re)) {
        entity.name = match[1].str();
    }
    if (std::regex_search(json, match, enabled_re)) {
        entity.enabled = (match[1].str() == "true");
    }
    if (std::regex_search(json, match, parent_re)) {
        entity.parent_uuid = safe_stoull(match[1].str());
    }

    return entity;
}

Entity SceneSerializer::create_entity_from_serialized(World& world, const SerializedEntity& data,
                                                       Entity parent) {
    Entity entity = world.create();

    EntityInfo& info = world.emplace<EntityInfo>(entity);
    info.name = data.name;
    info.uuid = data.uuid != 0 ? data.uuid : generate_uuid();
    info.enabled = data.enabled;

    if (parent != NullEntity) {
        set_parent(world, entity, parent);
    }

    return entity;
}

// Prefab implementation
Prefab Prefab::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        log(LogLevel::Error, "Failed to load prefab: {}", path);
        return Prefab{};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return Prefab{buffer.str()};
}

bool Prefab::save(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        log(LogLevel::Error, "Failed to save prefab: {}", path);
        return false;
    }

    file << m_data;
    return true;
}

Entity Prefab::instantiate(World& world, SceneSerializer& serializer, Entity parent) const {
    if (!valid()) return NullEntity;
    return serializer.deserialize_entity(world, m_data, parent);
}

Prefab Prefab::create_from_entity(World& world, SceneSerializer& serializer, Entity entity) {
    return Prefab{serializer.serialize_entity(world, entity, true)};
}

// Scene utility functions
namespace scene_utils {

Entity clone_entity(World& world, Entity source, Entity new_parent) {
    if (source == NullEntity) return NullEntity;

    SceneSerializer serializer;
    std::string json = serializer.serialize_entity(world, source, true);

    // Generate new UUIDs for cloned entities
    // (simplified - real implementation would modify JSON)

    return serializer.deserialize_entity(world, json, new_parent);
}

void delete_entity_recursive(World& world, Entity entity) {
    if (entity == NullEntity) return;

    // First delete all children recursively
    if (world.has<Hierarchy>(entity)) {
        const auto& hier = world.get<Hierarchy>(entity);
        Entity child = hier.first_child;
        while (child != NullEntity) {
            Entity next = NullEntity;
            if (world.has<Hierarchy>(child)) {
                next = world.get<Hierarchy>(child).next_sibling;
            }
            delete_entity_recursive(world, child);
            child = next;
        }
    }

    // Then destroy this entity
    world.destroy(entity);
}

Entity find_entity_by_name(World& world, const std::string& name) {
    Entity result = NullEntity;

    world.view<EntityInfo>().each([&](Entity entity, const EntityInfo& info) {
        if (result == NullEntity && info.name == name) {
            result = entity;
        }
    });

    return result;
}

std::vector<Entity> find_entities_by_name(World& world, const std::string& name) {
    std::vector<Entity> results;

    world.view<EntityInfo>().each([&](Entity entity, const EntityInfo& info) {
        if (info.name == name) {
            results.push_back(entity);
        }
    });

    return results;
}

Entity find_entity_by_uuid(World& world, uint64_t uuid) {
    Entity result = NullEntity;

    world.view<EntityInfo>().each([&](Entity entity, const EntityInfo& info) {
        if (result == NullEntity && info.uuid == uuid) {
            result = entity;
        }
    });

    return result;
}

std::string get_entity_path(World& world, Entity entity) {
    if (entity == NullEntity) return "";

    std::vector<std::string> names;

    Entity current = entity;
    while (current != NullEntity) {
        if (world.has<EntityInfo>(current)) {
            names.push_back(world.get<EntityInfo>(current).name);
        } else {
            names.push_back("?");
        }

        if (world.has<Hierarchy>(current)) {
            current = world.get<Hierarchy>(current).parent;
        } else {
            break;
        }
    }

    std::reverse(names.begin(), names.end());

    std::string path;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) path += "/";
        path += names[i];
    }

    return path;
}

Entity find_entity_by_path(World& world, const std::string& path) {
    if (path.empty()) return NullEntity;

    // Split path by /
    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;
    while (std::getline(iss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }

    if (parts.empty()) return NullEntity;

    // Find root with first name
    auto roots = get_root_entities(world);
    Entity current = NullEntity;

    for (Entity root : roots) {
        if (world.has<EntityInfo>(root) && world.get<EntityInfo>(root).name == parts[0]) {
            current = root;
            break;
        }
    }

    if (current == NullEntity) return NullEntity;

    // Navigate down the path
    for (size_t i = 1; i < parts.size(); ++i) {
        if (!world.has<Hierarchy>(current)) return NullEntity;

        const auto& hier = world.get<Hierarchy>(current);
        Entity child = hier.first_child;
        Entity found = NullEntity;

        while (child != NullEntity) {
            if (world.has<EntityInfo>(child) && world.get<EntityInfo>(child).name == parts[i]) {
                found = child;
                break;
            }

            if (world.has<Hierarchy>(child)) {
                child = world.get<Hierarchy>(child).next_sibling;
            } else {
                break;
            }
        }

        if (found == NullEntity) return NullEntity;
        current = found;
    }

    return current;
}

size_t count_entities(World& world) {
    size_t count = 0;
    world.view<EntityInfo>().each([&](Entity, const EntityInfo&) {
        ++count;
    });
    return count;
}

} // namespace scene_utils

// Custom component serialization/deserialization using reflection
bool SceneSerializer::serialize_custom_component(World& world, Entity entity, const std::string& type_name, std::string& out_json) const {
    using namespace engine::reflect;
    
    auto& registry = TypeRegistry::instance();
    auto* type_info = registry.get_type_info(type_name);
    
    if (!type_info || !type_info->is_component) {
        return false;
    }
    
    // Get component from entity
    auto comp_any = registry.get_component_any(world.registry(), entity, type_name);
    if (!comp_any) {
        return false;
    }
    
    // Serialize properties to JSON
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\n";
    
    for (size_t i = 0; i < type_info->properties.size(); ++i) {
        const auto& prop = type_info->properties[i];
        if (!prop.getter) continue;
        
        auto value = prop.getter(comp_any);
        if (!value) continue;
        
        ss << "  \"" << prop.name << "\": ";
        
        // Serialize based on property type
        auto type_id = value.type().id();
        if (type_id == entt::type_hash<bool>::value()) {
            ss << (value.cast<bool>() ? "true" : "false");
        } else if (type_id == entt::type_hash<int32_t>::value()) {
            ss << value.cast<int32_t>();
        } else if (type_id == entt::type_hash<uint32_t>::value()) {
            ss << value.cast<uint32_t>();
        } else if (type_id == entt::type_hash<int64_t>::value()) {
            ss << value.cast<int64_t>();
        } else if (type_id == entt::type_hash<uint64_t>::value()) {
            ss << value.cast<uint64_t>();
        } else if (type_id == entt::type_hash<float>::value()) {
            ss << value.cast<float>();
        } else if (type_id == entt::type_hash<double>::value()) {
            ss << value.cast<double>();
        } else if (type_id == entt::type_hash<std::string>::value()) {
            ss << "\"" << value.cast<std::string>() << "\"";
        } else if (type_id == entt::type_hash<Vec3>::value()) {
            ss << to_json(value.cast<Vec3>());
        } else if (type_id == entt::type_hash<Vec4>::value()) {
            ss << to_json(value.cast<Vec4>());
        } else if (type_id == entt::type_hash<Quat>::value()) {
            ss << to_json(value.cast<Quat>());
        } else {
            // Unknown type, skip
            continue;
        }
        
        if (i < type_info->properties.size() - 1) {
            ss << ",";
        }
        ss << "\n";
    }
    
    ss << "}";
    out_json = ss.str();
    return true;
}

bool SceneSerializer::deserialize_custom_component(World& world, Entity entity, const std::string& type_name, const std::string& json) {
    return deserialize_custom_component(world, entity, type_name, json, nullptr);
}

bool SceneSerializer::deserialize_custom_component(World& world, Entity entity, const std::string& type_name, const std::string& json,
                                                    const EntityResolutionContext* entity_ctx) {
    using namespace engine::reflect;

    auto& registry = TypeRegistry::instance();
    auto* type_info = registry.get_type_info(type_name);

    if (!type_info || !type_info->is_component) {
        return false;
    }

    // Create component on entity
    if (!registry.add_component_any(world.registry(), entity, type_name)) {
        return false;
    }

    // Get component handle
    auto comp_any = registry.get_component_any(world.registry(), entity, type_name);
    if (!comp_any) {
        return false;
    }

    // Parse and set each property
    for (const auto& prop : type_info->properties) {
        if (!prop.setter) continue;

        // Build regex to extract property value from JSON
        std::string pattern = "\"" + prop.name + "\"\\s*:\\s*";
        std::regex prop_re(pattern + R"(([^,}\n]+))");
        std::smatch match;

        if (!std::regex_search(json, match, prop_re)) {
            continue;
        }

        std::string value_str = match[1].str();

        // Trim whitespace
        value_str.erase(0, value_str.find_first_not_of(" \t\r\n"));
        value_str.erase(value_str.find_last_not_of(" \t\r\n") + 1);

        // Parse based on property type
        auto type_id = prop.type.id();
        entt::meta_any value;

        if (type_id == entt::type_hash<bool>::value()) {
            value = entt::meta_any{value_str == "true"};
        } else if (type_id == entt::type_hash<int32_t>::value()) {
            value = entt::meta_any{static_cast<int32_t>(std::stoi(value_str))};
        } else if (type_id == entt::type_hash<uint32_t>::value()) {
            value = entt::meta_any{static_cast<uint32_t>(std::stoul(value_str))};
        } else if (type_id == entt::type_hash<int64_t>::value()) {
            value = entt::meta_any{static_cast<int64_t>(std::stoll(value_str))};
        } else if (type_id == entt::type_hash<uint64_t>::value()) {
            value = entt::meta_any{static_cast<uint64_t>(std::stoull(value_str))};
        } else if (type_id == entt::type_hash<float>::value()) {
            value = entt::meta_any{safe_stof(value_str)};
        } else if (type_id == entt::type_hash<double>::value()) {
            value = entt::meta_any{std::stod(value_str)};
        } else if (type_id == entt::type_hash<std::string>::value()) {
            // Remove quotes
            if (value_str.front() == '"' && value_str.back() == '"') {
                value_str = value_str.substr(1, value_str.length() - 2);
            }
            value = entt::meta_any{value_str};
        } else if (type_id == entt::type_hash<Vec3>::value()) {
            value = entt::meta_any{parse_vec3(value_str)};
        } else if (type_id == entt::type_hash<Vec4>::value()) {
            value = entt::meta_any{parse_vec4(value_str)};
        } else if (type_id == entt::type_hash<Quat>::value()) {
            value = entt::meta_any{parse_quat(value_str)};
        } else if (type_id == entt::type_hash<entt::entity>::value() && prop.meta.is_entity_ref) {
            // Handle entity references - parse UUID and resolve to entity
            uint64_t uuid = 0;
            try {
                uuid = std::stoull(value_str);
            } catch (...) {
                uuid = 0;
            }

            entt::entity resolved_entity = entt::null;
            if (uuid != 0 && entity_ctx && entity_ctx->can_deserialize()) {
                resolved_entity = entity_ctx->uuid_to_entity(uuid);
                if (resolved_entity == entt::null) {
                    log(LogLevel::Warn, "Entity reference UUID {} not found for property '{}'", uuid, prop.name);
                }
            }
            value = entt::meta_any{resolved_entity};
        } else {
            // Unknown type, skip
            continue;
        }

        if (value) {
            prop.setter(comp_any, value);
        }
    }

    return true;
}

// Asset handle serialization helpers
std::string SceneSerializer::serialize_asset_handle(const MeshHandle& handle, const char* asset_type) const {
    if (!m_asset_resolver) {
        // No resolver, just serialize raw ID
        return std::to_string(handle.id);
    }
    
    // Resolve asset to get path and serialize both ID and path
    AssetReference ref = m_asset_resolver(handle.id);
    std::ostringstream ss;
    ss << "{\"id\": " << handle.id;
    if (!ref.path.empty()) {
        ss << ", \"path\": \"" << ref.path << "\"";
    }
    ss << "}";
    return ss.str();
}

std::string SceneSerializer::serialize_asset_handle(const MaterialHandle& handle, const char* asset_type) const {
    if (!m_asset_resolver) {
        return std::to_string(handle.id);
    }
    
    AssetReference ref = m_asset_resolver(handle.id);
    std::ostringstream ss;
    ss << "{\"id\": " << handle.id;
    if (!ref.path.empty()) {
        ss << ", \"path\": \"" << ref.path << "\"";
    }
    ss << "}";
    return ss.str();
}

std::string SceneSerializer::serialize_asset_handle(const TextureHandle& handle, const char* asset_type) const {
    if (!m_asset_resolver) {
        return std::to_string(handle.id);
    }
    
    AssetReference ref = m_asset_resolver(handle.id);
    std::ostringstream ss;
    ss << "{\"id\": " << handle.id;
    if (!ref.path.empty()) {
        ss << ", \"path\": \"" << ref.path << "\"";
    }
    ss << "}";
    return ss.str();
}

template<typename HandleType>
HandleType SceneSerializer::deserialize_asset_handle(const std::string& json, const char* asset_type) const {
    HandleType handle;
    
    // Check if JSON is an object (contains path) or just a number
    if (json.find('{') != std::string::npos) {
        // Parse object with ID and optional path
        std::regex id_re(R"("id"\s*:\s*(\d+))");
        // Use standard string escaping to avoid potential raw string literal issues across compilers
        std::regex path_re("\"path\"\\s*:\\s*\"([^\"]+)\"");
        std::smatch match;
        
        uint32_t id = UINT32_MAX;
        std::string path;
        
        if (std::regex_search(json, match, id_re)) {
            id = safe_stoul(match[1].str());
        }
        if (std::regex_search(json, match, path_re)) {
            path = match[1].str();
        }
        
        // If we have an asset loader and a path, try to reload/validate
        if (m_asset_loader && !path.empty()) {
            AssetReference ref;
            ref.path = path;
            ref.type = asset_type;
            
            uint32_t loaded_id = m_asset_loader(ref);
            if (loaded_id != UINT32_MAX) {
                // Successfully loaded from path
                if (loaded_id != id && id != UINT32_MAX) {
                    log(LogLevel::Warn, "Asset ID mismatch for '{}': stored={}, loaded={}. Using loaded ID.",
                        path, id, loaded_id);
                }
                handle.id = loaded_id;
                return handle;
            }
        }
        
        // Fall back to stored ID
        handle.id = id;
    } else {
        // Just a raw number
        handle.id = safe_stoul(json);
    }
    
    return handle;
}

// Template instantiations for the handle types
template MeshHandle SceneSerializer::deserialize_asset_handle<MeshHandle>(const std::string&, const char*) const;
template MaterialHandle SceneSerializer::deserialize_asset_handle<MaterialHandle>(const std::string&, const char*) const;
template TextureHandle SceneSerializer::deserialize_asset_handle<TextureHandle>(const std::string&, const char*) const;

} // namespace engine::scene
