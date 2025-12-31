#pragma once

#include <engine/physics/physics_world.hpp>
#include <engine/render/debug_draw.hpp>

namespace engine::physics {

// Debug renderer for physics visualization
class PhysicsDebugRenderer {
public:
    // What to draw
    struct DrawFlags {
        bool bodies = true;
        bool shapes = true;
        bool contacts = false;
        bool constraints = false;
        bool aabbs = false;
        bool velocities = false;
        bool center_of_mass = false;
    };

    explicit PhysicsDebugRenderer(PhysicsWorld* world = nullptr);

    void set_world(PhysicsWorld* world) { m_world = world; }
    PhysicsWorld* world() const { return m_world; }

    void set_flags(const DrawFlags& flags) { m_flags = flags; }
    DrawFlags& flags() { return m_flags; }
    const DrawFlags& flags() const { return m_flags; }

    // Draw all physics debug visualization
    void draw();

    // Draw specific elements
    void draw_bodies();
    void draw_contacts();
    void draw_constraints();
    void draw_body(PhysicsBodyId body_id);

private:
    // Color helpers based on body state
    uint32_t get_body_color(PhysicsBodyId body_id) const;

    // Shape drawing
    void draw_box_shape(const core::Vec3& pos, const core::Quat& rot,
                        const core::Vec3& half_extents, uint32_t color);
    void draw_sphere_shape(const core::Vec3& pos, float radius, uint32_t color);
    void draw_capsule_shape(const core::Vec3& pos, const core::Quat& rot,
                            float radius, float height, uint32_t color);

    PhysicsWorld* m_world = nullptr;
    DrawFlags m_flags;
};

} // namespace engine::physics
