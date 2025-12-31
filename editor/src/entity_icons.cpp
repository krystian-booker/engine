#include "entity_icons.hpp"
#include <engine/scene/render_components.hpp>

namespace editor {

bool EntityIcons::s_initialized = false;
QIcon EntityIcons::s_default_icon;
QIcon EntityIcons::s_mesh_icon;
QIcon EntityIcons::s_camera_icon;
QIcon EntityIcons::s_light_directional_icon;
QIcon EntityIcons::s_light_point_icon;
QIcon EntityIcons::s_light_spot_icon;
QIcon EntityIcons::s_particle_icon;
QIcon EntityIcons::s_visibility_on_icon;
QIcon EntityIcons::s_visibility_off_icon;

void EntityIcons::init() {
    if (s_initialized) return;

    s_default_icon = QIcon(":/icons/entity.svg");
    s_mesh_icon = QIcon(":/icons/mesh.svg");
    s_camera_icon = QIcon(":/icons/camera.svg");
    s_light_directional_icon = QIcon(":/icons/light_directional.svg");
    s_light_point_icon = QIcon(":/icons/light_point.svg");
    s_light_spot_icon = QIcon(":/icons/light_spot.svg");
    s_particle_icon = QIcon(":/icons/particle.svg");
    s_visibility_on_icon = QIcon(":/icons/visibility_on.svg");
    s_visibility_off_icon = QIcon(":/icons/visibility_off.svg");

    s_initialized = true;
}

QIcon EntityIcons::get_entity_icon(engine::scene::World& world, engine::scene::Entity entity) {
    if (!s_initialized) init();

    // Priority order: Camera > Light > MeshRenderer > Particle > default
    if (world.has<engine::scene::Camera>(entity)) {
        return s_camera_icon;
    }

    if (world.has<engine::scene::Light>(entity)) {
        auto* light = world.try_get<engine::scene::Light>(entity);
        if (light) {
            switch (light->type) {
                case engine::scene::LightType::Directional:
                    return s_light_directional_icon;
                case engine::scene::LightType::Point:
                    return s_light_point_icon;
                case engine::scene::LightType::Spot:
                    return s_light_spot_icon;
            }
        }
        return s_light_point_icon;
    }

    if (world.has<engine::scene::MeshRenderer>(entity)) {
        return s_mesh_icon;
    }

    if (world.has<engine::scene::ParticleEmitter>(entity)) {
        return s_particle_icon;
    }

    return s_default_icon;
}

QIcon EntityIcons::visibility_icon(bool visible) {
    if (!s_initialized) init();
    return visible ? s_visibility_on_icon : s_visibility_off_icon;
}

QIcon EntityIcons::default_icon() {
    if (!s_initialized) init();
    return s_default_icon;
}

QIcon EntityIcons::mesh_icon() {
    if (!s_initialized) init();
    return s_mesh_icon;
}

QIcon EntityIcons::camera_icon() {
    if (!s_initialized) init();
    return s_camera_icon;
}

QIcon EntityIcons::directional_light_icon() {
    if (!s_initialized) init();
    return s_light_directional_icon;
}

QIcon EntityIcons::point_light_icon() {
    if (!s_initialized) init();
    return s_light_point_icon;
}

QIcon EntityIcons::spot_light_icon() {
    if (!s_initialized) init();
    return s_light_spot_icon;
}

QIcon EntityIcons::particle_icon() {
    if (!s_initialized) init();
    return s_particle_icon;
}

} // namespace editor
