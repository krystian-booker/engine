#include <engine/physics/boat.hpp>
#include <engine/physics/water_volume.hpp>
#include <engine/physics/physics_world.hpp>
#include <cmath>
#include <algorithm>

namespace engine::physics {

// Constants
constexpr float PI = 3.14159265358979323846f;
constexpr float GRAVITY = 9.81f;
constexpr float CAPSIZE_ANGLE = 1.4f;  // ~80 degrees

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
    return Vec3{0.0f, 0.0f, 0.0f};
}

static float vec3_dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 vec3_cross(const Vec3& a, const Vec3& b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// Extract euler angles from quaternion (roll, pitch, yaw)
static void quat_to_euler(const Quat& q, float& roll, float& pitch, float& yaw) {
    // Roll (x-axis rotation)
    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    roll = std::atan2(sinr_cosp, cosr_cosp);

    // Pitch (y-axis rotation)
    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    if (std::abs(sinp) >= 1.0f) {
        pitch = std::copysign(PI / 2.0f, sinp);
    } else {
        pitch = std::asin(sinp);
    }

    // Yaw (z-axis rotation)
    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    yaw = std::atan2(siny_cosp, cosy_cosp);
}

// Rotate vector by quaternion
static Vec3 quat_rotate(const Quat& q, const Vec3& v) {
    // Convert quaternion to rotation matrix and apply (simplified)
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    return Vec3{
        v.x * (1.0f - 2.0f * (yy + zz)) + v.y * 2.0f * (xy - wz) + v.z * 2.0f * (xz + wy),
        v.x * 2.0f * (xy + wz) + v.y * (1.0f - 2.0f * (xx + zz)) + v.z * 2.0f * (yz - wx),
        v.x * 2.0f * (xz - wy) + v.y * 2.0f * (yz + wx) + v.z * (1.0f - 2.0f * (xx + yy))
    };
}

// Boat implementation
Boat::Boat() = default;
Boat::~Boat() { shutdown(); }

Boat::Boat(Boat&&) noexcept = default;
Boat& Boat::operator=(Boat&&) noexcept = default;

void Boat::init(PhysicsWorld& world, const BoatComponent& settings) {
    if (m_initialized) {
        shutdown();
    }

    m_world = &world;
    m_settings = settings;
    m_state = BoatState{};

    // Create hull rigid body
    BodySettings body_settings;
    body_settings.type = BodyType::Dynamic;
    body_settings.shape = settings.hull.hull_shape.valueless_by_exception() ?
        nullptr : std::visit([](auto& s) -> ShapeSettings* { return &s; },
                            const_cast<ShapeVariant&>(settings.hull.hull_shape));
    body_settings.mass = settings.hull.hull_mass;
    body_settings.linear_damping = 0.1f;
    body_settings.angular_damping = 0.2f;
    body_settings.layer = settings.layer;
    body_settings.allow_sleep = false;  // Boats should stay awake in water

    m_hull_body = world.create_body(body_settings);
    m_initialized = m_hull_body.valid();
}

void Boat::shutdown() {
    if (m_initialized && m_world && m_hull_body.valid()) {
        m_world->destroy_body(m_hull_body);
    }
    m_hull_body = PhysicsBodyId{};
    m_world = nullptr;
    m_initialized = false;
}

void Boat::set_throttle(float value) {
    m_settings.throttle = clamp(value, -1.0f, 1.0f);
}

void Boat::set_rudder(float value) {
    m_settings.rudder = clamp(value, -1.0f, 1.0f);
}

void Boat::set_input(float throttle, float rudder) {
    set_throttle(throttle);
    set_rudder(rudder);
}

void Boat::set_engine(bool on) {
    m_settings.engine_on = on;
}

void Boat::set_position(const Vec3& pos) {
    if (m_world && m_hull_body.valid()) {
        m_world->set_position(m_hull_body, pos);
    }
}

Vec3 Boat::get_position() const {
    if (m_world && m_hull_body.valid()) {
        return m_world->get_position(m_hull_body);
    }
    return Vec3{0.0f};
}

void Boat::set_rotation(const Quat& rot) {
    if (m_world && m_hull_body.valid()) {
        m_world->set_rotation(m_hull_body, rot);
    }
}

Quat Boat::get_rotation() const {
    if (m_world && m_hull_body.valid()) {
        return m_world->get_rotation(m_hull_body);
    }
    return Quat{1.0f, 0.0f, 0.0f, 0.0f};
}

void Boat::teleport(const Vec3& pos, const Quat& rot) {
    if (m_world && m_hull_body.valid()) {
        m_world->set_transform(m_hull_body, pos, rot);
        m_world->set_linear_velocity(m_hull_body, Vec3{0.0f});
        m_world->set_angular_velocity(m_hull_body, Vec3{0.0f});
    }
}

void Boat::add_impulse(const Vec3& impulse) {
    if (m_world && m_hull_body.valid()) {
        m_world->add_impulse(m_hull_body, impulse);
    }
}

void Boat::add_impulse_at_point(const Vec3& impulse, const Vec3& world_point) {
    if (m_world && m_hull_body.valid()) {
        m_world->add_impulse_at_point(m_hull_body, impulse, world_point);
    }
}

void Boat::flip_upright() {
    if (!m_world || !m_hull_body.valid()) return;

    Vec3 pos = get_position();
    Quat rot = get_rotation();

    // Extract current heading (yaw) and reset roll/pitch
    float roll, pitch, yaw;
    quat_to_euler(rot, roll, pitch, yaw);

    // Create new rotation with only yaw preserved
    float half_yaw = yaw * 0.5f;
    Quat new_rot{
        std::cos(half_yaw),
        0.0f,
        std::sin(half_yaw),
        0.0f
    };

    // Lift boat slightly above water
    pos.y += 1.0f;

    teleport(pos, new_rot);
    m_state.is_capsized = false;
}

void Boat::set_mode(BoatMode mode) {
    m_settings.mode = mode;
}

void Boat::update(float dt, const WaterVolume* water) {
    if (!m_initialized || !m_world || !m_hull_body.valid()) return;

    // Update state from physics
    update_state_from_physics();

    // Check water interaction
    m_state.in_water = (water != nullptr);
    if (!water) {
        // No water - just update state
        check_grounded(nullptr);
        return;
    }

    // Update based on mode
    switch (m_settings.mode) {
        case BoatMode::Arcade:
            update_arcade(dt, water);
            break;
        case BoatMode::Simulation:
            update_simulation(dt, water);
            break;
    }

    // Check for capsize
    check_capsize();
    check_grounded(water);
}

void Boat::update_arcade(float dt, const WaterVolume* water) {
    const auto& arcade = m_settings.arcade;

    // Apply buoyancy (simplified for arcade)
    apply_buoyancy(*water);

    // Apply arcade stability (keeps boat upright)
    apply_arcade_stability();

    // Calculate forward direction
    Quat rot = get_rotation();
    Vec3 forward = quat_rotate(rot, Vec3{0.0f, 0.0f, 1.0f});
    Vec3 right = quat_rotate(rot, Vec3{1.0f, 0.0f, 0.0f});

    // Get current velocity
    Vec3 velocity = m_world->get_linear_velocity(m_hull_body);
    float current_speed = vec3_dot(velocity, forward);

    // Throttle control
    if (m_settings.engine_on && std::abs(m_settings.throttle) > 0.01f) {
        float target_speed = m_settings.throttle * arcade.max_speed;
        float accel = (target_speed > current_speed) ? arcade.acceleration : arcade.braking;

        // Apply acceleration
        float speed_diff = target_speed - current_speed;
        float delta_v = clamp(speed_diff, -accel * dt, accel * dt);

        Vec3 thrust = Vec3{
            forward.x * delta_v * m_settings.hull.hull_mass,
            0.0f,
            forward.z * delta_v * m_settings.hull.hull_mass
        };

        m_world->add_impulse(m_hull_body, thrust);
        m_state.thrust_force = thrust;
    } else {
        // Natural deceleration
        float decel_force = arcade.deceleration * m_settings.hull.hull_mass;
        if (std::abs(current_speed) > 0.1f) {
            float decel = std::min(std::abs(current_speed), decel_force * dt / m_settings.hull.hull_mass);
            Vec3 drag = Vec3{
                -forward.x * decel * m_settings.hull.hull_mass * (current_speed > 0 ? 1.0f : -1.0f),
                0.0f,
                -forward.z * decel * m_settings.hull.hull_mass * (current_speed > 0 ? 1.0f : -1.0f)
            };
            m_world->add_impulse(m_hull_body, drag);
        }
    }

    // Rudder control (turning)
    if (std::abs(m_settings.rudder) > 0.01f && std::abs(current_speed) > 0.5f) {
        // Turn rate decreases at higher speeds
        float speed_factor = 1.0f - clamp(std::abs(current_speed) / arcade.max_speed, 0.0f, 1.0f);
        float turn_rate = lerp(arcade.turn_speed_at_max, arcade.turn_speed, speed_factor);

        // Apply turn rate based on rudder input
        float angular_velocity_y = m_settings.rudder * turn_rate * (current_speed > 0 ? 1.0f : -1.0f);

        Vec3 current_angular = m_world->get_angular_velocity(m_hull_body);
        current_angular.y = angular_velocity_y;
        m_world->set_angular_velocity(m_hull_body, current_angular);

        // Apply drift (lateral velocity reduction)
        float lateral_speed = vec3_dot(velocity, right);
        if (std::abs(lateral_speed) > 0.1f) {
            float drift_damping = 1.0f - arcade.drift_factor;
            Vec3 lateral_drag = Vec3{
                -right.x * lateral_speed * drift_damping * m_settings.hull.hull_mass * dt * 10.0f,
                0.0f,
                -right.z * lateral_speed * drift_damping * m_settings.hull.hull_mass * dt * 10.0f
            };
            m_world->add_impulse(m_hull_body, lateral_drag);
        }
    }

    // Apply water drag
    apply_hydrodynamic_drag(*water);
}

void Boat::update_simulation(float dt, const WaterVolume* water) {
    // Full simulation mode - use realistic forces
    apply_buoyancy(*water);
    apply_hydrodynamic_drag(*water);
    apply_propulsion(dt);
    apply_rudder_forces();
}

void Boat::apply_buoyancy(const WaterVolume& water) {
    if (!m_world || !m_hull_body.valid()) return;

    Vec3 pos = get_position();
    float water_density = water.get_density();

    // Calculate buoyancy using hull buoyancy points or automatic
    const auto& hull = m_settings.hull;

    if (hull.buoyancy_points.empty()) {
        // Automatic buoyancy based on hull dimensions
        const Vec3& half = hull.hull_half_extents;
        float hull_volume = 8.0f * half.x * half.y * half.z;

        float surface_height = water.get_surface_height_at(pos);
        float depth = surface_height - (pos.y - half.y);

        float submerged_fraction = clamp(depth / (2.0f * half.y), 0.0f, 1.0f);
        m_state.submerged_fraction = submerged_fraction;

        if (submerged_fraction > 0.001f) {
            float buoyancy_force = water_density * GRAVITY * hull_volume * submerged_fraction;
            m_world->add_force(m_hull_body, Vec3{0.0f, buoyancy_force, 0.0f});
            m_state.buoyancy_force = Vec3{0.0f, buoyancy_force, 0.0f};
        }
    } else {
        // Manual buoyancy points
        Quat rot = get_rotation();
        Vec3 total_force{0.0f};
        float total_submerged = 0.0f;
        float total_volume = 0.0f;

        for (const auto& point : hull.buoyancy_points) {
            Vec3 world_point = pos + quat_rotate(rot, point.local_position);
            float surface_height = water.get_surface_height_at(world_point);
            float depth = surface_height - world_point.y;

            total_volume += point.volume;

            if (depth > -point.radius) {
                float submerged_fraction = clamp((depth + point.radius) / (2.0f * point.radius), 0.0f, 1.0f);
                float submerged_volume = point.volume * submerged_fraction;
                total_submerged += submerged_volume;

                float buoyancy_force = water_density * GRAVITY * submerged_volume;
                Vec3 force{0.0f, buoyancy_force, 0.0f};

                // Apply force at point for torque
                m_world->add_force_at_point(m_hull_body, force, world_point);
                total_force.y += buoyancy_force;
            }
        }

        m_state.submerged_fraction = (total_volume > 0.0f) ? (total_submerged / total_volume) : 0.0f;
        m_state.buoyancy_force = total_force;
    }

    // Update water line
    m_state.water_line_height = water.get_surface_height_at(pos);
}

void Boat::apply_hydrodynamic_drag(const WaterVolume& water) {
    if (!m_world || !m_hull_body.valid()) return;
    if (m_state.submerged_fraction < 0.001f) return;

    Vec3 velocity = m_world->get_linear_velocity(m_hull_body);
    Vec3 angular_velocity = m_world->get_angular_velocity(m_hull_body);

    const auto& hull = m_settings.hull;
    float water_density = water.get_density();

    // Linear drag - direction dependent
    Quat rot = get_rotation();
    Vec3 forward = quat_rotate(rot, Vec3{0.0f, 0.0f, 1.0f});
    Vec3 right = quat_rotate(rot, Vec3{1.0f, 0.0f, 0.0f});
    Vec3 up = quat_rotate(rot, Vec3{0.0f, 1.0f, 0.0f});

    // Decompose velocity into components
    float v_forward = vec3_dot(velocity, forward);
    float v_lateral = vec3_dot(velocity, right);
    float v_vertical = vec3_dot(velocity, up);

    // Calculate drag for each axis
    // F_drag = 0.5 * ρ * Cd * A * v²
    float cd = hull.hull_drag_coefficient;
    const Vec3& area = hull.drag_reference_area;

    Vec3 drag_force{0.0f};

    // Forward/backward drag (lowest - streamlined)
    float drag_forward = -0.5f * water_density * cd * area.z * v_forward * std::abs(v_forward);
    drag_force = drag_force + Vec3{forward.x * drag_forward, forward.y * drag_forward, forward.z * drag_forward};

    // Lateral drag (highest - broad side)
    float drag_lateral = -0.5f * water_density * cd * 3.0f * area.x * v_lateral * std::abs(v_lateral);
    drag_force = drag_force + Vec3{right.x * drag_lateral, right.y * drag_lateral, right.z * drag_lateral};

    // Vertical drag
    float drag_vertical = -0.5f * water_density * cd * 2.0f * area.y * v_vertical * std::abs(v_vertical);
    drag_force = drag_force + Vec3{up.x * drag_vertical, up.y * drag_vertical, up.z * drag_vertical};

    // Scale by submerged fraction
    drag_force.x *= m_state.submerged_fraction;
    drag_force.y *= m_state.submerged_fraction;
    drag_force.z *= m_state.submerged_fraction;

    m_world->add_force(m_hull_body, drag_force);
    m_state.drag_force = drag_force;

    // Angular drag
    float angular_speed = vec3_length(angular_velocity);
    if (angular_speed > 0.01f) {
        float angular_drag = -0.5f * water_density * cd * 0.5f * angular_speed * m_state.submerged_fraction;
        Vec3 angular_drag_torque{
            angular_velocity.x * angular_drag,
            angular_velocity.y * angular_drag,
            angular_velocity.z * angular_drag
        };
        m_world->add_torque(m_hull_body, angular_drag_torque);
    }
}

void Boat::apply_propulsion(float dt) {
    if (!m_world || !m_hull_body.valid()) return;
    if (!m_settings.engine_on) return;
    if (m_state.submerged_fraction < 0.1f) return;  // Propeller needs to be in water

    Quat rot = get_rotation();
    Vec3 pos = get_position();

    Vec3 total_thrust{0.0f};

    for (auto& propeller : m_settings.propellers) {
        // Calculate current RPM with spin-up/down
        float target_rpm = std::abs(m_settings.throttle) * propeller.max_rpm;
        float rpm_rate = (target_rpm > m_state.current_rpm) ?
            propeller.max_rpm / propeller.spin_up_time :
            propeller.max_rpm / propeller.spin_down_time;
        m_state.current_rpm = lerp(m_state.current_rpm, target_rpm,
            std::min(1.0f, rpm_rate * dt / propeller.max_rpm));

        // Calculate thrust
        float efficiency = (m_settings.throttle >= 0) ?
            propeller.efficiency : propeller.reverse_efficiency;
        float thrust_fraction = m_state.current_rpm / propeller.max_rpm;
        float thrust = propeller.max_thrust * thrust_fraction * efficiency *
            (m_settings.throttle >= 0 ? 1.0f : -1.0f);

        // Transform thrust direction to world space
        Vec3 world_thrust_dir = quat_rotate(rot, propeller.thrust_direction);
        Vec3 thrust_force{
            world_thrust_dir.x * thrust,
            world_thrust_dir.y * thrust,
            world_thrust_dir.z * thrust
        };

        // Apply at propeller position
        Vec3 world_prop_pos = pos + quat_rotate(rot, propeller.position);
        m_world->add_force_at_point(m_hull_body, thrust_force, world_prop_pos);

        total_thrust.x += thrust_force.x;
        total_thrust.y += thrust_force.y;
        total_thrust.z += thrust_force.z;
    }

    m_state.thrust_force = total_thrust;
}

void Boat::apply_rudder_forces() {
    if (!m_world || !m_hull_body.valid()) return;
    if (std::abs(m_settings.rudder) < 0.01f) return;
    if (m_state.speed < 0.5f) return;  // Need forward motion for rudder effect

    Quat rot = get_rotation();
    Vec3 pos = get_position();
    Vec3 forward = quat_rotate(rot, Vec3{0.0f, 0.0f, 1.0f});
    Vec3 right = quat_rotate(rot, Vec3{1.0f, 0.0f, 0.0f});

    Vec3 total_rudder_force{0.0f};

    for (auto& rudder : m_settings.rudders) {
        // Update rudder angle towards target
        float target_angle = m_settings.rudder * rudder.max_angle;
        // Instant for now - could add rate limiting
        m_state.current_rudder_angle = target_angle;

        // Rudder force: F = 0.5 * ρ * Cl * A * v² * sin(angle)
        // Simplified lateral force proportional to angle and speed
        float water_density = 1000.0f;  // Assume standard water
        float speed_sq = m_state.speed * m_state.speed;
        float rudder_force = 0.5f * water_density * rudder.lift_coefficient *
            rudder.area * speed_sq * std::sin(m_state.current_rudder_angle);

        // Apply lateral force
        Vec3 force = Vec3{
            right.x * rudder_force,
            0.0f,
            right.z * rudder_force
        };

        // Apply at rudder position
        Vec3 world_rudder_pos = pos + quat_rotate(rot, rudder.position);
        m_world->add_force_at_point(m_hull_body, force, world_rudder_pos);

        total_rudder_force.x += force.x;
        total_rudder_force.y += force.y;
        total_rudder_force.z += force.z;
    }

    m_state.rudder_force = total_rudder_force;
}

void Boat::apply_arcade_stability() {
    if (!m_world || !m_hull_body.valid()) return;

    const auto& arcade = m_settings.arcade;
    Quat rot = get_rotation();

    float roll, pitch, yaw;
    quat_to_euler(rot, roll, pitch, yaw);

    Vec3 angular_vel = m_world->get_angular_velocity(m_hull_body);

    // Dampen roll
    float roll_correction = -roll * arcade.stability_roll * 50.0f;
    roll_correction -= angular_vel.z * arcade.stability_roll * 10.0f;

    // Dampen pitch
    float pitch_correction = -pitch * arcade.stability_pitch * 50.0f;
    pitch_correction -= angular_vel.x * arcade.stability_pitch * 10.0f;

    Vec3 stability_torque{pitch_correction, 0.0f, roll_correction};
    m_world->add_torque(m_hull_body, stability_torque);
}

void Boat::update_state_from_physics() {
    if (!m_world || !m_hull_body.valid()) return;

    Vec3 velocity = m_world->get_linear_velocity(m_hull_body);
    m_state.velocity = velocity;

    Quat rot = get_rotation();
    Vec3 forward = quat_rotate(rot, Vec3{0.0f, 0.0f, 1.0f});
    Vec3 right = quat_rotate(rot, Vec3{1.0f, 0.0f, 0.0f});

    m_state.speed = vec3_dot(velocity, forward);
    m_state.lateral_speed = vec3_dot(velocity, right);

    m_state.angular_velocity = m_world->get_angular_velocity(m_hull_body);

    float roll, pitch, yaw;
    quat_to_euler(rot, roll, pitch, yaw);
    m_state.roll = roll;
    m_state.pitch = pitch;
    m_state.heading = yaw;
}

void Boat::check_capsize() {
    m_state.is_capsized = (std::abs(m_state.roll) > CAPSIZE_ANGLE ||
                          std::abs(m_state.pitch) > CAPSIZE_ANGLE);
}

void Boat::check_grounded(const WaterVolume* water) {
    // Simple ground check - if very low velocity and below water surface significantly
    // This is a placeholder - proper implementation would use collision detection
    Vec3 pos = get_position();
    float speed = vec3_length(m_state.velocity);

    if (water) {
        float surface = water->get_surface_height_at(pos);
        float depth = surface - pos.y;

        // Consider grounded if deep underwater and nearly stopped
        if (depth > m_settings.hull.hull_half_extents.y * 3.0f && speed < 0.1f) {
            m_state.is_grounded = true;
        } else {
            m_state.is_grounded = false;
        }

        // Check sinking
        m_state.is_sinking = (m_state.submerged_fraction > 0.9f && speed < 0.5f);
    } else {
        // Not in water - check if on ground via physics (simplified)
        m_state.is_grounded = (std::abs(m_state.velocity.y) < 0.1f && speed < 0.5f);
    }
}

// Factory functions
BoatComponent make_small_motorboat() {
    BoatComponent boat;
    boat.mode = BoatMode::Arcade;

    boat.hull.hull_shape = BoxShapeSettings{Vec3{1.0f, 0.3f, 2.5f}};
    boat.hull.hull_mass = 500.0f;
    boat.hull.hull_half_extents = Vec3{1.0f, 0.3f, 2.5f};
    boat.hull.center_of_mass_offset = Vec3{0.0f, -0.2f, 0.0f};

    // Single outboard motor
    PropellerSettings prop;
    prop.position = Vec3{0.0f, -0.2f, -2.0f};
    prop.max_thrust = 10000.0f;
    prop.max_rpm = 5000.0f;
    boat.propellers.push_back(prop);

    // Single rudder (combined with motor)
    RudderSettings rudder;
    rudder.position = Vec3{0.0f, -0.2f, -2.2f};
    rudder.max_angle = 0.6f;
    boat.rudders.push_back(rudder);

    boat.arcade.max_speed = 15.0f;
    boat.arcade.acceleration = 8.0f;
    boat.arcade.turn_speed = 1.5f;

    return boat;
}

BoatComponent make_speedboat() {
    BoatComponent boat;
    boat.mode = BoatMode::Arcade;

    boat.hull.hull_shape = BoxShapeSettings{Vec3{1.5f, 0.4f, 4.0f}};
    boat.hull.hull_mass = 1200.0f;
    boat.hull.hull_half_extents = Vec3{1.5f, 0.4f, 4.0f};
    boat.hull.center_of_mass_offset = Vec3{0.0f, -0.3f, 0.5f};

    // Twin engines
    PropellerSettings prop_left;
    prop_left.position = Vec3{-0.6f, -0.3f, -3.5f};
    prop_left.max_thrust = 30000.0f;
    boat.propellers.push_back(prop_left);

    PropellerSettings prop_right;
    prop_right.position = Vec3{0.6f, -0.3f, -3.5f};
    prop_right.max_thrust = 30000.0f;
    boat.propellers.push_back(prop_right);

    RudderSettings rudder;
    rudder.position = Vec3{0.0f, -0.3f, -3.8f};
    rudder.max_angle = 0.5f;
    rudder.area = 0.3f;
    boat.rudders.push_back(rudder);

    boat.arcade.max_speed = 30.0f;
    boat.arcade.acceleration = 12.0f;
    boat.arcade.turn_speed = 1.2f;
    boat.arcade.drift_factor = 0.85f;

    return boat;
}

BoatComponent make_sailboat() {
    BoatComponent boat;
    boat.mode = BoatMode::Simulation;

    boat.hull.hull_shape = BoxShapeSettings{Vec3{1.2f, 0.8f, 5.0f}};
    boat.hull.hull_mass = 2000.0f;
    boat.hull.hull_half_extents = Vec3{1.2f, 0.8f, 5.0f};
    boat.hull.center_of_mass_offset = Vec3{0.0f, -0.6f, 0.0f};

    // No propeller for sailboat (would need wind simulation)

    RudderSettings rudder;
    rudder.position = Vec3{0.0f, -0.6f, -4.5f};
    rudder.max_angle = 0.7f;
    rudder.area = 0.8f;
    boat.rudders.push_back(rudder);

    return boat;
}

BoatComponent make_cargo_ship() {
    BoatComponent boat;
    boat.mode = BoatMode::Simulation;

    boat.hull.hull_shape = BoxShapeSettings{Vec3{8.0f, 4.0f, 30.0f}};
    boat.hull.hull_mass = 50000.0f;
    boat.hull.hull_half_extents = Vec3{8.0f, 4.0f, 30.0f};
    boat.hull.center_of_mass_offset = Vec3{0.0f, -2.0f, 2.0f};
    boat.hull.hull_drag_coefficient = 0.2f;

    // Single large propeller
    PropellerSettings prop;
    prop.position = Vec3{0.0f, -3.0f, -28.0f};
    prop.max_thrust = 500000.0f;
    prop.max_rpm = 120.0f;
    prop.propeller_radius = 3.0f;
    prop.spin_up_time = 10.0f;
    prop.spin_down_time = 30.0f;
    boat.propellers.push_back(prop);

    RudderSettings rudder;
    rudder.position = Vec3{0.0f, -3.0f, -29.0f};
    rudder.max_angle = 0.6f;
    rudder.area = 20.0f;
    rudder.turn_rate = 0.1f;
    boat.rudders.push_back(rudder);

    return boat;
}

} // namespace engine::physics
