#include <engine/terrain/terrain.hpp>
#include <algorithm>
#include <fstream>

namespace engine::terrain {

// Global instances
static TerrainManager* s_terrain_manager = nullptr;

TerrainManager& get_terrain_manager() {
    if (!s_terrain_manager) {
        static TerrainManager instance;
        s_terrain_manager = &instance;
    }
    return *s_terrain_manager;
}

TerrainManager& TerrainManager::instance() {
    return get_terrain_manager();
}

// Terrain implementation

bool Terrain::create(const TerrainConfig& config) {
    if (m_initialized) destroy();

    m_config = config;

    // Load heightmap
    if (!config.heightmap_path.empty()) {
        if (!m_heightmap.load_raw(config.heightmap_path, 513, 513, HeightmapFormat::R16)) {
            // Try to generate flat terrain as fallback
            m_heightmap.generate_flat(513, 513, 0.5f);
        }
    } else {
        m_heightmap.generate_flat(513, 513, 0.5f);
    }

    // Load splat map
    if (!config.splat_map_path.empty()) {
        m_splat_map.load_from_file(config.splat_map_path);
    }
    if (!m_splat_map.is_valid()) {
        m_splat_map.generate_from_heightmap(m_heightmap, 512, 512);
    }

    // Load hole map
    if (!config.hole_map_path.empty()) {
        m_hole_map.load_from_file(config.hole_map_path);
    }

    // Initialize renderer
    m_renderer.init(m_heightmap, config.scale, config.render_settings);
    m_renderer.set_splat_map(m_splat_map);

    // Generate collision mesh
    if (config.generate_collision) {
        rebuild_collision();
    }

    m_initialized = true;
    return true;
}

bool Terrain::create_flat(const Vec3& position, const Vec3& scale, uint32_t resolution) {
    TerrainConfig config;
    config.position = position;
    config.scale = scale;

    m_heightmap.generate_flat(resolution, resolution, 0.0f);

    return create(config);
}

bool Terrain::create_from_heightmap(const std::string& path, const Vec3& position, const Vec3& scale) {
    TerrainConfig config;
    config.position = position;
    config.scale = scale;
    config.heightmap_path = path;

    return create(config);
}

void Terrain::destroy() {
    if (!m_initialized) return;

    m_renderer.shutdown();
    m_collision_vertices.clear();
    m_collision_indices.clear();
    m_dirty_regions.clear();

    m_physics_body = UINT32_MAX;
    m_initialized = false;
}

void Terrain::set_position(const Vec3& position) {
    m_config.position = position;
}

AABB Terrain::get_bounds() const {
    AABB bounds;
    bounds.min = m_config.position;
    bounds.max = m_config.position + m_config.scale;
    bounds.min.y = m_config.position.y + m_heightmap.get_min_height() * m_config.scale.y;
    bounds.max.y = m_config.position.y + m_heightmap.get_max_height() * m_config.scale.y;
    return bounds;
}

float Terrain::get_height_at(float world_x, float world_z) const {
    if (!m_initialized) return 0.0f;

    float local_x = world_x - m_config.position.x;
    float local_z = world_z - m_config.position.z;

    return m_config.position.y + m_heightmap.sample_world(local_x, local_z, m_config.scale);
}

Vec3 Terrain::get_normal_at(float world_x, float world_z) const {
    if (!m_initialized) return Vec3(0.0f, 1.0f, 0.0f);

    float local_x = world_x - m_config.position.x;
    float local_z = world_z - m_config.position.z;

    return m_heightmap.calculate_normal_world(local_x, local_z, m_config.scale);
}

bool Terrain::get_height_and_normal(float world_x, float world_z, float& out_height, Vec3& out_normal) const {
    if (!is_point_on_terrain(world_x, world_z)) return false;

    out_height = get_height_at(world_x, world_z);
    out_normal = get_normal_at(world_x, world_z);
    return true;
}

bool Terrain::raycast(const Vec3& origin, const Vec3& direction, float max_dist,
                       Vec3& out_hit, Vec3& out_normal) const {
    if (!m_initialized) return false;

    // Transform to local space
    Vec3 local_origin = origin - m_config.position;

    if (m_renderer.raycast(local_origin, direction, max_dist, out_hit, out_normal)) {
        out_hit = out_hit + m_config.position;
        return true;
    }

    return false;
}

bool Terrain::is_point_on_terrain(float world_x, float world_z) const {
    float local_x = world_x - m_config.position.x;
    float local_z = world_z - m_config.position.z;

    return local_x >= 0.0f && local_x <= m_config.scale.x &&
           local_z >= 0.0f && local_z <= m_config.scale.z;
}

Vec3 Terrain::project_point_to_terrain(const Vec3& point) const {
    float height = get_height_at(point.x, point.z);
    return Vec3(point.x, height, point.z);
}

void Terrain::set_layer(uint32_t index, const TerrainLayer& layer) {
    m_renderer.set_layer(index, layer);
}

const TerrainLayer& Terrain::get_layer(uint32_t index) const {
    return m_renderer.get_layer(index);
}

void Terrain::apply_brush(const Vec3& world_pos, const TerrainBrush& brush, float dt) {
    if (!m_initialized) return;

    Vec2 uv = world_to_uv(world_pos.x, world_pos.z);
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) return;

