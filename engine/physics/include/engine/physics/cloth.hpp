#pragma once

#include <engine/physics/cloth_component.hpp>
#include <memory>

namespace engine::physics {

class PhysicsWorld;

// Cloth controller - manages cloth/soft body physics
class Cloth {
public:
    Cloth();
    ~Cloth();

    // Non-copyable, movable
    Cloth(const Cloth&) = delete;
    Cloth& operator=(const Cloth&) = delete;
    Cloth(Cloth&&) noexcept;
    Cloth& operator=(Cloth&&) noexcept;

    // Initialization
    void init(PhysicsWorld& world, const ClothComponent& settings);
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Enable/disable
    void set_enabled(bool enabled);
    bool is_enabled() const { return m_enabled; }

    // Transform (position and rotation of the cloth root)
    void set_position(const Vec3& pos);
    Vec3 get_position() const;
    void set_rotation(const Quat& rot);
    Quat get_rotation() const;
    void teleport(const Vec3& pos, const Quat& rot);

    // Attachment management
    void attach_vertex(uint32_t vertex_index, const Vec3& world_position);
    void attach_vertex_to_entity(uint32_t vertex_index, uint32_t entity_id, const Vec3& local_offset);
    void detach_vertex(uint32_t vertex_index);
    void detach_all();
    void set_attachment_position(uint32_t vertex_index, const Vec3& world_position);
    bool is_vertex_attached(uint32_t vertex_index) const;

    // Vertex manipulation
    void set_vertex_position(uint32_t index, const Vec3& position);
    Vec3 get_vertex_position(uint32_t index) const;
    void set_vertex_velocity(uint32_t index, const Vec3& velocity);
    Vec3 get_vertex_velocity(uint32_t index) const;
    void set_vertex_mass(uint32_t index, float mass);
    float get_vertex_mass(uint32_t index) const;
    uint32_t get_vertex_count() const;

    // Force application
    void add_force(const Vec3& force);                          // To all vertices
    void add_force_at_vertex(uint32_t index, const Vec3& force);
    void add_impulse(const Vec3& impulse);                      // To all vertices
    void add_impulse_at_vertex(uint32_t index, const Vec3& impulse);
    void add_explosion_force(const Vec3& center, float force, float radius);

    // Wind
    void set_wind_mode(ClothWindMode mode);
    void set_wind(const Vec3& direction, float strength);
    void set_wind_turbulence(float turbulence, float frequency);
    void apply_wind(const Vec3& wind_velocity); // Direct wind application

    // Render data (for mesh generation)
    const ClothState& get_state() const { return m_state; }
    void get_render_data(std::vector<Vec3>& positions,
                         std::vector<Vec3>& normals,
                         std::vector<uint32_t>& indices) const;

    // Bounds
    Vec3 get_bounds_min() const { return m_state.bounds_min; }
    Vec3 get_bounds_max() const { return m_state.bounds_max; }
    Vec3 get_center() const { return m_state.center; }

    // Physical properties
    void set_stiffness(float edge, float bend, float shear);
    void set_damping(float damping);
    void set_mass(float total_mass);
    void set_gravity(const Vec3& gravity);
    void set_use_gravity(bool use);

    // Collision
    void set_collision_enabled(bool world, bool dynamic, bool self);
    void set_collision_margin(float margin);
    void set_collision_mask(uint16_t mask);

    // Sleep/wake
    void wake_up();
    void put_to_sleep();
    bool is_sleeping() const { return m_state.is_sleeping; }

    // Solver settings
    void set_solver_iterations(int iterations);
    void set_substep_delta(float delta);

    // Reset to initial state
    void reset();

    // Physics update (called by system)
    void update(float dt);

    // Settings access
    const ClothComponent& get_settings() const { return m_settings; }

private:
    PhysicsWorld* m_world = nullptr;
    ClothComponent m_settings;
    ClothState m_state;
    bool m_initialized = false;
    bool m_enabled = true;

    // Root transform
    Vec3 m_position{0.0f};
    Quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};

    // Initial state for reset
    std::vector<Vec3> m_initial_positions;
    std::vector<ClothAttachment> m_initial_attachments;

    // Implementation details
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    // Internal helpers
    void generate_grid_mesh();
    void create_constraints();
    void update_bounds();
    void update_normals();
    void apply_constraints(float dt);
    void apply_gravity(float dt);
    void apply_wind_forces(float dt);
    void check_sleep_state();
    void sync_attachments();

    // Constraint solving
    void solve_distance_constraints();
    void solve_bend_constraints();
    void solve_attachment_constraints();
    void solve_collision_constraints();
};

// ECS component wrapper
struct ClothControllerComponent {
    std::unique_ptr<Cloth> cloth;

    // Convenience accessors
    void set_wind(const Vec3& direction, float strength) {
        if (cloth) cloth->set_wind(direction, strength);
    }

    bool is_sleeping() const {
        return cloth && cloth->is_sleeping();
    }

    void wake_up() {
        if (cloth) cloth->wake_up();
    }

    const ClothState* get_state() const {
        return cloth ? &cloth->get_state() : nullptr;
    }

    void get_render_data(std::vector<Vec3>& positions,
                         std::vector<Vec3>& normals,
                         std::vector<uint32_t>& indices) const {
        if (cloth) cloth->get_render_data(positions, normals, indices);
    }
};

} // namespace engine::physics
