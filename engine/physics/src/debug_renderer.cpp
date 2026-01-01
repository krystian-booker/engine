#include <engine/physics/debug_renderer.hpp>
#include <engine/physics/body.hpp>

namespace engine::physics {

using namespace engine::core;
using namespace engine::render;

PhysicsDebugRenderer::PhysicsDebugRenderer(PhysicsWorld* world)
    : m_world(world)
{
}

void PhysicsDebugRenderer::draw() {
    if (!m_world) return;

    if (m_flags.bodies || m_flags.shapes) {
        draw_bodies();
    }

    if (m_flags.contacts) {
        draw_contacts();
    }

    if (m_flags.constraints) {
        draw_constraints();
    }
}

void PhysicsDebugRenderer::draw_bodies() {
    if (!m_world) return;

    auto bodies = m_world->get_all_body_ids();
    for (const auto& body_id : bodies) {
        draw_body(body_id);
    }
}

void PhysicsDebugRenderer::draw_contacts() {
    // TODO: Draw contact points from the physics world
}

void PhysicsDebugRenderer::draw_constraints() {
    // TODO: Draw constraints/joints from the physics world
}

void PhysicsDebugRenderer::draw_body(PhysicsBodyId body_id) {
    if (!m_world) return;

    Vec3 pos = m_world->get_position(body_id);
    Quat rot = m_world->get_rotation(body_id);
    uint32_t color = get_body_color(body_id);

    // Draw center of mass
    if (m_flags.center_of_mass) {
        DebugDraw::cross(pos, 0.1f, DebugDraw::YELLOW);
    }

    // Draw velocity
    if (m_flags.velocities) {
        Vec3 vel = m_world->get_linear_velocity(body_id);
        if (glm::length(vel) > 0.01f) {
            DebugDraw::arrow(pos, pos + vel * 0.1f, DebugDraw::CYAN, 0.05f);
        }
    }

    // TODO: Get shape info from body and draw appropriate shape
    // For now, draw a generic box as placeholder
    if (m_flags.shapes) {
        draw_box_shape(pos, rot, Vec3{0.5f}, color);
    }
}

uint32_t PhysicsDebugRenderer::get_body_color(PhysicsBodyId /*body_id*/) const {
    // TODO: Get actual body type from physics world
    // For now return a default color
    return DebugDraw::GREEN;

    // Color scheme:
    // - Static bodies: Gray (0x808080FF)
    // - Kinematic bodies: Blue (0x0080FFFF)
    // - Dynamic active: Green (0x00FF00FF)
    // - Dynamic sleeping: Dark green (0x008000FF)
    // - Triggers: Purple (0xFF00FFFF)
}

void PhysicsDebugRenderer::draw_box_shape(const Vec3& pos, const Quat& rot,
                                          const Vec3& half_extents, uint32_t color) {
    DebugDraw::box(pos, half_extents * 2.0f, rot, color);
}

void PhysicsDebugRenderer::draw_sphere_shape(const Vec3& pos, float radius, uint32_t color) {
    DebugDraw::sphere(pos, radius, color);
}

void PhysicsDebugRenderer::draw_capsule_shape(const Vec3& pos, const Quat& rot,
                                              float radius, float height, uint32_t color) {
    Vec3 up = rot * Vec3{0, 1, 0};
    float half_height = height * 0.5f - radius;
    Vec3 a = pos - up * half_height;
    Vec3 b = pos + up * half_height;
    DebugDraw::capsule(a, b, radius, color);
}

} // namespace engine::physics
