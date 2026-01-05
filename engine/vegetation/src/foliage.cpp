#include <engine/vegetation/foliage.hpp>
#include <engine/vegetation/grass.hpp>
#include <engine/render/instancing.hpp>
#include <engine/render/billboard.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <random>
#include <cmath>
#include <fstream>

namespace engine::vegetation {

using namespace engine::core;
using namespace engine::render;

// Global instances
static FoliageSystem* s_foliage_system = nullptr;
static VegetationManager* s_vegetation_manager = nullptr;

FoliageSystem& get_foliage_system() {
    if (!s_foliage_system) {
        static FoliageSystem instance;
        s_foliage_system = &instance;
    }
    return *s_foliage_system;
}

VegetationManager& get_vegetation_manager() {
    if (!s_vegetation_manager) {
        static VegetationManager instance;
        s_vegetation_manager = &instance;
    }
    return *s_vegetation_manager;
}

VegetationManager& VegetationManager::instance() {
    return get_vegetation_manager();
}

// FoliageSystem implementation

FoliageSystem::~FoliageSystem() {
    shutdown();
}

void FoliageSystem::init(const AABB& bounds, const FoliageSettings& settings,
                          render::IRenderer* renderer) {
    if (m_initialized) shutdown();

    m_bounds = bounds;
    m_settings = settings;
    m_renderer = renderer;

    create_gpu_resources();

    m_initialized = true;
}

void FoliageSystem::shutdown() {
    if (!m_initialized) return;

    destroy_gpu_resources();

    m_types.clear();
    m_type_order.clear();
    m_instances.clear();
    m_chunks.clear();
    m_renderer = nullptr;

    m_initialized = false;
}

void FoliageSystem::set_settings(const FoliageSettings& settings) {
    m_settings = settings;
}

void FoliageSystem::register_type(const FoliageType& type) {
    if (m_types.find(type.id) == m_types.end()) {
        m_type_order.push_back(type.id);
    }
    m_types[type.id] = type;
    m_stats.total_types = static_cast<uint32_t>(m_types.size());
}

void FoliageSystem::unregister_type(const std::string& id) {
    m_types.erase(id);
    m_type_order.erase(
        std::remove(m_type_order.begin(), m_type_order.end(), id),
        m_type_order.end()
    );
    m_stats.total_types = static_cast<uint32_t>(m_types.size());
}

const FoliageType* FoliageSystem::get_type(const std::string& id) const {
    auto it = m_types.find(id);
    return it != m_types.end() ? &it->second : nullptr;
}

std::vector<std::string> FoliageSystem::get_all_type_ids() const {
    return m_type_order;
}

uint32_t FoliageSystem::add_instance(const std::string& type_id, const Vec3& position,
                                       const Quat& rotation, float scale) {
    auto it = m_types.find(type_id);
    if (it == m_types.end()) return UINT32_MAX;

    // Find type index
    uint32_t type_index = 0;
    for (size_t i = 0; i < m_type_order.size(); ++i) {
        if (m_type_order[i] == type_id) {
            type_index = static_cast<uint32_t>(i);
            break;
        }
    }

    FoliageInstance instance;
    instance.position = position;
    instance.rotation = rotation;
    instance.scale = scale;
    instance.type_index = type_index;

    std::random_device rd;
    instance.random_seed = rd();

    uint32_t index = static_cast<uint32_t>(m_instances.size());
    m_instances.push_back(instance);

    m_stats.total_instances = static_cast<uint32_t>(m_instances.size());

    rebuild_chunks();
    return index;
}

void FoliageSystem::remove_instance(uint32_t index) {
    if (index >= m_instances.size()) return;

    m_instances.erase(m_instances.begin() + index);
    m_stats.total_instances = static_cast<uint32_t>(m_instances.size());

    rebuild_chunks();
}

void FoliageSystem::clear_instances() {
    m_instances.clear();
    m_chunks.clear();
    m_stats.total_instances = 0;
}

void FoliageSystem::add_instances(const std::string& type_id, const std::vector<Vec3>& positions) {
    for (const auto& pos : positions) {
        add_instance(type_id, pos);
    }
}

void FoliageSystem::generate_from_rules(const std::vector<FoliagePlacementRule>& rules,
                                          std::function<float(float x, float z)> height_func,
                                          std::function<Vec3(float x, float z)> normal_func) {
    clear_instances();

    for (const auto& rule : rules) {
        generate_in_region(m_bounds, rule, height_func, normal_func);
    }

    rebuild_chunks();
}

void FoliageSystem::generate_in_region(const AABB& region, const FoliagePlacementRule& rule,
                                         std::function<float(float x, float z)> height_func,
                                         std::function<Vec3(float x, float z)> normal_func) {
    const FoliageType* type = get_type(rule.type_id);
    if (!type) return;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> pos_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> angle_dist(0.0f, 360.0f);

    float spacing = 1.0f / std::sqrt(rule.density);

    for (float z = region.min.z; z < region.max.z; z += spacing) {
        for (float x = region.min.x; x < region.max.x; x += spacing) {
            // Jitter position
            float jx = x + (pos_dist(gen) - 0.5f) * spacing;
            float jz = z + (pos_dist(gen) - 0.5f) * spacing;

            // Clamp to region
            jx = std::clamp(jx, region.min.x, region.max.x);
            jz = std::clamp(jz, region.min.z, region.max.z);

            // Get height and normal
            float y = height_func ? height_func(jx, jz) : 0.0f;
            Vec3 normal = normal_func ? normal_func(jx, jz) : Vec3(0.0f, 1.0f, 0.0f);

            // Height check
            if (y < rule.min_height || y > rule.max_height) continue;

            // Slope check
            float slope = std::acos(normal.y) * 57.2958f;  // to degrees
            if (slope < rule.min_slope || slope > rule.max_slope) continue;

            // Noise-based density
            if (rule.noise_scale > 0.0f) {
                float noise = std::sin(jx * rule.noise_scale) * std::sin(jz * rule.noise_scale);
                noise = (noise + 1.0f) * 0.5f;
                if (noise < rule.noise_threshold) continue;
            }

            // Random rejection for density variation
            if (pos_dist(gen) > rule.density / (1.0f / (spacing * spacing))) continue;

            // Exclusion zones
            bool excluded = false;
            Vec3 pos(jx, y, jz);
            for (const auto& zone : rule.exclusion_zones) {
                if (zone.contains(pos)) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) continue;

            // Custom filter
            if (rule.custom_filter && !rule.custom_filter(pos, normal)) continue;

            // Calculate rotation
            Quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            if (type->random_rotation) {
                float angle = type->min_rotation + pos_dist(gen) * (type->max_rotation - type->min_rotation);
                rotation = glm::angleAxis(angle * 0.0174533f, Vec3(0.0f, 1.0f, 0.0f));
            }

            // Align to terrain
            if (type->align_to_terrain && normal.y < 0.99f) {
                Vec3 up(0.0f, 1.0f, 0.0f);
                Vec3 axis = cross(up, normal);
                if (length(axis) > 0.001f) {
                    float angle = std::acos(dot(up, normal));
                    Quat align = glm::angleAxis(angle, normalize(axis));
                    rotation = align * rotation;
                }
            }

            // Calculate scale
            float scale = type->min_scale + pos_dist(gen) * (type->max_scale - type->min_scale);

            // Apply terrain offset
            y += type->terrain_offset;

            // Check instance limit
            if (m_instances.size() >= m_settings.max_instances) return;

            add_instance(rule.type_id, Vec3(jx, y, jz), rotation, scale);

            // Clustering
            if (rule.enable_clustering && pos_dist(gen) < 0.3f) {
                for (uint32_t c = 0; c < rule.cluster_count; ++c) {
                    float cx = jx + (pos_dist(gen) - 0.5f) * rule.cluster_radius * 2.0f;
                    float cz = jz + (pos_dist(gen) - 0.5f) * rule.cluster_radius * 2.0f;
                    float cy = height_func ? height_func(cx, cz) : 0.0f;

                    if (m_instances.size() >= m_settings.max_instances) return;

                    float cluster_scale = scale * (0.7f + pos_dist(gen) * 0.6f);
                    float cluster_angle = pos_dist(gen) * 360.0f;
                    Quat cluster_rot = glm::angleAxis(cluster_angle * 0.0174533f, Vec3(0.0f, 1.0f, 0.0f));

                    add_instance(rule.type_id, Vec3(cx, cy + type->terrain_offset, cz), cluster_rot, cluster_scale);
                }
            }
        }
    }
}

void FoliageSystem::update(float dt, const Vec3& camera_position, const Frustum& frustum) {
    if (!m_initialized) return;

    update_wind(dt);

    // Check if camera moved enough to update LODs
    float dist_moved = length(camera_position - m_last_camera_pos);
    if (dist_moved > m_settings.update_distance) {
        update_lods(camera_position);
        m_last_camera_pos = camera_position;
    }

    update_visibility(camera_position, frustum);
}

void FoliageSystem::render(uint16_t view_id) {
    if (!m_initialized) return;

    render_instances(view_id, false);

    if (m_settings.enable_billboards) {
        render_billboards(view_id);
    }
}

void FoliageSystem::render_shadows(uint16_t view_id) {
    if (!m_initialized || !m_settings.cast_shadows) return;

    render_instances(view_id, true);
}

const FoliageInstance* FoliageSystem::get_instance(uint32_t index) const {
    return index < m_instances.size() ? &m_instances[index] : nullptr;
}

std::vector<uint32_t> FoliageSystem::get_instances_in_radius(const Vec3& center, float radius) const {
    std::vector<uint32_t> result;
    float radius_sq = radius * radius;

    for (uint32_t i = 0; i < m_instances.size(); ++i) {
        Vec3 diff = m_instances[i].position - center;
        if (dot(diff, diff) <= radius_sq) {
            result.push_back(i);
        }
    }

    return result;
}

std::vector<uint32_t> FoliageSystem::get_instances_in_bounds(const AABB& bounds) const {
    std::vector<uint32_t> result;

    for (uint32_t i = 0; i < m_instances.size(); ++i) {
        if (bounds.contains(m_instances[i].position)) {
            result.push_back(i);
        }
    }

    return result;
}

bool FoliageSystem::raycast(const Vec3& origin, const Vec3& direction, float max_dist,
                             Vec3& out_hit, uint32_t& out_instance) const {
    float closest_dist = max_dist;
    bool hit = false;

    for (uint32_t i = 0; i < m_instances.size(); ++i) {
        const FoliageInstance& inst = m_instances[i];
        if (!inst.visible) continue;

        const FoliageType* type = get_type(m_type_order[inst.type_index]);
        if (!type || !type->has_collision) continue;

        // Simple cylinder collision test
        float radius = type->collision_radius * inst.scale;
        float height = type->collision_height * inst.scale;

        // Ray-cylinder intersection
        Vec3 to_center = inst.position - origin;
        float proj = dot(to_center, direction);

        if (proj < 0.0f || proj > closest_dist) continue;

        Vec3 closest_on_ray = origin + direction * proj;
        Vec3 diff = closest_on_ray - inst.position;
        diff.y = 0.0f;

        if (length(diff) <= radius) {
            float hit_y = origin.y + direction.y * proj;
            if (hit_y >= inst.position.y && hit_y <= inst.position.y + height) {
                if (proj < closest_dist) {
                    closest_dist = proj;
                    out_hit = closest_on_ray;
                    out_instance = i;
                    hit = true;
                }
            }
        }
    }

    return hit;
}

bool FoliageSystem::save_to_file(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Write instance count
    uint32_t count = static_cast<uint32_t>(m_instances.size());
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    // Write instances
    for (const auto& inst : m_instances) {
        file.write(reinterpret_cast<const char*>(&inst.position), sizeof(Vec3));
        file.write(reinterpret_cast<const char*>(&inst.rotation), sizeof(Quat));
        file.write(reinterpret_cast<const char*>(&inst.scale), sizeof(float));
        file.write(reinterpret_cast<const char*>(&inst.type_index), sizeof(uint32_t));
    }

    return true;
}

bool FoliageSystem::load_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    clear_instances();

