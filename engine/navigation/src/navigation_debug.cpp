#include <engine/navigation/navigation_debug.hpp>
#include <engine/navigation/navmesh.hpp>
#include <engine/navigation/nav_agent.hpp>
#include <engine/navigation/nav_obstacle.hpp>
#include <engine/navigation/nav_behaviors.hpp>
#include <engine/navigation/navigation_systems.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/render/debug_draw.hpp>
#include <engine/core/input.hpp>

#include <imgui.h>

namespace engine::navigation {

using namespace engine::core;
using namespace engine::render;

// Color constants
static constexpr uint32_t COLOR_NAVMESH = 0x4080FF80;       // Light blue, semi-transparent
static constexpr uint32_t COLOR_AGENT_IDLE = 0x808080FF;     // Gray
static constexpr uint32_t COLOR_AGENT_MOVING = 0x00FF00FF;   // Green
static constexpr uint32_t COLOR_AGENT_WAITING = 0xFFFF00FF;  // Yellow
static constexpr uint32_t COLOR_AGENT_ARRIVED = 0x00FFFFFF;  // Cyan
static constexpr uint32_t COLOR_AGENT_FAILED = 0xFF0000FF;   // Red
static constexpr uint32_t COLOR_PATH = 0xFFFF00FF;           // Yellow
static constexpr uint32_t COLOR_TARGET = 0x00FF00FF;         // Green
static constexpr uint32_t COLOR_VELOCITY = 0x00FFFFFF;       // Cyan
static constexpr uint32_t COLOR_OBSTACLE = 0xFF8000FF;       // Orange
static constexpr uint32_t COLOR_OBSTACLE_DISABLED = 0x404040FF; // Dark gray

uint32_t DebugNavigationWindow::get_shortcut_key() const {
    return static_cast<uint32_t>(Key::F8);
}

void DebugNavigationWindow::draw() {
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(get_title(), &m_open)) {
        ImGui::End();
        return;
    }

    // Status section
    ImGui::Text("Navigation Status");
    ImGui::Separator();

    bool nav_init = navigation_is_initialized();
    ImGui::Text("Initialized: %s", nav_init ? "Yes" : "No");

    if (m_navmesh && m_navmesh->is_valid()) {
        auto bounds = m_navmesh->get_bounds();
        ImGui::Text("NavMesh: Valid");
        ImGui::Text("Bounds: (%.1f, %.1f, %.1f) - (%.1f, %.1f, %.1f)",
            bounds.min.x, bounds.min.y, bounds.min.z,
            bounds.max.x, bounds.max.y, bounds.max.z);
    } else {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "NavMesh: Not loaded");
    }

    if (m_world) {
        auto agent_view = m_world->view<NavAgentComponent>();
        auto obstacle_view = m_world->view<NavObstacleComponent>();
        ImGui::Text("Agents: %d", static_cast<int>(agent_view.size()));
        ImGui::Text("Obstacles: %d", static_cast<int>(obstacle_view.size()));
    }

    ImGui::Spacing();

    // Visualization toggles
    ImGui::Text("Visualization");
    ImGui::Separator();

    ImGui::Checkbox("NavMesh", &m_draw_navmesh);
    ImGui::Checkbox("Agents", &m_draw_agents);
    ImGui::Checkbox("Paths", &m_draw_paths);
    ImGui::Checkbox("Velocities", &m_draw_velocities);
    ImGui::Checkbox("Obstacles", &m_draw_obstacles);

    ImGui::Spacing();

    // Draw visualizations
    if (nav_init && m_world) {
        if (m_draw_navmesh && m_navmesh) {
            draw_navmesh_visualization();
        }
        if (m_draw_agents) {
            draw_agent_visualization();
        }
        if (m_draw_obstacles) {
            draw_obstacle_visualization();
        }
    }

    // Agent list
    if (m_world) {
        ImGui::Text("Agent List");
        ImGui::Separator();

        ImGui::BeginChild("AgentList", ImVec2(0, 150), true);

        auto view = m_world->view<NavAgentComponent, scene::EntityInfo>();
        for (auto entity : view) {
            auto& info = view.get<scene::EntityInfo>(entity);
            auto& agent = view.get<NavAgentComponent>(entity);

            const char* state_str = "Unknown";
            ImVec4 state_color = ImVec4(1, 1, 1, 1);

            switch (agent.state) {
                case NavAgentState::Idle:
                    state_str = "Idle";
                    state_color = ImVec4(0.5f, 0.5f, 0.5f, 1);
                    break;
                case NavAgentState::Moving:
                    state_str = "Moving";
                    state_color = ImVec4(0, 1, 0, 1);
                    break;
                case NavAgentState::Waiting:
                    state_str = "Waiting";
                    state_color = ImVec4(1, 1, 0, 1);
                    break;
                case NavAgentState::Arrived:
                    state_str = "Arrived";
                    state_color = ImVec4(0, 1, 1, 1);
                    break;
                case NavAgentState::Failed:
                    state_str = "Failed";
                    state_color = ImVec4(1, 0, 0, 1);
                    break;
            }

            bool selected = (m_selected_agent == entity);
            ImGui::PushStyleColor(ImGuiCol_Text, state_color);
            if (ImGui::Selectable(info.name.c_str(), selected)) {
                m_selected_agent = entity;
            }
            ImGui::PopStyleColor();

            ImGui::SameLine(180);
            ImGui::TextColored(state_color, "[%s]", state_str);
        }

        ImGui::EndChild();

        // Selected agent details
        if (m_selected_agent != scene::NullEntity && m_world->valid(m_selected_agent)) {
            auto* agent = m_world->try_get<NavAgentComponent>(m_selected_agent);
            if (agent) {
                ImGui::Text("Selected Agent Details");
                ImGui::Separator();

                ImGui::Text("Speed: %.2f / %.2f", agent->current_speed, agent->speed);
                ImGui::Text("Path Points: %zu", agent->path.size());
                ImGui::Text("Path Index: %zu", agent->path_index);
                ImGui::Text("Remaining: %.2f", agent->path_distance);
                if (agent->has_target) {
                    ImGui::Text("Target: (%.1f, %.1f, %.1f)",
                        agent->target.x, agent->target.y, agent->target.z);
                }

                // Check for behavior component
                auto* behavior = m_world->try_get<NavBehaviorComponent>(m_selected_agent);
                if (behavior) {
                    ImGui::Spacing();
                    ImGui::Text("Behavior");
                    const char* type_str = "None";
                    switch (behavior->type) {
                        case NavBehaviorType::Wander: type_str = "Wander"; break;
                        case NavBehaviorType::Patrol: type_str = "Patrol"; break;
                        case NavBehaviorType::Follow: type_str = "Follow"; break;
                        case NavBehaviorType::Flee: type_str = "Flee"; break;
                        default: break;
                    }
                    ImGui::Text("Type: %s", type_str);
                    ImGui::Text("Enabled: %s", behavior->enabled ? "Yes" : "No");
                }
            }
        }
    }

    ImGui::End();
}

