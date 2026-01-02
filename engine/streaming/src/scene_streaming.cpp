#include <engine/streaming/scene_streaming.hpp>
#include <engine/streaming/streaming_volume.hpp>
#include <algorithm>
#include <chrono>

namespace engine::streaming {

// Global instances
static SceneStreamingSystem* s_scene_streaming = nullptr;
static StreamingVolumeManager* s_volume_manager = nullptr;

static Vec3 closest_point_on_aabb(const AABB& box, const Vec3& point) {
    return glm::clamp(point, box.min, box.max);
}

SceneStreamingSystem& get_scene_streaming() {
    if (!s_scene_streaming) {
        static SceneStreamingSystem instance;
        s_scene_streaming = &instance;
    }
    return *s_scene_streaming;
}

StreamingVolumeManager& get_streaming_volumes() {
    if (!s_volume_manager) {
        static StreamingVolumeManager instance;
        s_volume_manager = &instance;
    }
    return *s_volume_manager;
}

// SceneStreamingSystem implementation

void SceneStreamingSystem::init(const StreamingSettings& settings) {
    if (m_initialized) return;

    m_settings = settings;
    m_initialized = true;
}

void SceneStreamingSystem::shutdown() {
    if (!m_initialized) return;

    // Wait for async loads to complete
    for (auto& task : m_async_loads) {
        if (task.future.valid()) {
            task.future.wait();
        }
    }
    m_async_loads.clear();

    // Unload all cells
    for (auto& [name, cell] : m_cells) {
        if (cell.state == CellState::Loaded || cell.state == CellState::Visible) {
            unload_cell_internal(cell);
        }
    }

    m_cells.clear();
    m_load_times.clear();

    m_initialized = false;
}

void SceneStreamingSystem::register_cell(const StreamingCellData& cell) {
    m_cells[cell.name] = cell;
    m_stats.total_cells = static_cast<uint32_t>(m_cells.size());
}

void SceneStreamingSystem::unregister_cell(const std::string& name) {
    auto it = m_cells.find(name);
    if (it != m_cells.end()) {
        if (it->second.state == CellState::Loaded || it->second.state == CellState::Visible) {
            unload_cell_internal(it->second);
        }
        m_cells.erase(it);
        m_stats.total_cells = static_cast<uint32_t>(m_cells.size());
    }
}

void SceneStreamingSystem::clear_cells() {
    for (auto& [name, cell] : m_cells) {
        if (cell.state == CellState::Loaded || cell.state == CellState::Visible) {
            unload_cell_internal(cell);
        }
    }
    m_cells.clear();
    m_stats.total_cells = 0;
}

StreamingCellData* SceneStreamingSystem::get_cell(const std::string& name) {
    auto it = m_cells.find(name);
    return it != m_cells.end() ? &it->second : nullptr;
}

const StreamingCellData* SceneStreamingSystem::get_cell(const std::string& name) const {
    auto it = m_cells.find(name);
    return it != m_cells.end() ? &it->second : nullptr;
}

std::vector<std::string> SceneStreamingSystem::get_all_cell_names() const {
    std::vector<std::string> names;
    names.reserve(m_cells.size());
    for (const auto& [name, cell] : m_cells) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> SceneStreamingSystem::get_loaded_cell_names() const {
    std::vector<std::string> names;
    for (const auto& [name, cell] : m_cells) {
        if (cell.state == CellState::Loaded || cell.state == CellState::Visible) {
            names.push_back(name);
        }
    }
    return names;
}

std::vector<std::string> SceneStreamingSystem::get_visible_cell_names() const {
    std::vector<std::string> names;
    for (const auto& [name, cell] : m_cells) {
        if (cell.state == CellState::Visible) {
            names.push_back(name);
        }
    }
    return names;
}

void SceneStreamingSystem::request_load(const std::string& cell_name, StreamingPriority priority) {
    auto* cell = get_cell(cell_name);
    if (!cell) return;

    if (cell->state == CellState::Unloaded) {
        StreamingLoadRequest request;
        request.cell_name = cell_name;
        request.priority = priority;
        request.distance = cell->distance_to_player;
        m_load_queue.push(request);
    }
}

void SceneStreamingSystem::request_unload(const std::string& cell_name) {
    auto* cell = get_cell(cell_name);
    if (!cell) return;

    if (cell->state == CellState::Loaded || cell->state == CellState::Visible) {
        m_unload_queue.push_back(cell_name);
    }
}

void SceneStreamingSystem::force_load_sync(const std::string& cell_name) {
    auto* cell = get_cell(cell_name);
    if (!cell) return;

    if (cell->state == CellState::Unloaded) {
        load_cell_internal(*cell);
    }
}

void SceneStreamingSystem::force_unload_sync(const std::string& cell_name) {
    auto* cell = get_cell(cell_name);
    if (!cell) return;

    if (cell->state == CellState::Loaded || cell->state == CellState::Visible) {
        unload_cell_internal(*cell);
    }
}

void SceneStreamingSystem::load_cells_in_radius(const Vec3& center, float radius) {
    for (auto& [name, cell] : m_cells) {
        Vec3 closest = closest_point_on_aabb(cell.bounds, center);
        float dist = length(closest - center);

        if (dist <= radius && cell.state == CellState::Unloaded) {
            request_load(name, StreamingPriority::High);
        }
    }
}

void SceneStreamingSystem::unload_cells_outside_radius(const Vec3& center, float radius) {
    for (auto& [name, cell] : m_cells) {
        Vec3 closest = closest_point_on_aabb(cell.bounds, center);
        float dist = length(closest - center);

        if (dist > radius && (cell.state == CellState::Loaded || cell.state == CellState::Visible)) {
            request_unload(name);
        }
    }
}

void SceneStreamingSystem::preload_cells(const std::vector<std::string>& cell_names) {
    for (const auto& name : cell_names) {
        request_load(name, StreamingPriority::Background);
    }
}

void SceneStreamingSystem::update(float dt, const Vec3& player_position, const Vec3& camera_position) {
    if (!m_initialized) return;

    m_current_time += static_cast<uint64_t>(dt * 1000.0f);
    m_stats.loads_this_frame = 0;
    m_stats.unloads_this_frame = 0;

    // Determine streaming origin
    Vec3 origin = m_settings.use_camera_position ? camera_position : player_position;
    m_streaming_origin = origin;

    // Update cell distances and priorities
    update_cell_distances(origin);
    update_cell_priorities();
    update_cell_lods();

    // Queue cells for loading/unloading based on distance
    for (auto& [name, cell] : m_cells) {
        update_cell_visibility(cell, origin);

        if (cell.state == CellState::Unloaded && cell.distance_to_player < cell.load_distance) {
            request_load(name, cell.priority);
        } else if ((cell.state == CellState::Loaded || cell.state == CellState::Visible) &&
                   cell.distance_to_player > cell.unload_distance) {
            request_unload(name);
        }
    }

    // Process async load completions
    check_async_loads();

    // Process queues
    process_load_queue();
    process_unload_queue();

    // Update stats
    m_stats.loaded_cells = 0;
    m_stats.visible_cells = 0;
    m_stats.loading_cells = 0;
    m_stats.unloading_cells = 0;

    for (const auto& [name, cell] : m_cells) {
        switch (cell.state) {
            case CellState::Loaded: m_stats.loaded_cells++; break;
            case CellState::Visible: m_stats.visible_cells++; m_stats.loaded_cells++; break;
            case CellState::Loading: m_stats.loading_cells++; break;
            case CellState::Unloading: m_stats.unloading_cells++; break;
            default: break;
        }
    }
}

void SceneStreamingSystem::set_streaming_origin(const Vec3& position) {
    m_streaming_origin = position;
    m_settings.use_camera_position = false;
    m_settings.override_position = position;
}

StreamingStats SceneStreamingSystem::get_stats() const {
    return m_stats;
}

bool SceneStreamingSystem::is_cell_loaded(const std::string& name) const {
    const auto* cell = get_cell(name);
    return cell && (cell->state == CellState::Loaded || cell->state == CellState::Visible);
}

bool SceneStreamingSystem::is_cell_visible(const std::string& name) const {
    const auto* cell = get_cell(name);
    return cell && cell->state == CellState::Visible;
}

CellState SceneStreamingSystem::get_cell_state(const std::string& name) const {
    const auto* cell = get_cell(name);
    return cell ? cell->state : CellState::Unloaded;
}

void SceneStreamingSystem::update_cell_distances(const Vec3& origin) {
    for (auto& [name, cell] : m_cells) {
        Vec3 closest = closest_point_on_aabb(cell.bounds, origin);
        cell.distance_to_player = length(closest - origin);
    }
}

void SceneStreamingSystem::update_cell_priorities() {
    for (auto& [name, cell] : m_cells) {
        // Priority based on distance
        if (cell.distance_to_player < cell.load_distance * 0.5f) {
            cell.priority = StreamingPriority::Critical;
        } else if (cell.distance_to_player < cell.load_distance) {
            cell.priority = StreamingPriority::High;
        } else if (cell.distance_to_player < cell.load_distance * 1.5f) {
            cell.priority = StreamingPriority::Normal;
        } else if (cell.distance_to_player < cell.load_distance * 2.0f) {
            cell.priority = StreamingPriority::Low;
        } else {
            cell.priority = StreamingPriority::Background;
        }
    }
}

void SceneStreamingSystem::update_cell_lods() {
    if (!m_settings.use_lod) return;

    for (auto& [name, cell] : m_cells) {
        float lod_dist = cell.load_distance * m_settings.lod_distance_multiplier;
        float bias = m_settings.lod_bias;

        if (cell.distance_to_player < lod_dist * 0.5f + bias) {
            cell.lod = CellLOD::Full;
        } else if (cell.distance_to_player < lod_dist + bias) {
            cell.lod = CellLOD::Reduced;
        } else if (cell.distance_to_player < lod_dist * 1.5f + bias) {
            cell.lod = CellLOD::Proxy;
        } else {
            cell.lod = CellLOD::Hidden;
        }
    }
}

void SceneStreamingSystem::process_load_queue() {
    uint32_t loads_this_frame = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (!m_load_queue.empty() && loads_this_frame < m_settings.max_loads_per_frame) {
        // Check time budget
        auto current_time = std::chrono::high_resolution_clock::now();
        float elapsed_ms = std::chrono::duration<float, std::milli>(current_time - start_time).count();
        if (elapsed_ms > m_settings.load_budget_ms) break;

        // Check concurrent load limit
        if (m_async_loads.size() >= m_settings.max_concurrent_loads) break;

        StreamingLoadRequest request = m_load_queue.top();
        m_load_queue.pop();

        auto* cell = get_cell(request.cell_name);
        if (!cell || cell->state != CellState::Unloaded) continue;

        // Check dependencies
        bool deps_loaded = true;
        for (const auto& dep : cell->dependencies) {
            if (!is_cell_loaded(dep)) {
                deps_loaded = false;
                request_load(dep, StreamingPriority::Critical);
            }
        }
        if (!deps_loaded) {
            // Re-queue this cell
            m_load_queue.push(request);
            continue;
        }

        // Start loading
        cell->state = CellState::Loading;

        if (m_cell_loader) {
            // Use async loading
            AsyncLoadTask task;
            task.cell_name = request.cell_name;

            std::string scene_path = cell->scene_path;
            task.future = std::async(std::launch::async, [this, scene_path, &task]() {
                return m_cell_loader(scene_path, task.loaded_entities);
            });

            m_async_loads.push_back(std::move(task));
        } else {
            // No loader, just mark as loaded
            cell->state = CellState::Loaded;
            m_stats.loaded_cells++;
            if (m_on_loaded) m_on_loaded(cell->name);
        }

        loads_this_frame++;
        m_stats.loads_this_frame++;
    }
}

void SceneStreamingSystem::process_unload_queue() {
    uint32_t unloads_this_frame = 0;

    while (!m_unload_queue.empty() && unloads_this_frame < m_settings.max_unloads_per_frame) {
        std::string cell_name = m_unload_queue.back();
        m_unload_queue.pop_back();

        auto* cell = get_cell(cell_name);
        if (!cell) continue;

        if (cell->state == CellState::Loaded || cell->state == CellState::Visible) {
            unload_cell_internal(*cell);
            unloads_this_frame++;
            m_stats.unloads_this_frame++;
        }
    }
}

void SceneStreamingSystem::check_async_loads() {
    for (auto it = m_async_loads.begin(); it != m_async_loads.end();) {
        if (it->future.valid() &&
            it->future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {

            bool success = it->future.get();
            auto* cell = get_cell(it->cell_name);

            if (cell) {
                if (success) {
                    cell->state = CellState::Loaded;
                    cell->entity_ids = std::move(it->loaded_entities);
                    if (m_on_loaded) m_on_loaded(cell->name);
                } else {
                    cell->state = CellState::Unloaded;
                }
            }

            it = m_async_loads.erase(it);
        } else {
            ++it;
        }
    }
}

bool SceneStreamingSystem::load_cell_internal(StreamingCellData& cell) {
    auto start = std::chrono::high_resolution_clock::now();

    if (m_cell_loader) {
        std::vector<uint32_t> entities;
        bool success = m_cell_loader(cell.scene_path, entities);
        if (success) {
            cell.entity_ids = std::move(entities);
            cell.state = CellState::Loaded;

            auto end = std::chrono::high_resolution_clock::now();
            float load_time = std::chrono::duration<float, std::milli>(end - start).count();
            m_load_times.push_back(load_time);

            // Update average
            float sum = 0.0f;
            for (float t : m_load_times) sum += t;
            m_stats.average_load_time_ms = sum / m_load_times.size();

            if (m_on_loaded) m_on_loaded(cell.name);
            return true;
        }
        return false;
    }

    cell.state = CellState::Loaded;
    if (m_on_loaded) m_on_loaded(cell.name);
    return true;
}

void SceneStreamingSystem::unload_cell_internal(StreamingCellData& cell) {
    cell.state = CellState::Unloading;

    if (m_cell_unloader && !cell.entity_ids.empty()) {
        m_cell_unloader(cell.entity_ids);
    }

    cell.entity_ids.clear();
    cell.state = CellState::Unloaded;

    if (m_on_unloaded) m_on_unloaded(cell.name);
}

void SceneStreamingSystem::update_cell_visibility(StreamingCellData& cell, const Vec3& origin) {
    if (cell.state != CellState::Loaded && cell.state != CellState::Visible) return;

    bool should_be_visible = cell.distance_to_player < cell.load_distance;

    if (should_be_visible && cell.state == CellState::Loaded) {
        cell.state = CellState::Visible;
        cell.last_visible_time = m_current_time;
        if (m_on_visibility_changed) m_on_visibility_changed(cell.name, true);
    } else if (!should_be_visible && cell.state == CellState::Visible) {
        cell.state = CellState::Loaded;
        if (m_on_visibility_changed) m_on_visibility_changed(cell.name, false);
    }
}

void SceneStreamingSystem::debug_draw() const {
    // Implementation would use debug draw system
}

// StreamingVolumeManager implementation

void StreamingVolumeManager::add_volume(const StreamingVolume& volume) {
    // Check if volume with same name exists
    for (auto& v : m_volumes) {
        if (v.name == volume.name) {
            v = volume;
            return;
        }
    }
    m_volumes.push_back(volume);
}

void StreamingVolumeManager::remove_volume(const std::string& name) {
    m_volumes.erase(
        std::remove_if(m_volumes.begin(), m_volumes.end(),
            [&name](const StreamingVolume& v) { return v.name == name; }),
        m_volumes.end()
    );
}

void StreamingVolumeManager::clear_volumes() {
    m_volumes.clear();
    m_active_volumes.clear();
    m_pending_loads.clear();
    m_pending_unloads.clear();
    m_pending_preloads.clear();
}

StreamingVolume* StreamingVolumeManager::get_volume(const std::string& name) {
    for (auto& v : m_volumes) {
        if (v.name == name) return &v;
    }
    return nullptr;
}

const StreamingVolume* StreamingVolumeManager::get_volume(const std::string& name) const {
    for (const auto& v : m_volumes) {
        if (v.name == name) return &v;
    }
    return nullptr;
}

std::vector<std::string> StreamingVolumeManager::get_all_volume_names() const {
    std::vector<std::string> names;
    names.reserve(m_volumes.size());
    for (const auto& v : m_volumes) {
        names.push_back(v.name);
    }
    return names;
}

void StreamingVolumeManager::set_volume_enabled(const std::string& name, bool enabled) {
    auto* vol = get_volume(name);
    if (vol) vol->enabled = enabled;
}

bool StreamingVolumeManager::is_volume_enabled(const std::string& name) const {
    const auto* vol = get_volume(name);
    return vol ? vol->enabled : false;
}

void StreamingVolumeManager::update(const Vec3& player_position, uint32_t player_layer) {
    m_pending_loads.clear();
    m_pending_unloads.clear();
    m_pending_preloads.clear();
    m_blocking = false;

    std::vector<std::string> new_active_volumes;

    for (auto& volume : m_volumes) {
        if (!volume.enabled) continue;
        if (volume.player_only && (volume.activation_layers & player_layer) == 0) continue;

        bool inside = test_point_in_volume(volume, player_position);
        bool was_inside = volume.was_inside;

        // Update fade
        float target_fade = inside ? 1.0f : 0.0f;
        if (volume.fade_distance > 0.0f) {
            float dist = get_signed_distance(volume, player_position);
            if (dist < volume.fade_distance) {
                target_fade = 1.0f - (dist / volume.fade_distance);
            }
        }
        volume.current_fade = target_fade;

        if (inside && !was_inside) {
            // Enter event
            volume.is_active = true;
            new_active_volumes.push_back(volume.name);

            for (const auto& cell : volume.load_cells) {
                m_pending_loads.push_back(cell);
            }
            for (const auto& cell : volume.unload_cells) {
                m_pending_unloads.push_back(cell);
            }
            for (const auto& cell : volume.preload_cells) {
                m_pending_preloads.push_back(cell);
            }

            if (volume.block_until_loaded) {
                m_blocking = true;
            }

            if (m_on_volume_event) {
                m_on_volume_event(volume, VolumeEvent::Enter);
            }

            if (volume.one_shot) {
                volume.enabled = false;
            }
        } else if (!inside && was_inside) {
            // Exit event
            volume.is_active = false;

            if (m_on_volume_event) {
                m_on_volume_event(volume, VolumeEvent::Exit);
            }
        } else if (inside) {
            // Stay event
            new_active_volumes.push_back(volume.name);

            if (m_on_volume_event) {
                m_on_volume_event(volume, VolumeEvent::Stay);
            }
        }

        volume.was_inside = inside;
    }

    m_active_volumes = std::move(new_active_volumes);
}

std::vector<std::string> StreamingVolumeManager::get_volumes_at_point(const Vec3& point) const {
    std::vector<std::string> result;
    for (const auto& volume : m_volumes) {
        if (volume.enabled && test_point_in_volume(volume, point)) {
            result.push_back(volume.name);
        }
    }
    return result;
}

bool StreamingVolumeManager::is_point_in_volume(const std::string& name, const Vec3& point) const {
    const auto* vol = get_volume(name);
    return vol && test_point_in_volume(*vol, point);
}

std::vector<std::string> StreamingVolumeManager::get_cells_to_load() const {
    return m_pending_loads;
}

std::vector<std::string> StreamingVolumeManager::get_cells_to_unload() const {
    return m_pending_unloads;
}

std::vector<std::string> StreamingVolumeManager::get_cells_to_preload() const {
    return m_pending_preloads;
}

bool StreamingVolumeManager::is_blocking_required() const {
    return m_blocking;
}

float StreamingVolumeManager::get_blocking_progress() const {
    return m_blocking_progress;
}

bool StreamingVolumeManager::test_point_in_volume(const StreamingVolume& volume, const Vec3& point) const {
    // Transform point to local space
    Vec3 local = point - volume.position;

    // Apply inverse rotation
    Quat inv_rot = conjugate(volume.rotation);
    local = inv_rot * local;

    // Apply inverse scale
    local = local / volume.scale;

    switch (volume.shape) {
        case VolumeShape::Box:
            return std::abs(local.x) <= volume.box_extents.x &&
                   std::abs(local.y) <= volume.box_extents.y &&
                   std::abs(local.z) <= volume.box_extents.z;

        case VolumeShape::Sphere:
            return length(local) <= volume.sphere_radius;

        case VolumeShape::Capsule: {
            float half_height = volume.capsule_height * 0.5f;
            float y_clamped = std::clamp(local.y, -half_height, half_height);
            Vec3 closest(0.0f, y_clamped, 0.0f);
            return length(local - closest) <= volume.capsule_radius;
        }

        case VolumeShape::Cylinder: {
            float half_height = volume.cylinder_height * 0.5f;
            if (std::abs(local.y) > half_height) return false;
            float dist_xz = std::sqrt(local.x * local.x + local.z * local.z);
            return dist_xz <= volume.cylinder_radius;
        }
    }

    return false;
}

float StreamingVolumeManager::get_signed_distance(const StreamingVolume& volume, const Vec3& point) const {
    Vec3 local = point - volume.position;
    Quat inv_rot = conjugate(volume.rotation);
    local = inv_rot * local;
    local = local / volume.scale;

    switch (volume.shape) {
        case VolumeShape::Box: {
            Vec3 q = Vec3(std::abs(local.x), std::abs(local.y), std::abs(local.z)) - volume.box_extents;
            return length(Vec3(std::max(q.x, 0.0f), std::max(q.y, 0.0f), std::max(q.z, 0.0f))) +
                   std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
        }

        case VolumeShape::Sphere:
            return length(local) - volume.sphere_radius;

        case VolumeShape::Capsule: {
            float half_height = volume.capsule_height * 0.5f;
            float y_clamped = std::clamp(local.y, -half_height, half_height);
            Vec3 closest(0.0f, y_clamped, 0.0f);
            return length(local - closest) - volume.capsule_radius;
        }

        case VolumeShape::Cylinder: {
            float half_height = volume.cylinder_height * 0.5f;
            float dist_xz = std::sqrt(local.x * local.x + local.z * local.z);
            float dist_y = std::abs(local.y) - half_height;
            float dist_r = dist_xz - volume.cylinder_radius;
            return std::max(dist_y, dist_r);
        }
    }

    return 0.0f;
}

void StreamingVolumeManager::debug_draw() const {
    // Implementation would use debug draw system
}

} // namespace engine::streaming