    uint32_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    m_instances.resize(count);

    for (auto& inst : m_instances) {
        file.read(reinterpret_cast<char*>(&inst.position), sizeof(Vec3));
        file.read(reinterpret_cast<char*>(&inst.rotation), sizeof(Quat));
        file.read(reinterpret_cast<char*>(&inst.scale), sizeof(float));
        file.read(reinterpret_cast<char*>(&inst.type_index), sizeof(uint32_t));
    }

    m_stats.total_instances = count;
    rebuild_chunks();

    return true;
}

void FoliageSystem::rebuild_chunks() {
    m_chunks.clear();

    if (m_instances.empty()) return;

    float chunk_size = static_cast<float>(m_settings.chunk_size);
    float terrain_width = m_bounds.max.x - m_bounds.min.x;
    float terrain_depth = m_bounds.max.z - m_bounds.min.z;

    uint32_t chunks_x = static_cast<uint32_t>(std::ceil(terrain_width / chunk_size));
    uint32_t chunks_z = static_cast<uint32_t>(std::ceil(terrain_depth / chunk_size));

    m_chunks.resize(chunks_x * chunks_z);

    // Initialize chunk bounds
    for (uint32_t z = 0; z < chunks_z; ++z) {
        for (uint32_t x = 0; x < chunks_x; ++x) {
            uint32_t idx = z * chunks_x + x;
            m_chunks[idx].bounds.min = Vec3(
                m_bounds.min.x + x * chunk_size,
                m_bounds.min.y,
                m_bounds.min.z + z * chunk_size
            );
            m_chunks[idx].bounds.max = Vec3(
                m_bounds.min.x + (x + 1) * chunk_size,
                m_bounds.max.y,
                m_bounds.min.z + (z + 1) * chunk_size
            );
        }
    }

    // Assign instances to chunks
    for (uint32_t i = 0; i < m_instances.size(); ++i) {
        const Vec3& pos = m_instances[i].position;

        uint32_t cx = static_cast<uint32_t>((pos.x - m_bounds.min.x) / chunk_size);
        uint32_t cz = static_cast<uint32_t>((pos.z - m_bounds.min.z) / chunk_size);

        cx = std::min(cx, chunks_x - 1);
        cz = std::min(cz, chunks_z - 1);

        m_chunks[cz * chunks_x + cx].instance_indices.push_back(i);
    }
}

