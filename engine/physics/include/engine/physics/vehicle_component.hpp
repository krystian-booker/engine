#pragma once

#include <engine/physics/body.hpp>
#include <engine/physics/shapes.hpp>
#include <vector>

namespace engine::physics {

// Vehicle type
enum class VehicleType : uint8_t {
    Wheeled,        // Car, truck (4+ wheels)
    Tracked,        // Tank, bulldozer
    Motorcycle      // Two-wheeled with lean
};

// Vehicle physics mode
enum class VehicleMode : uint8_t {
    Arcade,         // Simplified, responsive, game-friendly
    Simulation      // Realistic engine/transmission/differential
};

// Drive type for wheeled vehicles
enum class DriveType : uint8_t {
    FrontWheelDrive,
    RearWheelDrive,
    AllWheelDrive
};

// Differential type
enum class DifferentialType : uint8_t {
    Open,           // Power goes to wheel with least resistance
    Limited,        // Limited slip differential
    Locked          // Equal power to both wheels
};

// Wheel configuration
struct WheelSettings {
    Vec3 attachment_point{0.0f};              // Local position on chassis
    Vec3 wheel_direction{0.0f, -1.0f, 0.0f};  // Suspension direction (usually down)
    Vec3 steering_axis{0.0f, 1.0f, 0.0f};     // Steering rotation axis (usually up)

    // Wheel geometry
    float radius = 0.3f;
    float width = 0.2f;

    // Suspension
    float suspension_min = 0.0f;              // Minimum compression
    float suspension_max = 0.3f;              // Maximum extension
    float suspension_stiffness = 50000.0f;    // N/m
    float suspension_damping = 500.0f;        // Ns/m
    float suspension_preload = 0.0f;          // Initial compression

    // Tire friction
    float longitudinal_friction = 1.0f;       // Forward/backward grip
    float lateral_friction = 1.0f;            // Sideways grip

    // Function
    float max_steering_angle = 0.5f;          // radians (~28 degrees)
    bool is_steerable = false;                // Connected to steering
    bool is_driven = false;                   // Connected to drivetrain
    bool has_handbrake = false;               // Affected by handbrake

    // Anti-roll bar connection
    int anti_roll_bar_group = -1;             // -1 = not connected
};

// Arcade mode settings (simplified, responsive)
struct ArcadeVehicleSettings {
    float max_speed = 30.0f;                  // m/s (~108 km/h)
    float reverse_max_speed = 10.0f;          // m/s
    float acceleration = 15.0f;               // m/s^2
    float braking = 25.0f;                    // m/s^2
    float deceleration = 5.0f;                // m/s^2 natural slowdown

    float steering_speed = 2.0f;              // rad/s
    float steering_return_speed = 3.0f;       // Auto-center rate
    float speed_sensitive_steering = 0.5f;    // Reduce steering at speed (0-1)

    float downforce = 1.0f;                   // Artificial downforce multiplier
    float air_control = 0.2f;                 // Control while airborne
    float drift_factor = 0.8f;                // Lateral grip reduction during drift

    bool auto_handbrake_at_low_speed = true;
    bool instant_reverse = false;             // Can reverse without stopping
};

// Simulation mode settings (realistic physics)
struct SimulationVehicleSettings {
    // Engine
    float max_rpm = 6000.0f;
    float idle_rpm = 1000.0f;
    float redline_rpm = 5500.0f;
    float max_torque = 300.0f;                // Nm at peak
    float peak_torque_rpm = 4000.0f;

    // Transmission
    std::vector<float> gear_ratios;           // Index 0 = reverse, 1 = first, etc.
    float final_drive_ratio = 3.5f;
    float shift_time = 0.2f;                  // seconds
    bool auto_transmission = true;
    float shift_up_rpm = 5500.0f;             // Auto upshift RPM
    float shift_down_rpm = 2000.0f;           // Auto downshift RPM

    // Differential
    DifferentialType differential_type = DifferentialType::Limited;
    float limited_slip_ratio = 0.5f;          // For LSD

    // Anti-roll bars
    float front_anti_roll = 1000.0f;          // Nm/rad
    float rear_anti_roll = 1000.0f;

    // Clutch
    float clutch_strength = 10.0f;
};

// Anti-roll bar settings
struct AntiRollBarSettings {
    int left_wheel_index = -1;
    int right_wheel_index = -1;
    float stiffness = 1000.0f;                // Nm/rad
};

// Main vehicle component
struct VehicleComponent {
    VehicleType type = VehicleType::Wheeled;
    VehicleMode mode = VehicleMode::Arcade;
    DriveType drive_type = DriveType::RearWheelDrive;

    // Chassis configuration
    ShapeVariant chassis_shape;
    float chassis_mass = 1500.0f;             // kg
    Vec3 center_of_mass_offset{0.0f, -0.3f, 0.0f};  // Lower for stability

    // Wheel configuration
    std::vector<WheelSettings> wheels;

    // Anti-roll bars
    std::vector<AntiRollBarSettings> anti_roll_bars;

    // Mode-specific settings
    ArcadeVehicleSettings arcade;
    SimulationVehicleSettings simulation;

    // Collision
    uint16_t layer = 1;                       // layers::DYNAMIC
    uint16_t wheel_collision_mask = 0xFFFF;

    // Input state (set by game)
    float throttle = 0.0f;                    // 0 to 1
    float brake = 0.0f;                       // 0 to 1
    float steering = 0.0f;                    // -1 to 1
    bool handbrake = false;

    // Runtime state (set by system)
    bool initialized = false;

    // Default constructor
    VehicleComponent() {
        // Default gear ratios for simulation mode
        simulation.gear_ratios = {-3.5f, 3.5f, 2.5f, 1.8f, 1.3f, 1.0f, 0.8f};
    }
};

// Vehicle runtime state
struct VehicleState {
    Vec3 velocity{0.0f};
    Vec3 angular_velocity{0.0f};
    float speed = 0.0f;                       // m/s forward
    float speed_kmh = 0.0f;                   // km/h
    float lateral_speed = 0.0f;               // m/s sideways

    // Engine/transmission
    float current_rpm = 0.0f;
    int current_gear = 1;                     // 0 = neutral, -1 = reverse, 1+ = forward
    bool is_shifting = false;

    // Ground contact
    bool is_grounded = false;                 // At least one wheel on ground
    int wheels_on_ground = 0;
    bool is_airborne = false;                 // All wheels off ground

    // Driving state
    bool is_drifting = false;
    bool is_flipped = false;
    float flip_angle = 0.0f;                  // Degrees from upright

    // Per-wheel state
    struct WheelState {
        bool in_contact = false;
        float suspension_compression = 0.0f;  // 0-1
        float slip_angle = 0.0f;              // radians
        float slip_ratio = 0.0f;              // -1 to 1
        float angular_velocity = 0.0f;        // rad/s
        PhysicsBodyId contact_body;
        Vec3 contact_point{0.0f};
        Vec3 contact_normal{0.0f, 1.0f, 0.0f};
    };
    std::vector<WheelState> wheel_states;
};

// Factory functions for common vehicle configurations
VehicleComponent make_sedan();
VehicleComponent make_sports_car();
VehicleComponent make_truck();
VehicleComponent make_motorcycle();

} // namespace engine::physics