void DebugNavigationWindow::draw_navmesh_visualization() {
    if (!m_navmesh || !m_navmesh->is_valid()) return;

    auto geometry = m_navmesh->get_debug_geometry();

    // Draw navmesh triangles as wireframe
    for (size_t i = 0; i < geometry.size(); i += 3) {
        if (i + 2 >= geometry.size()) break;
        
        const auto& v0 = geometry[i].position;
        const auto& v1 = geometry[i+1].position;
        const auto& v2 = geometry[i+2].position;
        
        DebugDraw::line(v0, v1, COLOR_NAVMESH);
        DebugDraw::line(v1, v2, COLOR_NAVMESH);
        DebugDraw::line(v2, v0, COLOR_NAVMESH);
    }
}

void DebugNavigationWindow::draw_agent_visualization() {
    if (!m_world) return;

    auto view = m_world->view<NavAgentComponent, scene::LocalTransform>();

    for (auto entity : view) {
        auto& agent = view.get<NavAgentComponent>(entity);
        auto& transform = view.get<scene::LocalTransform>(entity);

        Vec3 pos = transform.position;

        // Determine color based on state
        uint32_t color;
        switch (agent.state) {
            case NavAgentState::Idle: color = COLOR_AGENT_IDLE; break;
            case NavAgentState::Moving: color = COLOR_AGENT_MOVING; break;
            case NavAgentState::Waiting: color = COLOR_AGENT_WAITING; break;
            case NavAgentState::Arrived: color = COLOR_AGENT_ARRIVED; break;
            case NavAgentState::Failed: color = COLOR_AGENT_FAILED; break;
            default: color = COLOR_AGENT_IDLE; break;
        }

        // Draw agent cylinder
        Vec3 top = pos + Vec3{0, agent.height, 0};
        DebugDraw::cylinder(pos, top, agent.avoidance_radius, color, 8);

        // Draw velocity vector
        if (m_draw_velocities && agent.state == NavAgentState::Moving) {
            Vec3 vel_end = pos + Vec3{0, agent.height * 0.5f, 0} + agent.velocity;
            DebugDraw::arrow(pos + Vec3{0, agent.height * 0.5f, 0}, vel_end, COLOR_VELOCITY);
        }

        // Draw path
        if (m_draw_paths && !agent.path.empty() && agent.path_index < agent.path.size()) {
            // Draw line from current position to first waypoint
            DebugDraw::line(pos, agent.path[agent.path_index], COLOR_PATH);

            // Draw remaining path
            for (size_t i = agent.path_index; i < agent.path.size() - 1; ++i) {
                DebugDraw::line(agent.path[i], agent.path[i + 1], COLOR_PATH);
            }

            // Draw target marker
            if (agent.has_target) {
                DebugDraw::sphere(agent.target, 0.3f, COLOR_TARGET, 8);
            }
        }
    }
}

void DebugNavigationWindow::draw_obstacle_visualization() {
    if (!m_world) return;

    auto view = m_world->view<NavObstacleComponent, scene::WorldTransform>();

    for (auto entity : view) {
        auto& obstacle = view.get<NavObstacleComponent>(entity);
        auto& transform = view.get<scene::WorldTransform>(entity);

        Vec3 pos = Vec3(transform.matrix[3]) + obstacle.offset;
        uint32_t color = obstacle.enabled ? COLOR_OBSTACLE : COLOR_OBSTACLE_DISABLED;

        switch (obstacle.shape) {
            case ObstacleShape::Cylinder: {
                Vec3 top = pos + Vec3{0, obstacle.cylinder_height, 0};
                DebugDraw::cylinder(pos, top, obstacle.cylinder_radius, color, 12);
                break;
            }
            case ObstacleShape::Box:
            case ObstacleShape::OrientedBox: {
                DebugDraw::box(pos, obstacle.half_extents * 2.0f, color);
                break;
            }
        }
    }
}

void DebugNavigationWindow::draw_path_visualization() {
    // This is called from draw_agent_visualization, kept for potential future use
}

} // namespace engine::navigation