void FoliageSystem::update_lods(const Vec3& camera_position) {
    for (auto& inst : m_instances) {
        float dist = length(inst.position - camera_position);

        const FoliageType* type = get_type(m_type_order[inst.type_index]);
        if (!type) continue;

        // Determine LOD level
        uint32_t lod = 0;
        for (size_t i = 0; i < type->lods.size(); ++i) {
            if (dist > type->lods[i].screen_size) {
                lod = static_cast<uint32_t>(i);
            }
        }
        inst.current_lod = lod;

        // Check for billboard
        inst.use_billboard = m_settings.enable_billboards &&
                             type->use_billboard &&
                             dist > type->billboard.start_distance;

        // Calculate LOD blend factor
        if (lod < type->lods.size() - 1) {
            float lod_start = type->lods[lod].screen_size;
            float lod_end = type->lods[lod + 1].screen_size;
            float trans_width = type->lods[lod].transition_width * (lod_end - lod_start);
            float trans_start = lod_end - trans_width;

            if (dist > trans_start) {
                inst.lod_blend = (dist - trans_start) / trans_width;
            } else {
                inst.lod_blend = 0.0f;
            }
        }
    }
}

void FoliageSystem::update_visibility(const Vec3& camera_position, const Frustum& frustum) {
    m_stats.visible_instances = 0;
    m_stats.billboard_instances = 0;
    m_stats.visible_chunks = 0;

    for (auto& chunk : m_chunks) {
        chunk.distance_to_camera = length((chunk.bounds.min + chunk.bounds.max) * 0.5f - camera_position);
        chunk.visible = frustum.contains_aabb(chunk.bounds);

        if (chunk.visible) {
            m_stats.visible_chunks++;

            for (uint32_t idx : chunk.instance_indices) {
                FoliageInstance& inst = m_instances[idx];

                float dist = length(inst.position - camera_position);
                const FoliageType* type = get_type(m_type_order[inst.type_index]);

                if (type && dist <= type->cull_distance) {
                    inst.visible = true;
                    m_stats.visible_instances++;

                    if (inst.use_billboard) {
                        m_stats.billboard_instances++;
                    }
                } else {
                    inst.visible = false;
                }
            }
        }
    }
}

