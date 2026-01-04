#include <engine/physics/vehicle.hpp>
#include <engine/physics/physics_world.hpp>
#include <cmath>
#include <algorithm>

namespace engine::physics {

// Constants
constexpr float PI = 3.14159265358979323846f;
constexpr float GRAVITY = 9.81f;
constexpr float FLIP_THRESHOLD = 1.2f;  // ~70 degrees

// Helper functions
static float clamp(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float vec3_length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static Vec3 vec3_normalize(const Vec3& v) {
    float len = vec3_length(v);
    if (len > 0.0001f) {
        return Vec3{v.x / len, v.y / len, v.z / len};
    }
    return Vec3{0.0f};
}

static float vec3_dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 quat_rotate(const Quat& q, const Vec3& v) {
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    return Vec3{
        v.x * (1.0f - 2.0f * (yy + zz)) + v.y * 2.0f * (xy - wz) + v.z * 2.0f * (xz + wy),
        v.x * 2.0f * (xy + wz) + v.y * (1.0f - 2.0f * (xx + zz)) + v.z * 2.0f * (yz - wx),
        v.x * 2.0f * (xz - wy) + v.y * 2.0f * (yz + wx) + v.z * (1.0f - 2.0f * (xx + yy))
    };
}

// Vehicle::Impl - Jolt-specific data (defined in vehicle_jolt.cpp)
struct Vehicle::Impl {
    // Jolt VehicleConstraint and controller will be stored here
    // For now, we use simplified physics
    float current_steering_angle = 0.0f;
    float current_engine_rpm = 1000.0f;
    float clutch_position = 1.0f;  // 0 = disengaged, 1 = engaged
    bool shifting = false;
    float shift_timer = 0.0f;
    int target_gear = 1;
};

// Vehicle implementation
Vehicle::Vehicle() : m_impl(std::make_unique<Impl>()) {}

Vehicle::~Vehicle() {
    shutdown();
}

Vehicle::Vehicle(Vehicle&&) noexcept = default;
Vehicle& Vehicle::operator=(Vehicle&&) noexcept = default;

void Vehicle::init(PhysicsWorld& world, const VehicleComponent& settings) {
    if (m_initialized) {
        shutdown();
    }

    m_world = &world;
    m_settings = settings;
    m_state = VehicleState{};

    // Initialize wheel states
    m_state.wheel_states.resize(settings.wheels.size());

    // Create chassis rigid body
    BodySettings body_settings;
    body_settings.type = BodyType::Dynamic;
    body_settings.shape = settings.chassis_shape.valueless_by_exception() ?
        nullptr : std::visit([](auto& s) -> ShapeSettings* { return &s; },
                            const_cast<ShapeVariant&>(settings.chassis_shape));
    body_settings.mass = settings.chassis_mass;
    body_settings.linear_damping = 0.1f;
    body_settings.angular_damping = 0.5f;
    body_settings.layer = settings.layer;
    body_settings.allow_sleep = false;  // Vehicles should stay awake

    m_chassis_body = world.create_body(body_settings);
    m_initialized = m_chassis_body.valid();

    // Initialize implementation
    m_impl->current_engine_rpm = settings.simulation.idle_rpm;
    m_impl->current_steering_angle = 0.0f;
}

void Vehicle::shutdown() {
    if (m_initialized && m_world && m_chassis_body.valid()) {
        m_world->destroy_body(m_chassis_body);
    }
    m_chassis_body = PhysicsBodyId{};
    m_world = nullptr;
    m_initialized = false;
}

void Vehicle::set_throttle(float value) {
    m_settings.throttle = clamp(value, 0.0f, 1.0f);
}

void Vehicle::set_brake(float value) {
    m_settings.brake = clamp(value, 0.0f, 1.0f);
}

void Vehicle::set_steering(float value) {
    m_settings.steering = clamp(value, -1.0f, 1.0f);
}

void Vehicle::set_handbrake(bool active) {
    m_settings.handbrake = active;
}

void Vehicle::set_input(float throttle, float brake, float steering, bool handbrake) {
    set_throttle(throttle);
    set_brake(brake);
    set_steering(steering);
    set_handbrake(handbrake);
}

void Vehicle::shift_up() {
    if (m_settings.mode != VehicleMode::Simulation) return;
    if (m_impl->shifting) return;

    int max_gear = static_cast<int>(m_settings.simulation.gear_ratios.size()) - 1;
    if (m_state.current_gear < max_gear) {
        m_impl->target_gear = m_state.current_gear + 1;
        m_impl->shifting = true;
        m_impl->shift_timer = m_settings.simulation.shift_time;
    }
}

void Vehicle::shift_down() {
    if (m_settings.mode != VehicleMode::Simulation) return;
    if (m_impl->shifting) return;

    if (m_state.current_gear > -1) {
        m_impl->target_gear = m_state.current_gear - 1;
        m_impl->shifting = true;
        m_impl->shift_timer = m_settings.simulation.shift_time;
    }
}

void Vehicle::set_gear(int gear) {
    int max_gear = static_cast<int>(m_settings.simulation.gear_ratios.size()) - 1;
    m_state.current_gear = clamp(gear, -1, max_gear);
}

int Vehicle::get_gear() const {
    return m_state.current_gear;
}

void Vehicle::set_auto_transmission(bool enabled) {
    m_settings.simulation.auto_transmission = enabled;
}

bool Vehicle::is_auto_transmission() const {
    return m_settings.simulation.auto_transmission;
}

void Vehicle::set_position(const Vec3& pos) {
    if (m_world && m_chassis_body.valid()) {
        m_world->set_position(m_chassis_body, pos);
    }
}

Vec3 Vehicle::get_position() const {
    if (m_world && m_chassis_body.valid()) {
        return m_world->get_position(m_chassis_body);
    }
    return Vec3{0.0f};
}

void Vehicle::set_rotation(const Quat& rot) {
    if (m_world && m_chassis_body.valid()) {
        m_world->set_rotation(m_chassis_body, rot);
    }
}

Quat Vehicle::get_rotation() const {
    if (m_world && m_chassis_body.valid()) {
        return m_world->get_rotation(m_chassis_body);
    }
    return Quat{1.0f, 0.0f, 0.0f, 0.0f};
}

void Vehicle::teleport(const Vec3& pos, const Quat& rot) {
    if (m_world && m_chassis_body.valid()) {
        m_world->set_transform(m_chassis_body, pos, rot);
        m_world->set_linear_velocity(m_chassis_body, Vec3{0.0f});
        m_world->set_angular_velocity(m_chassis_body, Vec3{0.0f});
    }
}

void Vehicle::add_impulse(const Vec3& impulse) {
    if (m_world && m_chassis_body.valid()) {
        m_world->add_impulse(m_chassis_body, impulse);
    }
}

void Vehicle::add_impulse_at_point(const Vec3& impulse, const Vec3& world_point) {
    if (m_world && m_chassis_body.valid()) {
        m_world->add_impulse_at_point(m_chassis_body, impulse, world_point);
    }
}

void Vehicle::flip_upright() {
    if (!m_world || !m_chassis_body.valid()) return;

    Vec3 pos = get_position();
    Quat rot = get_rotation();

    // Get current up vector
    Vec3 up = quat_rotate(rot, Vec3{0.0f, 1.0f, 0.0f});

    // If already upright enough, don't flip
    if (up.y > 0.5f) return;

    // Reset to upright with current heading preserved
    // Extract yaw from current rotation (simplified)
    Vec3 forward = quat_rotate(rot, Vec3{0.0f, 0.0f, 1.0f});
    float yaw = std::atan2(forward.x, forward.z);

    // Create new rotation with only yaw
    float half_yaw = yaw * 0.5f;
    Quat new_rot{std::cos(half_yaw), 0.0f, std::sin(half_yaw), 0.0f};

    // Lift slightly and reset
    pos.y += 1.0f;
    teleport(pos, new_rot);
}

void Vehicle::set_enabled(bool enabled) {
    m_enabled = enabled;
}

void Vehicle::set_mode(VehicleMode mode) {
    m_settings.mode = mode;
}

void Vehicle::update(float dt) {
    if (!m_initialized || !m_world || !m_chassis_body.valid() || !m_enabled) return;

    // Update state from physics
    update_state_from_physics();
    update_wheel_states();
    check_flip_state();

    // Update based on mode
    switch (m_settings.mode) {
        case VehicleMode::Arcade:
            update_arcade(dt);
            break;
        case VehicleMode::Simulation:
            update_simulation(dt);
            break;
    }
}

void Vehicle::update_state_from_physics() {
    if (!m_world || !m_chassis_body.valid()) return;

    m_state.velocity = m_world->get_linear_velocity(m_chassis_body);
    m_state.angular_velocity = m_world->get_angular_velocity(m_chassis_body);

    Quat rot = get_rotation();
    Vec3 forward = quat_rotate(rot, Vec3{0.0f, 0.0f, 1.0f});
    Vec3 right = quat_rotate(rot, Vec3{1.0f, 0.0f, 0.0f});

    m_state.speed = vec3_dot(m_state.velocity, forward);
    m_state.speed_kmh = m_state.speed * 3.6f;
    m_state.lateral_speed = vec3_dot(m_state.velocity, right);

    // Detect drifting (high lateral to forward speed ratio)
    if (std::abs(m_state.speed) > 5.0f) {
        float drift_ratio = std::abs(m_state.lateral_speed / m_state.speed);
        m_state.is_drifting = drift_ratio > 0.3f;
    } else {
        m_state.is_drifting = false;
    }
}

void Vehicle::update_wheel_states() {
    // Simplified wheel state - would use Jolt's wheel collision in full implementation
    Vec3 pos = get_position();
    m_state.is_grounded = true;  // Simplified - assume grounded
    m_state.wheels_on_ground = static_cast<int>(m_settings.wheels.size());
    m_state.is_airborne = false;
}

void Vehicle::check_flip_state() {
    Quat rot = get_rotation();
    Vec3 up = quat_rotate(rot, Vec3{0.0f, 1.0f, 0.0f});

    // Calculate angle from upright
    float dot = up.y;  // Dot with world up
    m_state.flip_angle = std::acos(clamp(dot, -1.0f, 1.0f)) * (180.0f / PI);

    m_state.is_flipped = std::abs(dot) < std::cos(FLIP_THRESHOLD);
}

void Vehicle::update_arcade(float dt) {
    arcade_acceleration(dt);
    arcade_steering(dt);
    arcade_stability(dt);
}

void Vehicle::arcade_acceleration(float dt) {
    const auto& arcade = m_settings.arcade;
    Quat rot = get_rotation();
    Vec3 forward = quat_rotate(rot, Vec3{0.0f, 0.0f, 1.0f});

    float current_speed = m_state.speed;
    float target_speed = 0.0f;

    if (m_settings.throttle > 0.01f) {
        target_speed = m_settings.throttle * arcade.max_speed;
    } else if (m_settings.brake > 0.01f && arcade.instant_reverse) {
        target_speed = -m_settings.brake * arcade.reverse_max_speed;
    }

    // Calculate acceleration
    float accel = 0.0f;
    if (target_speed > current_speed) {
        accel = arcade.acceleration;
        if (current_speed < 0.0f) accel += arcade.braking;  // Brake when reversing
    } else if (target_speed < current_speed) {
        accel = -arcade.deceleration;
        if (m_settings.brake > 0.01f) {
            accel = -arcade.braking * m_settings.brake;
        }
    }

    // Apply handbrake
    if (m_settings.handbrake && current_speed > 0.5f) {
        accel = -arcade.braking * 1.5f;
    }

    // Calculate force
    float delta_speed = accel * dt;
    if (target_speed > current_speed) {
        delta_speed = std::min(delta_speed, target_speed - current_speed);
    } else {
        delta_speed = std::max(delta_speed, target_speed - current_speed);
    }

    // Apply force
    Vec3 force{
        forward.x * delta_speed * m_settings.chassis_mass / dt,
        0.0f,
        forward.z * delta_speed * m_settings.chassis_mass / dt
    };

    if (m_state.is_grounded) {
        m_world->add_force(m_chassis_body, force);
    } else if (arcade.air_control > 0.0f) {
        // Limited air control
        force.x *= arcade.air_control;
        force.z *= arcade.air_control;
        m_world->add_force(m_chassis_body, force);
    }

    // Apply downforce
    if (std::abs(current_speed) > 10.0f) {
        float downforce = arcade.downforce * current_speed * current_speed * 0.5f;
        m_world->add_force(m_chassis_body, Vec3{0.0f, -downforce, 0.0f});
    }
}

void Vehicle::arcade_steering(float dt) {
    const auto& arcade = m_settings.arcade;

    // Speed-sensitive steering
    float speed_factor = 1.0f - arcade.speed_sensitive_steering *
        clamp(std::abs(m_state.speed) / arcade.max_speed, 0.0f, 1.0f);

    float target_steering = m_settings.steering * speed_factor;

    // Smooth steering
    float steering_delta = arcade.steering_speed * dt;
    if (std::abs(m_settings.steering) < 0.1f) {
        steering_delta = arcade.steering_return_speed * dt;
    }

    float current = m_impl->current_steering_angle;
    if (target_steering > current) {
        m_impl->current_steering_angle = std::min(current + steering_delta, target_steering);
    } else {
        m_impl->current_steering_angle = std::max(current - steering_delta, target_steering);
    }

    // Apply steering as angular velocity
    if (m_state.is_grounded && std::abs(m_state.speed) > 0.5f) {
        float turn_rate = m_impl->current_steering_angle * 2.0f;  // Tuning value
        if (m_state.speed < 0.0f) turn_rate = -turn_rate;  // Reverse steering

        Vec3 angular_vel = m_world->get_angular_velocity(m_chassis_body);
        angular_vel.y = turn_rate;
        m_world->set_angular_velocity(m_chassis_body, angular_vel);
    }

    // Apply drift physics
    if (m_state.is_drifting) {
        Quat rot = get_rotation();
        Vec3 right = quat_rotate(rot, Vec3{1.0f, 0.0f, 0.0f});

        // Reduce lateral velocity based on drift factor
        float lateral_reduction = (1.0f - arcade.drift_factor) * std::abs(m_state.lateral_speed);
        Vec3 drift_force{
            -right.x * lateral_reduction * m_settings.chassis_mass * 2.0f,
            0.0f,
            -right.z * lateral_reduction * m_settings.chassis_mass * 2.0f
        };
        m_world->add_force(m_chassis_body, drift_force);
    }
}

void Vehicle::arcade_stability(float dt) {
    // Keep vehicle upright
    Quat rot = get_rotation();
    Vec3 up = quat_rotate(rot, Vec3{0.0f, 1.0f, 0.0f});

    // Roll correction
    float roll_error = -up.x;
    float roll_correction = roll_error * 50.0f * m_settings.chassis_mass;

    // Pitch correction
    float pitch_error = up.z;
    float pitch_correction = pitch_error * 20.0f * m_settings.chassis_mass;

    Vec3 angular_vel = m_world->get_angular_velocity(m_chassis_body);

    // Add damping
    roll_correction -= angular_vel.z * 10.0f * m_settings.chassis_mass;
    pitch_correction -= angular_vel.x * 10.0f * m_settings.chassis_mass;

    m_world->add_torque(m_chassis_body, Vec3{pitch_correction, 0.0f, roll_correction});
}

void Vehicle::update_simulation(float dt) {
    // Handle gear shifting
    if (m_impl->shifting) {
        m_impl->shift_timer -= dt;
        if (m_impl->shift_timer <= 0.0f) {
            m_state.current_gear = m_impl->target_gear;
            m_impl->shifting = false;
        }
        m_state.is_shifting = true;
    } else {
        m_state.is_shifting = false;
    }

    simulation_engine(dt);
    simulation_transmission(dt);
    simulation_differential(dt);
}

void Vehicle::simulation_engine(float dt) {
    const auto& sim = m_settings.simulation;

    // Engine RPM based on wheel speed and gear
    float wheel_rpm = std::abs(m_state.speed) * 60.0f / (2.0f * PI * 0.3f);  // Assume 0.3m wheel radius

    float gear_ratio = 0.0f;
    if (m_state.current_gear > 0 && m_state.current_gear < static_cast<int>(sim.gear_ratios.size())) {
        gear_ratio = sim.gear_ratios[m_state.current_gear];
    } else if (m_state.current_gear == -1 && !sim.gear_ratios.empty()) {
        gear_ratio = -sim.gear_ratios[0];  // Reverse
    }

    float engine_rpm_from_wheels = wheel_rpm * gear_ratio * sim.final_drive_ratio;

    // Blend between throttle-based RPM and wheel-based RPM
    float target_rpm = sim.idle_rpm;
    if (m_settings.throttle > 0.01f && !m_impl->shifting) {
        target_rpm = sim.idle_rpm + m_settings.throttle * (sim.max_rpm - sim.idle_rpm);
    }

    if (m_state.current_gear != 0 && m_impl->clutch_position > 0.5f) {
        // Engaged - RPM follows wheels
        target_rpm = std::max(engine_rpm_from_wheels, sim.idle_rpm);
    }

    // Smooth RPM changes
    float rpm_rate = 5000.0f * dt;
    if (target_rpm > m_impl->current_engine_rpm) {
        m_impl->current_engine_rpm = std::min(m_impl->current_engine_rpm + rpm_rate, target_rpm);
    } else {
        m_impl->current_engine_rpm = std::max(m_impl->current_engine_rpm - rpm_rate * 0.5f, target_rpm);
    }

    m_impl->current_engine_rpm = clamp(m_impl->current_engine_rpm, sim.idle_rpm, sim.max_rpm);
    m_state.current_rpm = m_impl->current_engine_rpm;

    // Auto transmission
    if (sim.auto_transmission && !m_impl->shifting) {
        if (m_impl->current_engine_rpm > sim.shift_up_rpm && m_state.current_gear > 0) {
            shift_up();
        } else if (m_impl->current_engine_rpm < sim.shift_down_rpm && m_state.current_gear > 1) {
            shift_down();
        }
    }
}

void Vehicle::simulation_transmission(float dt) {
    if (m_impl->shifting || m_state.current_gear == 0) return;

    const auto& sim = m_settings.simulation;

    // Calculate torque at engine RPM (simplified torque curve)
    float rpm_fraction = (m_impl->current_engine_rpm - sim.idle_rpm) / (sim.max_rpm - sim.idle_rpm);
    float torque_curve = 1.0f - std::pow(rpm_fraction - 0.6f, 2.0f);  // Peak at 60% RPM
    float engine_torque = sim.max_torque * torque_curve * m_settings.throttle;

    // Apply braking
    if (m_settings.brake > 0.01f) {
        engine_torque -= sim.max_torque * m_settings.brake * 2.0f;
    }

    // Get gear ratio
    float gear_ratio = 0.0f;
    if (m_state.current_gear > 0 && m_state.current_gear < static_cast<int>(sim.gear_ratios.size())) {
        gear_ratio = sim.gear_ratios[m_state.current_gear];
    } else if (m_state.current_gear == -1 && !sim.gear_ratios.empty()) {
        gear_ratio = -sim.gear_ratios[0];
    }

    if (std::abs(gear_ratio) < 0.01f) return;

    // Calculate wheel torque
    float wheel_torque = engine_torque * gear_ratio * sim.final_drive_ratio;

    // Convert to force (assume 0.3m wheel radius)
    float wheel_force = wheel_torque / 0.3f;

    // Apply force
    Quat rot = get_rotation();
    Vec3 forward = quat_rotate(rot, Vec3{0.0f, 0.0f, 1.0f});

    if (m_state.is_grounded) {
        m_world->add_force(m_chassis_body, Vec3{
            forward.x * wheel_force,
            0.0f,
            forward.z * wheel_force
        });
    }
}

void Vehicle::simulation_differential(float dt) {
    // Simplified differential - just apply equal force to driven wheels
    // Full implementation would distribute torque based on differential type
}

// Factory functions
VehicleComponent make_sedan() {
    VehicleComponent v;
    v.type = VehicleType::Wheeled;
    v.mode = VehicleMode::Arcade;
    v.drive_type = DriveType::FrontWheelDrive;

    v.chassis_shape = BoxShapeSettings{Vec3{1.0f, 0.5f, 2.0f}};
    v.chassis_mass = 1400.0f;
    v.center_of_mass_offset = Vec3{0.0f, -0.2f, 0.0f};

    // Four wheels
    float wheel_x = 0.8f;
    float wheel_z_front = 1.3f;
    float wheel_z_rear = -1.3f;

    // Front left
    v.wheels.push_back(WheelSettings{});
    v.wheels.back().attachment_point = Vec3{-wheel_x, -0.2f, wheel_z_front};
    v.wheels.back().is_steerable = true;
    v.wheels.back().is_driven = true;

    // Front right
    v.wheels.push_back(WheelSettings{});
    v.wheels.back().attachment_point = Vec3{wheel_x, -0.2f, wheel_z_front};
    v.wheels.back().is_steerable = true;
    v.wheels.back().is_driven = true;

    // Rear left
    v.wheels.push_back(WheelSettings{});
    v.wheels.back().attachment_point = Vec3{-wheel_x, -0.2f, wheel_z_rear};
    v.wheels.back().has_handbrake = true;

    // Rear right
    v.wheels.push_back(WheelSettings{});
    v.wheels.back().attachment_point = Vec3{wheel_x, -0.2f, wheel_z_rear};
    v.wheels.back().has_handbrake = true;

    v.arcade.max_speed = 40.0f;
    v.arcade.acceleration = 12.0f;

    return v;
}

VehicleComponent make_sports_car() {
    VehicleComponent v = make_sedan();
    v.drive_type = DriveType::RearWheelDrive;
    v.chassis_mass = 1200.0f;

    // Rear wheel drive
    for (auto& w : v.wheels) {
        w.is_driven = false;
    }
    v.wheels[2].is_driven = true;  // Rear left
    v.wheels[3].is_driven = true;  // Rear right

    v.arcade.max_speed = 60.0f;
    v.arcade.acceleration = 18.0f;
    v.arcade.drift_factor = 0.7f;

    return v;
}

VehicleComponent make_truck() {
    VehicleComponent v = make_sedan();
    v.chassis_shape = BoxShapeSettings{Vec3{1.2f, 0.8f, 3.0f}};
    v.chassis_mass = 3000.0f;
    v.drive_type = DriveType::RearWheelDrive;

    v.arcade.max_speed = 25.0f;
    v.arcade.acceleration = 6.0f;

    return v;
}

VehicleComponent make_motorcycle() {
    VehicleComponent v;
    v.type = VehicleType::Motorcycle;
    v.mode = VehicleMode::Arcade;

    v.chassis_shape = BoxShapeSettings{Vec3{0.3f, 0.4f, 1.0f}};
    v.chassis_mass = 200.0f;

    // Two wheels
    v.wheels.push_back(WheelSettings{});
    v.wheels.back().attachment_point = Vec3{0.0f, -0.3f, 0.6f};
    v.wheels.back().is_steerable = true;

    v.wheels.push_back(WheelSettings{});
    v.wheels.back().attachment_point = Vec3{0.0f, -0.3f, -0.6f};
    v.wheels.back().is_driven = true;

    v.arcade.max_speed = 50.0f;
    v.arcade.acceleration = 20.0f;

    return v;
}

} // namespace engine::physics
