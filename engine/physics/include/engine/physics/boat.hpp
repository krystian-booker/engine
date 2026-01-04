#pragma once

#include <engine/physics/body.hpp>
#include <engine/physics/shapes.hpp>
#include <engine/physics/buoyancy_component.hpp>
#include <memory>
#include <vector>

namespace engine::physics {

class PhysicsWorld;
class WaterVolume;

// Boat physics mode
enum class BoatMode : uint8_t {
    Arcade,     // Simplified, responsive handling
    Simulation  // Realistic hydrodynamics
};

// Hull configuration
struct HullSettings {
    // Hull shape (typically compound of box/convex hull primitives)
    ShapeVariant hull_shape;
    float hull_mass = 2000.0f;                      // kg
    Vec3 center_of_mass_offset{0.0f, -0.5f, 0.0f};  // Lower COM for stability

    // Buoyancy distribution (for manual control)
    std::vector<BuoyancyPoint> buoyancy_points;

    // Hull hydrodynamics
    float hull_drag_coefficient = 0.3f;    // Cd for drag calculation
    float hull_lift_coefficient = 0.1f;    // For planing hulls at speed
    Vec3 drag_reference_area{2.0f, 1.0f, 5.0f};  // Area in each axis (m^2)

    // Hull dimensions (for automatic buoyancy if no points specified)
    Vec3 hull_half_extents{1.5f, 0.5f, 4.0f};
};

// Propeller/motor configuration
struct PropellerSettings {
    Vec3 position{0.0f, -0.5f, -2.0f};          // Local position (usually stern)
    Vec3 thrust_direction{0.0f, 0.0f, 1.0f};    // Direction of thrust
    float max_thrust = 50000.0f;                 // Newtons at full throttle
    float max_rpm = 3000.0f;
    float propeller_radius = 0.5f;               // For cavitation calculations
    float efficiency = 0.7f;                     // Propeller efficiency (0-1)
    float reverse_efficiency = 0.5f;             // Lower efficiency in reverse
    float spin_up_time = 0.5f;                   // Time to reach full RPM
    float spin_down_time = 1.0f;                 // Time to stop
};

// Rudder configuration
struct RudderSettings {
    Vec3 position{0.0f, -0.3f, -3.0f};          // Local position (usually stern)
    float max_angle = 0.5f;                      // radians (~30 degrees)
    float area = 1.0f;                           // m^2
    float lift_coefficient = 1.5f;               // Rudder lift coefficient
    float turn_rate = 1.0f;                      // rad/s to reach target angle
    float stall_angle = 0.6f;                    // Angle at which rudder stalls
};

// Arcade mode settings (for responsive, game-friendly handling)
struct ArcadeBoatSettings {
    float max_speed = 20.0f;                // m/s
    float acceleration = 5.0f;              // m/s^2
    float deceleration = 3.0f;              // m/s^2 natural slowdown
    float braking = 8.0f;                   // m/s^2 active braking
    float turn_speed = 1.0f;                // rad/s at low speed
    float turn_speed_at_max = 0.3f;         // rad/s at max speed
    float stability_roll = 0.8f;            // Roll damping (0-1)
    float stability_pitch = 0.8f;           // Pitch damping (0-1)
    float drift_factor = 0.9f;              // How much momentum is preserved in turns
    float wave_response = 0.5f;             // How much waves affect the boat (0-1)
};

// Boat component for ECS
struct BoatComponent {
    // Physics mode
    BoatMode mode = BoatMode::Arcade;

    // Hull configuration
    HullSettings hull;

    // Propulsion
    std::vector<PropellerSettings> propellers;

    // Steering
    std::vector<RudderSettings> rudders;

    // Arcade mode settings
    ArcadeBoatSettings arcade;

    // Collision settings
    uint16_t layer = 1;  // layers::DYNAMIC
    uint16_t collision_mask = 0xFFFF;

    // Input state (set by game)
    float throttle = 0.0f;      // -1 to 1 (reverse to forward)
    float rudder = 0.0f;        // -1 to 1 (left to right)
    bool engine_on = true;