void FoliageSystem::update_wind(float dt) {
    m_wind_time += dt * m_settings.wind_speed;
}

// Shader path helper
static std::string get_foliage_shader_path() {
#if defined(_WIN32)
    return "shaders/dx11/";
#elif defined(__APPLE__)
    return "shaders/metal/";
#else
    return "shaders/glsl/";
#endif
}

// Load shader from file
static bgfx::ShaderHandle load_foliage_shader(const std::string& name) {
    std::string path = get_foliage_shader_path() + name + ".bin";
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        log(LogLevel::Warn, ("Foliage shader not found: " + path).c_str());
        return BGFX_INVALID_HANDLE;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
    file.read(reinterpret_cast<char*>(mem->data), size);
    mem->data[size] = '\0';

    return bgfx::createShader(mem);
}

void FoliageSystem::create_gpu_resources() {
    // Load main foliage shaders
    bgfx::ShaderHandle vs = load_foliage_shader("vs_foliage");
    bgfx::ShaderHandle fs = load_foliage_shader("fs_foliage");

    if (bgfx::isValid(vs) && bgfx::isValid(fs)) {
        m_foliage_program = bgfx::createProgram(vs, fs, true);
    } else {
        log(LogLevel::Warn, "Failed to load foliage shaders, foliage will not render");
        if (bgfx::isValid(vs)) bgfx::destroy(vs);
        if (bgfx::isValid(fs)) bgfx::destroy(fs);
    }

    // Load shadow shaders
    bgfx::ShaderHandle shadow_vs = load_foliage_shader("vs_foliage_shadow");
    bgfx::ShaderHandle shadow_fs = load_foliage_shader("fs_foliage_shadow");

    if (bgfx::isValid(shadow_vs) && bgfx::isValid(shadow_fs)) {
        m_shadow_program = bgfx::createProgram(shadow_vs, shadow_fs, true);
    } else {
        log(LogLevel::Debug, "Foliage shadow shaders not found, shadows will be disabled");
        if (bgfx::isValid(shadow_vs)) bgfx::destroy(shadow_vs);
        if (bgfx::isValid(shadow_fs)) bgfx::destroy(shadow_fs);
    }

    // Create uniforms
    m_u_foliage_wind = bgfx::createUniform("u_foliageWind", bgfx::UniformType::Vec4);
    m_u_foliage_params = bgfx::createUniform("u_foliageParams", bgfx::UniformType::Vec4);
    m_s_albedo = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);
    m_s_normal = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);
}

