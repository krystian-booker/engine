#pragma once

#include <engine/physics/physics_world.hpp>
#include <engine/core/math.hpp>
#include <memory>

namespace scene { class World; }

namespace engine::physics {

using namespace engine::core;

// Character controller settings
struct CharacterSettings {
    float height = 1.8f;              // Total height of character capsule
    float radius = 0.3f;              // Radius of character capsule
    float mass = 80.0f;               // Mass for physics interactions

    // Movement
    float max_slope_angle = 45.0f;    // Max walkable slope in degrees
    float step_height = 0.35f;        // Max step up height
    float skin_width = 0.02f;         // Collision skin (prevents interpenetration)

    // Physics response
    float push_force = 100.0f;        // Force applied when pushing dynamic objects
    bool can_push_objects = true;     // Whether character can push dynamic bodies

    // Collision
    uint16_t layer = 0x0002;          // Character layer (default: CHARACTER)
    uint16_t collide_with = 0xFFFF;   // Layers to collide with

    // Initial position
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

// Ground detection state
struct GroundState {
    bool on_ground = false;           // Standing on valid ground
    bool on_slope = false;            // On a slope (within max_slope_angle)
    bool sliding = false;             // On too-steep slope, sliding down
    bool was_on_ground = false;       // Was on ground last frame

    Vec3 ground_normal{0.0f, 1.0f, 0.0f};  // Normal of ground surface
    Vec3 ground_point{0.0f};          // Contact point with ground
    Vec3 ground_velocity{0.0f};       // Velocity of ground (for moving platforms)

    float slope_angle = 0.0f;         // Angle of slope in radians
    float time_since_grounded = 0.0f; // Time since last grounded (for coyote time)

    PhysicsBodyId ground_body;        // Body ID of ground (for moving platforms)
};

// Character controller for 3rd person gameplay
class CharacterController {
public:
    CharacterController();
    ~CharacterController();

    // Non-copyable
    CharacterController(const CharacterController&) = delete;
    CharacterController& operator=(const CharacterController&) = delete;

    // Movable
    CharacterController(CharacterController&&) noexcept;
    CharacterController& operator=(CharacterController&&) noexcept;

    // Initialization
    void init(PhysicsWorld& world, const CharacterSettings& settings);
    void shutdown();
    bool is_initialized() const;

    // Position/Rotation
    void set_position(const Vec3& pos);
    Vec3 get_position() const;
    void set_rotation(const Quat& rot);
    Quat get_rotation() const;

    // Movement input (call before update, values in local space)
    // direction should be normalized movement direction (e.g., from input stick)
    void set_movement_input(const Vec3& direction);
    void set_movement_input(float x, float z);  // Common 2D input (x = strafe, z = forward)

    // Jump
    void jump(float impulse = 5.0f);
    bool can_jump() const;

    // Physics update (call in fixed update)
    void update(float dt);

    // Ground detection
    const GroundState& get_ground_state() const;
    bool is_grounded() const;

    // Velocity
    Vec3 get_velocity() const;
    Vec3 get_linear_velocity() const;  // Alias
    void set_velocity(const Vec3& vel);
    void add_velocity(const Vec3& vel);

    // Movement parameters (can be changed at runtime)
    void set_movement_speed(float speed);
    float get_movement_speed() const;

    void set_jump_impulse(float impulse);
    float get_jump_impulse() const;

    void set_gravity_scale(float scale);
    float get_gravity_scale() const;

    void set_air_control(float control);  // 0-1, how much control while airborne
    float get_air_control() const;

    void set_friction(float friction);  // Ground friction
    float get_friction() const;

    void set_air_friction(float friction);  // Air resistance
    float get_air_friction() const;

    void set_acceleration(float accel);  // Ground acceleration
    float get_acceleration() const;

    void set_deceleration(float decel);  // Ground deceleration (when stopping)
    float get_deceleration() const;

    // Enable/disable
    void set_enabled(bool enabled);
    bool is_enabled() const;

    // Teleport (ignores physics, immediate position change)
    void teleport(const Vec3& position, const Quat& rotation = Quat{1, 0, 0, 0});

    // Force recalculation of ground state
    void refresh_ground_state();

    // Get the settings
    const CharacterSettings& get_settings() const { return m_settings; }

    // Coyote time (allows jump shortly after leaving ground)
    void set_coyote_time(float time) { m_coyote_time = time; }
    float get_coyote_time() const { return m_coyote_time; }

    // Jump buffer (remembers jump input for a short time)
    void set_jump_buffer_time(float time) { m_jump_buffer_time = time; }
    float get_jump_buffer_time() const { return m_jump_buffer_time; }

private:
    void update_ground_state(float dt);
    void apply_movement(float dt);
    void apply_gravity(float dt);
    void handle_step_up();

    CharacterSettings m_settings;

    // Physics reference
    PhysicsWorld* m_world = nullptr;

    // State
    Vec3 m_position{0.0f};
    Quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 m_velocity{0.0f};
    Vec3 m_movement_input{0.0f};
    GroundState m_ground_state;

    // Movement parameters
    float m_movement_speed = 5.0f;
    float m_jump_impulse = 5.0f;
    float m_gravity_scale = 1.0f;
    float m_air_control = 0.3f;
    float m_friction = 10.0f;
    float m_air_friction = 0.1f;
    float m_acceleration = 50.0f;
    float m_deceleration = 30.0f;

    // Jump mechanics
    float m_coyote_time = 0.15f;      // Time after leaving ground when jump still works
    float m_jump_buffer_time = 0.1f;  // Time before landing when jump input is buffered
    float m_time_since_jump_pressed = 1000.0f;
    bool m_jump_requested = false;
    bool m_has_jumped = false;        // Has jumped since last grounded

    bool m_enabled = true;
    bool m_initialized = false;

    // Pimpl for Jolt-specific implementation
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ECS Component wrapper
struct CharacterControllerComponent {
    std::unique_ptr<CharacterController> controller;

    // Convenience accessors
    void set_movement_input(const Vec3& dir) {
        if (controller) controller->set_movement_input(dir);
    }

    void jump(float impulse = 5.0f) {
        if (controller) controller->jump(impulse);
    }

    bool is_grounded() const {
        return controller ? controller->is_grounded() : false;
    }

    Vec3 get_velocity() const {
        return controller ? controller->get_velocity() : Vec3{0.0f};
    }
};

// System function to update all character controllers
void character_controller_system(class scene::World& world, PhysicsWorld& physics, float dt);

} // namespace engine::physics
