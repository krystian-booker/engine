#pragma once

#include <engine/debug-gui/debug_window.hpp>
#include <engine/scene/entity.hpp>
#include <entt/entt.hpp>

namespace engine::scene {
    class World;
}

namespace engine::reflect {
    struct PropertyInfo;
}

namespace engine::debug_gui {

// Debug window for inspecting and editing entities
class DebugEntityInspector : public IDebugWindow {
public:
    const char* get_name() const override { return "entity_inspector"; }
    const char* get_title() const override { return "Entity Inspector"; }
    uint32_t get_shortcut_key() const override;

    void draw() override;

    void set_world(scene::World* world) { m_world = world; }
    scene::World* get_world() const { return m_world; }

    void select_entity(scene::Entity entity) { m_selected = entity; }
    scene::Entity get_selected() const { return m_selected; }

private:
    void draw_hierarchy();
    void draw_entity_node(scene::Entity entity);
    void draw_inspector();
    void draw_component(const std::string& type_name);
    void draw_property_editor(const reflect::PropertyInfo& prop, entt::meta_any& comp_any);

    scene::World* m_world = nullptr;
    scene::Entity m_selected = scene::NullEntity;
    char m_search_filter[128] = {};
    bool m_show_hidden = false;
};

} // namespace engine::debug_gui
