#pragma once

#include <engine/debug-gui/debug_window.hpp>
#include <engine/scene/entity.hpp>

namespace engine::scene {
    class World;
}

namespace engine::navigation {

class NavMesh;

// Debug window for visualizing navigation data
class DebugNavigationWindow : public debug_gui::IDebugWindow {
public:
    const char* get_name() const override { return "navigation"; }
    const char* get_title() const override { return "Navigation Debug"; }
    uint32_t get_shortcut_key() const override;

    void draw() override;

    void set_world(scene::World* world) { m_world = world; }
    scene::World* get_world() const { return m_world; }

    void set_navmesh(NavMesh* navmesh) { m_navmesh = navmesh; }
    NavMesh* get_navmesh() const { return m_navmesh; }

private:
    void draw_navmesh_visualization();
    void draw_agent_visualization();
    void draw_obstacle_visualization();
    void draw_path_visualization();

    scene::World* m_world = nullptr;
    NavMesh* m_navmesh = nullptr;

    // Visualization toggles
    bool m_draw_navmesh = true;
    bool m_draw_agents = true;
    bool m_draw_paths = true;
    bool m_draw_obstacles = true;
    bool m_draw_velocities = true;

    // Selected entity for detailed view
    scene::Entity m_selected_agent = scene::NullEntity;
};

} // namespace engine::navigation
