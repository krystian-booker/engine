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
    if (!m_world) return;

    auto contacts = m_world->get_contact_points();

    for (const auto& contact : contacts) {
        // Draw contact point
        DebugDraw::sphere(contact.position, 0.02f, DebugDraw::RED);

        // Draw contact normal
        Vec3 normal_end = contact.position + contact.normal * 0.2f;
        DebugDraw::arrow(contact.position, normal_end, DebugDraw::YELLOW, 0.02f);

        // Draw penetration depth visualization
        if (contact.penetration_depth > 0.0f) {
            Vec3 penetration_end = contact.position - contact.normal * contact.penetration_depth;
            DebugDraw::line(contact.position, penetration_end, 0xFFA500FF);  // Orange
        }
    }
}

void PhysicsDebugRenderer::draw_constraints() {
    if (!m_world) return;

    auto constraints = m_world->get_all_constraints();

    for (const auto& constraint : constraints) {
        // Draw constraint anchor points
        DebugDraw::cross(constraint.world_anchor_a, 0.05f, 0xFF00FFFF);  // Purple
        DebugDraw::cross(constraint.world_anchor_b, 0.05f, 0xFF00FFFF);

        // Draw line connecting the two anchor points
        DebugDraw::line(constraint.world_anchor_a, constraint.world_anchor_b, 0xFF00FFFF);

        // Draw lines from body centers to anchors
        if (constraint.body_a.valid()) {
            Vec3 body_pos_a = m_world->get_position(constraint.body_a);
            DebugDraw::line(body_pos_a, constraint.world_anchor_a, 0x8000FFFF);  // Light purple
        }
        if (constraint.body_b.valid()) {
            Vec3 body_pos_b = m_world->get_position(constraint.body_b);
            DebugDraw::line(body_pos_b, constraint.world_anchor_b, 0x8000FFFF);
        }
    }
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

    // Get actual shape info and draw appropriate shape
    if (m_flags.shapes) {
        BodyShapeInfo shape_info = m_world->get_body_shape_info(body_id);
        Vec3 shape_pos = pos + rot * shape_info.center_offset;

        switch (shape_info.type) {
            case ShapeType::Box:
                draw_box_shape(shape_pos, rot, shape_info.dimensions, color);
                break;
            case ShapeType::Sphere:
                draw_sphere_shape(shape_pos, shape_info.dimensions.x, color);
                break;
            case ShapeType::Capsule:
                draw_capsule_shape(shape_pos, rot,
                                   shape_info.dimensions.x,  // radius
                                   shape_info.dimensions.y * 2.0f,  // full height
                                   color);
                break;
            case ShapeType::Cylinder:
                // Draw cylinder similar to capsule (approximate visualization)
                draw_capsule_shape(shape_pos, rot,
                                   shape_info.dimensions.x,
                                   shape_info.dimensions.y * 2.0f,
                                   color);
                break;
            default:
                // Fallback for complex shapes - draw bounding box
                draw_box_shape(shape_pos, rot, shape_info.dimensions, color);
                break;
        }
    }
}

uint32_t PhysicsDebugRenderer::get_body_color(PhysicsBodyId body_id) const {
    if (!m_world) return DebugDraw::WHITE;

    BodyType type = m_world->get_body_type(body_id);
    bool is_active = m_world->is_active(body_id);

    // Color scheme based on body type and state
    switch (type) {
        case BodyType::Static:
            return 0x808080FF;  // Gray

        case BodyType::Kinematic:
            return 0x0080FFFF;  // Blue

        case BodyType::Dynamic:
            if (is_active) {
                return 0x00FF00FF;  // Green (active)
            } else {
                return 0x008000FF;  // Dark green (sleeping)
            }
    }

    return DebugDraw::WHITE;
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
