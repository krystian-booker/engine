#pragma once

#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>
#include <QIcon>

namespace editor {

// Provides icons for entities based on their components
class EntityIcons {
public:
    // Initialize icons (call once at startup)
    static void init();

    // Get icon for entity based on its components
    // Priority: Camera > Light > MeshRenderer > Particle > default
    static QIcon get_entity_icon(engine::scene::World& world, engine::scene::Entity entity);

    // Get visibility toggle icon
    static QIcon visibility_icon(bool visible);

    // Individual component icons for menu items
    static QIcon default_icon();
    static QIcon mesh_icon();
    static QIcon camera_icon();
    static QIcon directional_light_icon();
    static QIcon point_light_icon();
    static QIcon spot_light_icon();
    static QIcon particle_icon();

private:
    static bool s_initialized;
    static QIcon s_default_icon;
    static QIcon s_mesh_icon;
    static QIcon s_camera_icon;
    static QIcon s_light_directional_icon;
    static QIcon s_light_point_icon;
    static QIcon s_light_spot_icon;
    static QIcon s_particle_icon;
    static QIcon s_visibility_on_icon;
    static QIcon s_visibility_off_icon;
};

} // namespace editor
