#include <engine/vegetation/foliage.hpp>
#include <engine/vegetation/grass.hpp>
#include <algorithm>
#include <random>
#include <cmath>
#include <fstream>

namespace engine::vegetation {

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

void FoliageSystem::init(const AABB& bounds, const FoliageSettings& settings) {
    if (m_initialized) shutdown();

    m_bounds = bounds;
    m_settings = settings;
    m_initialized = true;
}

void FoliageSystem::shutdown() {
    if (!m_initialized) return;

    m_types.clear();
    m_type_order.clear();
    m_instances.clear();
    m_chunks.clear();

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

void FoliageSystem::render_instances(uint16_t view_id, bool shadow_pass) {
    // Would render using bgfx here
    // Group by type, then by LOD for efficient batching
}

void FoliageSystem::render_billboards(uint16_t view_id) {
    // Would render billboards using bgfx here
}

// VegetationManager implementation

void VegetationManager::init(const AABB& terrain_bounds) {
    if (m_initialized) shutdown();

    m_bounds = terrain_bounds;
    m_grass.init(terrain_bounds);
    m_foliage.init(terrain_bounds);
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