    float radius_uv = brush.radius / m_config.scale.x;

    uint32_t width = m_heightmap.get_width();
    uint32_t height = m_heightmap.get_height();

    int center_x = static_cast<int>(uv.x * (width - 1));
    int center_y = static_cast<int>(uv.y * (height - 1));
    int radius_pixels = static_cast<int>(radius_uv * (width - 1));

    for (int y = -radius_pixels; y <= radius_pixels; ++y) {
        for (int x = -radius_pixels; x <= radius_pixels; ++x) {
            int px = center_x + x;
            int py = center_y + y;

            if (px < 0 || px >= static_cast<int>(width) ||
                py < 0 || py >= static_cast<int>(height)) continue;

            float dist = std::sqrt(static_cast<float>(x * x + y * y)) / radius_pixels;
            if (dist > 1.0f) continue;

            float falloff_weight = 1.0f - std::pow(dist, 1.0f / brush.falloff);
            float strength = brush.strength * dt * falloff_weight;

            switch (brush.mode) {
                case TerrainBrush::Mode::Raise: {
                    float h = m_heightmap.get_height(px, py);
                    m_heightmap.set_height(px, py, h + strength / m_config.scale.y);
                    break;
                }
                case TerrainBrush::Mode::Lower: {
                    float h = m_heightmap.get_height(px, py);
                    m_heightmap.set_height(px, py, h - strength / m_config.scale.y);
                    break;
                }
                case TerrainBrush::Mode::Flatten: {
                    float h = m_heightmap.get_height(px, py);
                    float target = (brush.target_height - m_config.position.y) / m_config.scale.y;
                    m_heightmap.set_height(px, py, h + (target - h) * strength);
                    break;
                }
                case TerrainBrush::Mode::Smooth: {
                    // Average with neighbors
                    float sum = 0.0f;
                    int count = 0;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            int nx = px + dx;
                            int ny = py + dy;
                            if (nx >= 0 && nx < static_cast<int>(width) &&
                                ny >= 0 && ny < static_cast<int>(height)) {
                                sum += m_heightmap.get_height(nx, ny);
                                count++;
                            }
                        }
                    }
                    float avg = sum / count;
                    float h = m_heightmap.get_height(px, py);
                    m_heightmap.set_height(px, py, h + (avg - h) * strength);
                    break;
                }
                case TerrainBrush::Mode::Noise: {
                    float noise = (std::sin(px * 0.5f) * std::sin(py * 0.7f)) * 0.5f + 0.5f;
                    float h = m_heightmap.get_height(px, py);
                    m_heightmap.set_height(px, py, h + (noise - 0.5f) * strength / m_config.scale.y);
                    break;
                }
                case TerrainBrush::Mode::Paint: {
                    m_splat_map.paint(
                        static_cast<float>(px) / (width - 1),
                        static_cast<float>(py) / (height - 1),
                        brush.paint_channel,
                        strength,
                        radius_uv,
                        brush.falloff
                    );
                    break;
                }
            }
        }
    }

    // Mark region as dirty
    AABB dirty_region;
    dirty_region.min = Vec3(
        world_pos.x - brush.radius,
        0.0f,
        world_pos.z - brush.radius
    );
    dirty_region.max = Vec3(
        world_pos.x + brush.radius,
        m_config.scale.y,
        world_pos.z + brush.radius
    );
    mark_dirty(dirty_region);
}

void Terrain::mark_dirty(const AABB& region) {
    m_dirty_regions.push_back(region);
    m_collision_dirty = true;
}

void Terrain::rebuild_dirty_chunks() {
    // Rebuild affected chunks
    m_dirty_regions.clear();
}

void Terrain::update(float dt, const Vec3& camera_position, const Frustum& frustum) {
    if (!m_initialized) return;

    // Transform camera to local space
    Vec3 local_camera = camera_position - m_config.position;

    m_renderer.update(local_camera, frustum);
}

void Terrain::render(uint16_t view_id) {
    if (!m_initialized) return;
    m_renderer.render(view_id);
}

void Terrain::render_shadow(uint16_t view_id) {
    if (!m_initialized) return;
    m_renderer.render_shadow(view_id);
}

void Terrain::rebuild_collision() {
    uint32_t resolution = m_config.collision_resolution;
    if (resolution == 0) {
        resolution = m_config.render_settings.chunk_resolution;
    }

    TerrainPhysicsGenerator::generate_collision_mesh(
        m_heightmap, m_config.scale,
        m_collision_vertices, m_collision_indices,
        resolution
    );

    // Transform to world space
    for (auto& v : m_collision_vertices) {
        v = v + m_config.position;
    }

    m_collision_dirty = false;

    // Would create physics body here using m_collision_vertices and m_collision_indices
}