void FoliageSystem::destroy_gpu_resources() {
    if (bgfx::isValid(m_foliage_program)) {
        bgfx::destroy(m_foliage_program);
        m_foliage_program = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_shadow_program)) {
        bgfx::destroy(m_shadow_program);
        m_shadow_program = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_u_foliage_wind)) {
        bgfx::destroy(m_u_foliage_wind);
        m_u_foliage_wind = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_u_foliage_params)) {
        bgfx::destroy(m_u_foliage_params);
        m_u_foliage_params = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_s_albedo)) {
        bgfx::destroy(m_s_albedo);
        m_s_albedo = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_s_normal)) {
        bgfx::destroy(m_s_normal);
        m_s_normal = BGFX_INVALID_HANDLE;
    }
}

void FoliageSystem::render_instances(uint16_t view_id, bool shadow_pass) {
    if (m_instances.empty()) return;

    // Check if we have the required resources
    bgfx::ProgramHandle program = shadow_pass ? m_shadow_program : m_foliage_program;
    if (!bgfx::isValid(program)) {
        return;  // Shaders not loaded
    }

    // Group visible instances by type and LOD for batching
    // Map: type_index -> lod -> list of instances
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::vector<const FoliageInstance*>>> batches;

    for (const auto& inst : m_instances) {
        if (!inst.visible || inst.use_billboard) continue;

        batches[inst.type_index][inst.current_lod].push_back(&inst);
    }

    // Set wind uniform (shared across all batches)
    if (!shadow_pass && bgfx::isValid(m_u_foliage_wind)) {
        Vec4 wind_data(
            m_settings.wind_direction.x,
            m_settings.wind_direction.y,
            m_wind_time,
            m_settings.wind_strength
        );
        bgfx::setUniform(m_u_foliage_wind, &wind_data);
    }

    // Set foliage params
    if (bgfx::isValid(m_u_foliage_params)) {
        Vec4 params(0.0f, 0.0f, 0.5f, 100.0f);  // alpha_cutoff=0.5, fade_start=100
        bgfx::setUniform(m_u_foliage_params, &params);
    }

    // Render each batch
    for (const auto& [type_idx, lod_map] : batches) {
        if (type_idx >= m_type_order.size()) continue;

        const FoliageType* type = get_type(m_type_order[type_idx]);
        if (!type) continue;

        for (const auto& [lod_level, instances] : lod_map) {
            if (instances.empty()) continue;

            // Get LOD mesh and material
            uint32_t lod_idx = shadow_pass ? std::min(lod_level, m_settings.shadow_lod) : lod_level;
            if (lod_idx >= type->lods.size()) lod_idx = static_cast<uint32_t>(type->lods.size()) - 1;

            const FoliageLOD& lod = type->lods[lod_idx];
            if (lod.mesh_id == UINT32_MAX) continue;

            // Get mesh buffer info from renderer
            MeshBufferInfo mesh_info{0, 0, 0, false};
            if (m_renderer) {
                mesh_info = m_renderer->get_mesh_buffer_info(MeshHandle{lod.mesh_id});
            }

            if (!mesh_info.valid) {
                continue;  // Skip if mesh not found
            }

            // Check instance buffer availability
            uint32_t num_instances = static_cast<uint32_t>(instances.size());
            uint16_t stride = sizeof(Mat4);  // Transform matrix per instance

            if (bgfx::getAvailInstanceDataBuffer(num_instances, stride) < num_instances) {
                continue;  // Not enough buffer space
            }

            // Allocate and fill instance buffer
            bgfx::InstanceDataBuffer idb;
            bgfx::allocInstanceDataBuffer(&idb, num_instances, stride);

            Mat4* transforms = reinterpret_cast<Mat4*>(idb.data);
            for (size_t i = 0; i < instances.size(); ++i) {
                const FoliageInstance* inst = instances[i];

                // Build transform matrix
                Mat4 transform = glm::translate(Mat4(1.0f), inst->position);
                transform = transform * glm::mat4_cast(inst->rotation);
                transform = glm::scale(transform, Vec3(inst->scale));

                transforms[i] = transform;
            }

            // Set vertex and index buffers
            bgfx::VertexBufferHandle vbh = {mesh_info.vertex_buffer};
            bgfx::IndexBufferHandle ibh = {mesh_info.index_buffer};

            bgfx::setVertexBuffer(0, vbh);
            bgfx::setIndexBuffer(ibh);
            bgfx::setInstanceDataBuffer(&idb);

            // Set state
            uint64_t state = BGFX_STATE_DEFAULT | BGFX_STATE_MSAA;
            if (shadow_pass) {
                state = BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CCW;
            }
            bgfx::setState(state);

            // Submit draw call
            bgfx::submit(view_id, program);
        }
    }
}