    // Runtime state (set by system)
    bool initialized = false;
};

// Boat runtime state
struct BoatState {
    // Motion
    Vec3 velocity{0.0f};
    Vec3 angular_velocity{0.0f};
    float speed = 0.0f;             // m/s forward speed
    float lateral_speed = 0.0f;     // m/s sideways drift

    // Orientation
    float heading = 0.0f;           // radians, 0 = +Z
    float pitch = 0.0f;             // radians
    float roll = 0.0f;              // radians

    // Water interaction
    float submerged_fraction = 0.0f;
    float water_line_height = 0.0f;
    bool in_water = true;

    // Status
    bool is_grounded = false;       // Hit bottom or shore
    bool is_capsized = false;       // Rolled too far (> 90 degrees)
    bool is_sinking = false;        // Submerged > threshold

    // Propulsion state
    float current_rpm = 0.0f;
    float current_rudder_angle = 0.0f;

    // Forces (for debug visualization)
    Vec3 buoyancy_force{0.0f};
    Vec3 drag_force{0.0f};
    Vec3 thrust_force{0.0f};
    Vec3 rudder_force{0.0f};
};

// Boat controller - manages boat physics
class Boat {
public:
    Boat();
    ~Boat();

    // Non-copyable, movable
    Boat(const Boat&) = delete;
    Boat& operator=(const Boat&) = delete;
    Boat(Boat&&) noexcept;
    Boat& operator=(Boat&&) noexcept;

    // Initialization
    void init(PhysicsWorld& world, const BoatComponent& settings);
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Input control
    void set_throttle(float value);     // -1 to 1
    void set_rudder(float value);       // -1 to 1
    void set_input(float throttle, float rudder);
    void set_engine(bool on);

    // Transform
    void set_position(const Vec3& pos);
    Vec3 get_position() const;
    void set_rotation(const Quat& rot);
    Quat get_rotation() const;
    void teleport(const Vec3& pos, const Quat& rot);

    // State queries
    const BoatState& get_state() const { return m_state; }
    float get_speed() const { return m_state.speed; }
    float get_speed_knots() const { return m_state.speed * 1.94384f; }
    bool is_capsized() const { return m_state.is_capsized; }
    bool is_grounded() const { return m_state.is_grounded; }

    // Physics update (called by system)
    void update(float dt, const WaterVolume* water);

    // Direct physics access
    PhysicsBodyId get_hull_body() const { return m_hull_body; }
    void add_impulse(const Vec3& impulse);
    void add_impulse_at_point(const Vec3& impulse, const Vec3& world_point);

    // Capsize recovery
    void flip_upright();

    // Settings access
    const BoatComponent& get_settings() const { return m_settings; }
    void set_mode(BoatMode mode);

private:
    PhysicsWorld* m_world = nullptr;
    PhysicsBodyId m_hull_body;
    BoatComponent m_settings;
    BoatState m_state;
    bool m_initialized = false;

    // Update functions
    void update_arcade(float dt, const WaterVolume* water);
    void update_simulation(float dt, const WaterVolume* water);

    // Force application
    void apply_buoyancy(const WaterVolume& water);
    void apply_hydrodynamic_drag(const WaterVolume& water);
    void apply_propulsion(float dt);
    void apply_rudder_forces();
    void apply_arcade_stability();

    // State calculations
    void update_state_from_physics();
    void check_capsize();
    void check_grounded(const WaterVolume* water);
};

// ECS component wrapper
struct BoatControllerComponent {
    std::unique_ptr<Boat> boat;

    // Convenience accessors
    void set_input(float throttle, float rudder) {
        if (boat) boat->set_input(throttle, rudder);
    }

    float get_speed() const {
        return boat ? boat->get_speed() : 0.0f;
    }

    float get_speed_knots() const {
        return boat ? boat->get_speed_knots() : 0.0f;
    }

    bool is_capsized() const {
        return boat && boat->is_capsized();
    }

    const BoatState* get_state() const {
        return boat ? &boat->get_state() : nullptr;
    }
};

// Factory functions for common boat configurations
BoatComponent make_small_motorboat();
BoatComponent make_speedboat();
BoatComponent make_sailboat();
BoatComponent make_cargo_ship();

} // namespace engine::physics