bool Terrain::save_to_file(const std::string& directory) const {
    // Save heightmap
    std::string heightmap_path = directory + "/heightmap.raw";
    if (!m_heightmap.save_raw(heightmap_path)) return false;

    // Save splat map
    std::string splat_path = directory + "/splatmap.png";
    m_splat_map.save_to_file(splat_path);

    // Save config
    std::string config_path = directory + "/terrain.json";
    // Would serialize config here

    return true;
}

bool Terrain::load_from_file(const std::string& directory) {
    TerrainConfig config;
    config.heightmap_path = directory + "/heightmap.raw";
    config.splat_map_path = directory + "/splatmap.png";

    // Would deserialize config from terrain.json

    return create(config);
}

Vec2 Terrain::world_to_uv(float world_x, float world_z) const {
    float local_x = world_x - m_config.position.x;
    float local_z = world_z - m_config.position.z;

    return Vec2(
        local_x / m_config.scale.x,
        local_z / m_config.scale.z
    );
}

Vec3 Terrain::uv_to_world(float u, float v, float height) const {
    return Vec3(
        m_config.position.x + u * m_config.scale.x,
        m_config.position.y + height,
        m_config.position.z + v * m_config.scale.z
    );
}

// TerrainManager implementation

uint32_t TerrainManager::create_terrain(const TerrainConfig& config) {
    uint32_t id = m_next_id++;

    auto terrain = std::make_unique<Terrain>();
    if (!terrain->create(config)) {
        return UINT32_MAX;
    }

    m_terrains[id] = std::move(terrain);
    return id;
}

void TerrainManager::destroy_terrain(uint32_t id) {
    auto it = m_terrains.find(id);
    if (it != m_terrains.end()) {
        it->second->destroy();
        m_terrains.erase(it);
    }
}

void TerrainManager::destroy_all() {
    for (auto& [id, terrain] : m_terrains) {
        terrain->destroy();
    }
    m_terrains.clear();
}

Terrain* TerrainManager::get_terrain(uint32_t id) {
    auto it = m_terrains.find(id);
    return it != m_terrains.end() ? it->second.get() : nullptr;
}

const Terrain* TerrainManager::get_terrain(uint32_t id) const {
    auto it = m_terrains.find(id);
    return it != m_terrains.end() ? it->second.get() : nullptr;
}

float TerrainManager::get_height_at(float world_x, float world_z) const {
    for (const auto& [id, terrain] : m_terrains) {
        if (terrain->is_point_on_terrain(world_x, world_z)) {
            return terrain->get_height_at(world_x, world_z);
        }
    }
    return 0.0f;
}

Vec3 TerrainManager::get_normal_at(float world_x, float world_z) const {
    for (const auto& [id, terrain] : m_terrains) {
        if (terrain->is_point_on_terrain(world_x, world_z)) {
            return terrain->get_normal_at(world_x, world_z);
        }
    }
    return Vec3(0.0f, 1.0f, 0.0f);
}

bool TerrainManager::raycast(const Vec3& origin, const Vec3& direction, float max_dist,
                              Vec3& out_hit, Vec3& out_normal, uint32_t* out_terrain_id) const {
    float closest_dist = max_dist;
    bool hit = false;

    for (const auto& [id, terrain] : m_terrains) {
        Vec3 hit_point, hit_normal;
        if (terrain->raycast(origin, direction, closest_dist, hit_point, hit_normal)) {
            float dist = length(hit_point - origin);
            if (dist < closest_dist) {
                closest_dist = dist;
                out_hit = hit_point;
                out_normal = hit_normal;
                if (out_terrain_id) *out_terrain_id = id;
                hit = true;
            }
        }
    }

    return hit;
}

Terrain* TerrainManager::get_terrain_at(float world_x, float world_z) {
    for (auto& [id, terrain] : m_terrains) {
        if (terrain->is_point_on_terrain(world_x, world_z)) {
            return terrain.get();
        }
    }
    return nullptr;
}

void TerrainManager::update(float dt, const Vec3& camera_position, const Frustum& frustum) {
    for (auto& [id, terrain] : m_terrains) {
        terrain->update(dt, camera_position, frustum);
    }
}

void TerrainManager::render(uint16_t view_id) {
    for (auto& [id, terrain] : m_terrains) {
        terrain->render(view_id);
    }
}

void TerrainManager::render_shadows(uint16_t view_id) {
    for (auto& [id, terrain] : m_terrains) {
        terrain->render_shadow(view_id);
    }
}

std::vector<uint32_t> TerrainManager::get_all_terrain_ids() const {
    std::vector<uint32_t> ids;
    ids.reserve(m_terrains.size());
    for (const auto& [id, terrain] : m_terrains) {
        ids.push_back(id);
    }
    return ids;
}

void TerrainManager::for_each_terrain(std::function<void(Terrain&)> func) {
    for (auto& [id, terrain] : m_terrains) {
        func(*terrain);
    }
}

} // namespace engine::terrain