void FoliageSystem::render_billboards(uint16_t view_id) {
    if (m_instances.empty()) return;

    auto& billboard_renderer = get_billboard_renderer();
    if (!billboard_renderer.is_initialized()) return;

    // Group billboard instances by type
    std::unordered_map<uint32_t, std::vector<const FoliageInstance*>> billboard_batches;

    for (const auto& inst : m_instances) {
        if (!inst.visible || !inst.use_billboard) continue;

        billboard_batches[inst.type_index].push_back(&inst);
    }

    // Create billboard batches
    for (const auto& [type_idx, instances] : billboard_batches) {
        if (type_idx >= m_type_order.size()) continue;

        const FoliageType* type = get_type(m_type_order[type_idx]);
        if (!type || !type->use_billboard) continue;

        const FoliageBillboard& bb = type->billboard;
        if (bb.texture == UINT32_MAX) continue;

        BillboardBatch batch;
        batch.texture = TextureHandle{bb.texture};
        batch.mode = bb.rotate_to_camera ? BillboardMode::ScreenAligned : BillboardMode::AxisAligned;
        batch.depth_test = true;
        batch.depth_write = true;

        batch.instances.reserve(instances.size());

        for (const FoliageInstance* inst : instances) {
            BillboardInstance bi;
            bi.position = inst->position + Vec3(0.0f, bb.size.y * 0.5f * inst->scale, 0.0f);  // Center billboard
            bi.size = bb.size * inst->scale;
            bi.color = Vec4(1.0f);  // No tint
            bi.uv_offset = bb.uv_min;
            bi.uv_scale = bb.uv_max - bb.uv_min;
            bi.rotation = 0.0f;

            batch.instances.push_back(bi);
        }

        billboard_renderer.submit_batch(batch);
    }
}

