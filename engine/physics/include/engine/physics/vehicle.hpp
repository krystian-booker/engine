#pragma once

#include <engine/physics/vehicle_component.hpp>
#include <memory>

namespace engine::physics {

class PhysicsWorld;

// Vehicle controller - manages vehicle physics
class Vehicle {
public:
    Vehicle();
    ~Vehicle();

    // Non-copyable, movable
    Vehicle(const Vehicle&) = delete;
    Vehicle& operator=(const Vehicle&) = delete;
    Vehicle(Vehicle&&) noexcept;
    Vehicle& operator=(Vehicle&&) noexcept;

    // Initialization
    void init(PhysicsWorld& world, const VehicleComponent& settings);
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Input control
    void set_throttle(float value);           // 0 to 1
    void set_brake(float value);              // 0 to 1
    void set_steering(float value);           // -1 to 1
    void set_handbrake(bool active);
    void set_input(float throttle, float brake, float steering, bool handbrake = false);

    // Gear control (simulation mode)
    void shift_up();
    void shift_down();
    void set_gear(int gear);                  // -1 = reverse, 0 = neutral, 1+ = forward
    int get_gear() const;
    void set_auto_transmission(bool enabled);
    bool is_auto_transmission() const;

    // Transform
    void set_position(const Vec3& pos);
    Vec3 get_position() const;
    void set_rotation(const Quat& rot);
    Quat get_rotation() const;
    void teleport(const Vec3& pos, const Quat& rot);

    // State queries
    const VehicleState& get_state() const { return m_state; }
    float get_speed() const { return m_state.speed; }
    float get_speed_kmh() const { return m_state.speed_kmh; }
    float get_speed_mph() const { return m_state.speed_kmh * 0.621371f; }
    float get_rpm() const { return m_state.current_rpm; }
    Vec3 get_velocity() const { return m_state.velocity; }
    bool is_grounded() const { return m_state.is_grounded; }
    bool is_flipped() const { return m_state.is_flipped; }
    bool is_drifting() const { return m_state.is_drifting; }

    // Forces
    void add_impulse(const Vec3& impulse);
    void add_impulse_at_point(const Vec3& impulse, const Vec3& world_point);

    // Recovery
    void flip_upright();                      // Reset if flipped

    // Enable/disable
    void set_enabled(bool enabled);
    bool is_enabled() const { return m_enabled; }

    // Physics update (called by system)
    void update(float dt);

    // Settings access
    const VehicleComponent& get_settings() const { return m_settings; }
    void set_mode(VehicleMode mode);

    // Chassis body access
    PhysicsBodyId get_chassis_body() const { return m_chassis_body; }

private:
    PhysicsWorld* m_world = nullptr;
    PhysicsBodyId m_chassis_body;
    VehicleComponent m_settings;
    VehicleState m_state;
    bool m_initialized = false;
    bool m_enabled = true;

    // Implementation details
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    // Update functions by mode
    void update_arcade(float dt);
    void update_simulation(float dt);

    // Common update helpers
    void update_state_from_physics();
    void update_wheel_states();
    void check_flip_state();
    void apply_arcade_forces(float dt);
    void apply_simulation_forces(float dt);

    // Arcade mode helpers
    void arcade_acceleration(float dt);
    void arcade_steering(float dt);
    void arcade_stability(float dt);

    // Simulation mode helpers
    void simulation_engine(float dt);
    void simulation_transmission(float dt);
    void simulation_differential(float dt);
};

// ECS component wrapper
struct VehicleControllerComponent {
    std::unique_ptr<Vehicle> vehicle;

    // Convenience accessors
    void set_input(float throttle, float brake, float steering, bool handbrake = false) {
        if (vehicle) vehicle->set_input(throttle, brake, steering, handbrake);
    }

    float get_speed() const {
        return vehicle ? vehicle->get_speed() : 0.0f;
    }

    float get_speed_kmh() const {
        return vehicle ? vehicle->get_speed_kmh() : 0.0f;
    }

    float get_rpm() const {
        return vehicle ? vehicle->get_rpm() : 0.0f;
    }

    int get_gear() const {
        return vehicle ? vehicle->get_gear() : 0;
    }

    bool is_grounded() const {
        return vehicle && vehicle->is_grounded();
    }

    bool is_flipped() const {
        return vehicle && vehicle->is_flipped();
    }

    const VehicleState* get_state() const {
        return vehicle ? &vehicle->get_state() : nullptr;
    }
};

} // namespace engine::physics
