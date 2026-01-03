#include <engine/streaming/streaming_volume.hpp>
#include <engine/streaming/scene_streaming.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/render_components.hpp>

namespace engine::streaming {

// ============================================================================
// Portal Graph Implementation
// ============================================================================

static PortalGraph s_portal_graph;

PortalGraph& get_portal_graph() {
    return s_portal_graph;
}

void PortalGraph::add_portal(const std::string& from_cell, const PortalEdge& edge) {
    adjacency[from_cell].push_back(edge);
}

void PortalGraph::clear() {
    adjacency.clear();
}

const std::vector<PortalGraph::PortalEdge>* PortalGraph::get_portals_from(
    const std::string& cell) const {
    auto it = adjacency.find(cell);
    return it != adjacency.end() ? &it->second : nullptr;
}

// ============================================================================
// Helper: Point in AABB test
// ============================================================================

static bool point_in_aabb(const Vec3& point, const AABB& box) {
    return point.x >= box.min.x && point.x <= box.max.x &&
           point.y >= box.min.y && point.y <= box.max.y &&
           point.z >= box.min.z && point.z <= box.max.z;
}

// ============================================================================
// Streaming Volume System
// ============================================================================

void streaming_volume_system(scene::World& world, double /*dt*/) {
    using namespace engine::scene;

    auto& volume_manager = get_streaming_volumes();

    // 1. Sync entity transforms to volumes
    auto vol_view = world.view<StreamingVolumeComponent, WorldTransform>();
    for (auto entity : vol_view) {
        auto& vol_comp = vol_view.get<StreamingVolumeComponent>(entity);
        auto& world_tf = vol_view.get<WorldTransform>(entity);

        if (vol_comp.use_inline_volume) {
            vol_comp.inline_volume.position = world_tf.position();
            vol_comp.inline_volume.rotation = world_tf.rotation();
            volume_manager.add_volume(vol_comp.inline_volume);
        } else if (!vol_comp.volume_name.empty()) {
            if (auto* volume = volume_manager.get_volume(vol_comp.volume_name)) {
                volume->position = world_tf.position();
                volume->rotation = world_tf.rotation();
            }
        }
    }

    // 2. Find active camera position for player position
    Vec3 player_position{0.0f};
    auto camera_view = world.view<Camera, WorldTransform>();
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<Camera>(entity);
        if (cam.active) {
            auto& world_tf = camera_view.get<WorldTransform>(entity);
            player_position = world_tf.position();
            break;
        }
    }

    // 3. Update volume manager with player position
    volume_manager.update(player_position);

    // 4. Forward load/unload requests to scene streaming system
    auto& streaming = get_scene_streaming();

    for (const auto& cell : volume_manager.get_cells_to_load()) {
        streaming.request_load(cell, StreamingPriority::High);
    }

    for (const auto& cell : volume_manager.get_cells_to_unload()) {
        streaming.request_unload(cell);
    }

    for (const auto& cell : volume_manager.get_cells_to_preload()) {
        streaming.request_load(cell, StreamingPriority::Background);
    }
}

// ============================================================================
// Streaming Portal System
// ============================================================================

void streaming_portal_system(scene::World& world, double /*dt*/) {
    using namespace engine::scene;

    auto& graph = get_portal_graph();
    auto& streaming = get_scene_streaming();

    // 1. Rebuild portal graph from entities
    graph.clear();

    auto portal_view = world.view<StreamingPortalComponent, WorldTransform>();
    for (auto entity : portal_view) {
        auto& portal = portal_view.get<StreamingPortalComponent>(entity);
        auto& world_tf = portal_view.get<WorldTransform>(entity);

        Vec3 pos = world_tf.position();
        Vec3 normal = world_tf.rotation() * portal.normal;

        PortalGraph::PortalEdge edge;
        edge.portal_center = pos;
        edge.portal_normal = normal;
        edge.width = portal.width;
        edge.height = portal.height;

        // Cell A -> Cell B
        edge.target_cell = portal.cell_b;
        graph.add_portal(portal.cell_a, edge);

        // Cell B -> Cell A (if bidirectional)
        if (portal.bidirectional) {
            edge.target_cell = portal.cell_a;
            edge.portal_normal = -normal;
            graph.add_portal(portal.cell_b, edge);
        }
    }

    // 2. Get active camera position and forward direction
    Vec3 camera_pos{0.0f};
    Vec3 camera_forward{0.0f, 0.0f, -1.0f};

    auto camera_view = world.view<Camera, WorldTransform>();
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<Camera>(entity);
        if (cam.active) {
            auto& world_tf = camera_view.get<WorldTransform>(entity);
            camera_pos = world_tf.position();
            camera_forward = world_tf.rotation() * Vec3{0.0f, 0.0f, -1.0f};
            break;
        }
    }

    // 3. Determine which loaded cell the camera is in
    std::string current_cell;
    for (const auto& cell_name : streaming.get_loaded_cell_names()) {
        const auto* cell = streaming.get_cell(cell_name);
        if (cell && point_in_aabb(camera_pos, cell->bounds)) {
            current_cell = cell_name;
            break;
        }
    }

    if (current_cell.empty()) {
        return; // Camera not in any loaded cell
    }

    // 4. Check visibility through portals from current cell
    const auto* portals = graph.get_portals_from(current_cell);
    if (!portals) {
        return;
    }

    for (const auto& edge : *portals) {
        // Simple visibility check: is camera looking toward portal?
        Vec3 to_portal = edge.portal_center - camera_pos;
        float distance = glm::length(to_portal);

        if (distance < 0.001f) {
            continue;
        }

        to_portal = to_portal / distance; // normalize

        // Camera facing portal?
        float facing = glm::dot(camera_forward, to_portal);

        // Portal facing camera? (using reversed normal)
        float portal_facing = glm::dot(-edge.portal_normal, to_portal);

        if (facing > 0.0f && portal_facing > 0.0f) {
            // Visibility score based on how directly we're looking
            float score = facing * portal_facing;

            // Boost priority for cells visible through portal
            if (score > 0.5f) {
                streaming.request_load(edge.target_cell, StreamingPriority::High);
            } else if (score > 0.2f) {
                streaming.request_load(edge.target_cell, StreamingPriority::Normal);
            } else {
                streaming.request_load(edge.target_cell, StreamingPriority::Low);
            }
        }
    }
}

} // namespace engine::streaming