// VegetationManager implementation

void VegetationManager::init(const AABB& terrain_bounds, render::IRenderer* renderer) {
    if (m_initialized) shutdown();

    m_bounds = terrain_bounds;
    m_grass.init(terrain_bounds);
    m_foliage.init(terrain_bounds, FoliageSettings{}, renderer);
    m_initialized = true;
}

void VegetationManager::shutdown() {
    if (!m_initialized) return;

    m_grass.shutdown();
    m_foliage.shutdown();
    m_initialized = false;
}

void VegetationManager::update(float dt, const Vec3& camera_position, const Frustum& frustum) {
    if (!m_initialized) return;

    m_grass.update(dt, camera_position, frustum);
    m_foliage.update(dt, camera_position, frustum);
}

void VegetationManager::render(uint16_t view_id) {
    if (!m_initialized) return;

    m_grass.render(view_id);
    m_foliage.render(view_id);
}

void VegetationManager::render_shadows(uint16_t view_id) {
    if (!m_initialized) return;

    m_grass.render_shadow(view_id);
    m_foliage.render_shadows(view_id);
}

void VegetationManager::generate_vegetation(
    std::function<float(float x, float z)> height_func,
    std::function<Vec3(float x, float z)> normal_func,
    std::function<float(float x, float z)> grass_density_func,
    const std::vector<FoliagePlacementRule>& foliage_rules) {

    // Generate grass
    m_grass.generate_grass(height_func, grass_density_func, normal_func);

    // Generate foliage
    if (!foliage_rules.empty()) {
        m_foliage.generate_from_rules(foliage_rules, height_func, normal_func);
    }
}

void VegetationManager::clear() {
    m_grass.clear();
    m_foliage.clear_instances();
}

} // namespace engine::vegetation
