#include <engine/physics/character_controller.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <cmath>

namespace engine::physics {

// Simple implementation without Jolt internals
// A full implementation would use Jolt's CharacterVirtual class

struct CharacterController::Impl {
    // Jolt-specific data would go here
    // For now, we use a simplified physics simulation
};

CharacterController::CharacterController()
    : m_impl(std::make_unique<Impl>())
{
}

CharacterController::~CharacterController() {
    shutdown();
}

CharacterController::CharacterController(CharacterController&&) noexcept = default;
CharacterController& CharacterController::operator=(CharacterController&&) noexcept = default;

void CharacterController::init(PhysicsWorld& world, const CharacterSettings& settings) {
    m_world = &world;
    m_settings = settings;
    m_position = settings.position;
    m_rotation = settings.rotation;
    m_initialized = true;

    core::log(core::LogLevel::Debug, "CharacterController initialized at ({}, {}, {})",
              m_position.x, m_position.y, m_position.z);
}

void CharacterController::shutdown() {
    if (!m_initialized) return;

    m_world = nullptr;
    m_initialized = false;
}

bool CharacterController::is_initialized() const {
    return m_initialized;
}

void CharacterController::set_position(const Vec3& pos) {
    m_position = pos;
}

Vec3 CharacterController::get_position() const {
    return m_position;
}

void CharacterController::set_rotation(const Quat& rot) {
    m_rotation = rot;
}

Quat CharacterController::get_rotation() const {
    return m_rotation;
}

void CharacterController::set_movement_input(const Vec3& direction) {
    m_movement_input = direction;
    // Clamp magnitude to 1
    float len = glm::length(m_movement_input);
    if (len > 1.0f) {
        m_movement_input /= len;
    }
}

void CharacterController::set_movement_input(float x, float z) {
    set_movement_input(Vec3{x, 0.0f, z});
}

void CharacterController::jump(float impulse) {
    m_jump_requested = true;
    m_jump_impulse = impulse;
    m_time_since_jump_pressed = 0.0f;
}

bool CharacterController::can_jump() const {
    if (!m_enabled) return false;

    // Can jump if grounded or within coyote time
    if (m_ground_state.on_ground) return true;
    if (m_ground_state.time_since_grounded < m_coyote_time && !m_has_jumped) return true;

    return false;
}

void CharacterController::update(float dt) {
    if (!m_initialized || !m_enabled || !m_world) return;

    // Update ground state
    update_ground_state(dt);

    // Handle jump buffering
    m_time_since_jump_pressed += dt;

    // Check for buffered jump
    if (m_time_since_jump_pressed < m_jump_buffer_time && can_jump()) {
        m_jump_requested = true;
    }

    // Execute jump if requested and allowed
    if (m_jump_requested && can_jump()) {
        m_velocity.y = m_jump_impulse;
        m_has_jumped = true;
        m_jump_requested = false;
        m_ground_state.on_ground = false;
    }
    m_jump_requested = false;

    // Apply movement
    apply_movement(dt);

    // Apply gravity
    apply_gravity(dt);

    // Handle step up for stairs/small obstacles
    handle_step_up();

    // Update position
    m_position += m_velocity * dt;

    // Ground clamping when on ground
    if (m_ground_state.on_ground && m_velocity.y <= 0.0f) {
        // Project position onto ground
        // In a full implementation, this would use the ground contact point
    }

    // Reset jump state when landing
    if (m_ground_state.on_ground && !m_ground_state.was_on_ground) {
        m_has_jumped = false;
    }
}

const GroundState& CharacterController::get_ground_state() const {
    return m_ground_state;
}

bool CharacterController::is_grounded() const {
    return m_ground_state.on_ground;
}

Vec3 CharacterController::get_velocity() const {
    return m_velocity;
}

Vec3 CharacterController::get_linear_velocity() const {
    return m_velocity;
}

void CharacterController::set_velocity(const Vec3& vel) {
    m_velocity = vel;
}

void CharacterController::add_velocity(const Vec3& vel) {
    m_velocity += vel;
}

void CharacterController::set_movement_speed(float speed) {
    m_movement_speed = speed;
}

float CharacterController::get_movement_speed() const {
    return m_movement_speed;
}

void CharacterController::set_jump_impulse(float impulse) {
    m_jump_impulse = impulse;
}

float CharacterController::get_jump_impulse() const {
    return m_jump_impulse;
}

void CharacterController::set_gravity_scale(float scale) {
    m_gravity_scale = scale;
}

float CharacterController::get_gravity_scale() const {
    return m_gravity_scale;
}

void CharacterController::set_air_control(float control) {
    m_air_control = glm::clamp(control, 0.0f, 1.0f);
}

float CharacterController::get_air_control() const {
    return m_air_control;
}

void CharacterController::set_friction(float friction) {
    m_friction = friction;
}

float CharacterController::get_friction() const {
    return m_friction;
}

void CharacterController::set_air_friction(float friction) {
    m_air_friction = friction;
}

float CharacterController::get_air_friction() const {
    return m_air_friction;
}

void CharacterController::set_acceleration(float accel) {
    m_acceleration = accel;
}

float CharacterController::get_acceleration() const {
    return m_acceleration;
}

void CharacterController::set_deceleration(float decel) {
    m_deceleration = decel;
}

float CharacterController::get_deceleration() const {
    return m_deceleration;
}

void CharacterController::set_enabled(bool enabled) {
    m_enabled = enabled;
}

bool CharacterController::is_enabled() const {
    return m_enabled;
}

void CharacterController::teleport(const Vec3& position, const Quat& rotation) {
    m_position = position;
    m_rotation = rotation;
    m_velocity = Vec3{0.0f};
    refresh_ground_state();
}

void CharacterController::refresh_ground_state() {
    update_ground_state(0.016f);  // Default dt for manual refresh
}

void CharacterController::update_ground_state(float dt) {
    if (!m_world) return;

    m_ground_state.was_on_ground = m_ground_state.on_ground;

    // Raycast down to detect ground
    float cast_distance = m_settings.skin_width + 0.1f;
    Vec3 cast_origin = m_position;
    cast_origin.y += m_settings.radius;  // Start from bottom of capsule

    RaycastHit hit = m_world->raycast(cast_origin, Vec3{0.0f, -1.0f, 0.0f}, cast_distance);

    if (hit.hit) {
        m_ground_state.ground_normal = hit.normal;
        m_ground_state.ground_point = hit.point;
        m_ground_state.ground_body = hit.body;

        // Calculate slope angle
        float dot = glm::dot(hit.normal, Vec3{0.0f, 1.0f, 0.0f});
        m_ground_state.slope_angle = std::acos(glm::clamp(dot, -1.0f, 1.0f));

        float max_slope_rad = glm::radians(m_settings.max_slope_angle);

        if (m_ground_state.slope_angle <= max_slope_rad) {
            m_ground_state.on_ground = true;
            m_ground_state.on_slope = m_ground_state.slope_angle > 0.01f;
            m_ground_state.sliding = false;
            m_ground_state.time_since_grounded = 0.0f;
        } else {
            // Too steep - sliding
            m_ground_state.on_ground = false;
            m_ground_state.on_slope = true;
            m_ground_state.sliding = true;
        }
    } else {
        m_ground_state.on_ground = false;
        m_ground_state.on_slope = false;
        m_ground_state.sliding = false;
        m_ground_state.ground_normal = Vec3{0.0f, 1.0f, 0.0f};
    }

    // Update time since grounded
    if (!m_ground_state.on_ground) {
        m_ground_state.time_since_grounded += dt;
    }
}

void CharacterController::apply_movement(float dt) {
    // Transform input to world space based on rotation
    Vec3 forward = m_rotation * Vec3{0.0f, 0.0f, -1.0f};
    Vec3 right = m_rotation * Vec3{1.0f, 0.0f, 0.0f};

    // Project to horizontal plane
    forward.y = 0.0f;
    right.y = 0.0f;
    forward = glm::normalize(forward);
    right = glm::normalize(right);

    // Calculate desired velocity
    Vec3 desired_velocity = (right * m_movement_input.x + forward * m_movement_input.z) * m_movement_speed;

    // Get current horizontal velocity
    Vec3 current_horizontal{m_velocity.x, 0.0f, m_velocity.z};

    float control = m_ground_state.on_ground ? 1.0f : m_air_control;
    float friction = m_ground_state.on_ground ? m_friction : m_air_friction;

    // Apply friction/deceleration when no input
    if (glm::length(m_movement_input) < 0.01f) {
        float decel = m_deceleration * dt * control;
        float speed = glm::length(current_horizontal);
        if (speed > 0.0f) {
            float new_speed = std::max(0.0f, speed - decel);
            current_horizontal = glm::normalize(current_horizontal) * new_speed;
        }
    } else {
        // Accelerate towards desired velocity
        Vec3 velocity_diff = desired_velocity - current_horizontal;
        float accel = m_acceleration * dt * control;

        if (glm::length(velocity_diff) > accel) {
            velocity_diff = glm::normalize(velocity_diff) * accel;
        }

        current_horizontal += velocity_diff;
    }

    // Apply friction
    float friction_force = friction * dt;
    float horizontal_speed = glm::length(current_horizontal);
    if (horizontal_speed > friction_force && m_ground_state.on_ground && glm::length(m_movement_input) < 0.01f) {
        current_horizontal -= glm::normalize(current_horizontal) * friction_force;
    }

    // Update velocity
    m_velocity.x = current_horizontal.x;
    m_velocity.z = current_horizontal.z;

    // Handle sliding on steep slopes
    if (m_ground_state.sliding) {
        Vec3 slide_dir = m_ground_state.ground_normal;
        slide_dir.y = 0.0f;
        if (glm::length(slide_dir) > 0.01f) {
            slide_dir = glm::normalize(slide_dir);
            m_velocity += slide_dir * 9.81f * dt;  // Slide acceleration
        }
    }
}

void CharacterController::apply_gravity(float dt) {
    if (!m_world) return;

    if (!m_ground_state.on_ground) {
        Vec3 gravity = m_world->get_gravity() * m_gravity_scale;
        m_velocity += gravity * dt;
    } else if (m_velocity.y < 0.0f) {
        // Snap to ground
        m_velocity.y = 0.0f;
    }
}

void CharacterController::handle_step_up() {
    // Simplified step-up handling
    // A full implementation would cast a shape forward, then up, then down
    // to detect and climb small steps

    if (!m_ground_state.on_ground || !m_world) return;

    Vec3 move_dir{m_velocity.x, 0.0f, m_velocity.z};
    float move_speed = glm::length(move_dir);

    if (move_speed < 0.01f) return;

    move_dir /= move_speed;

    // Cast forward to check for obstacle
    RaycastHit forward_hit = m_world->raycast(
        m_position + Vec3{0.0f, m_settings.step_height * 0.5f, 0.0f},
        move_dir,
        m_settings.radius + 0.1f
    );

    if (forward_hit.hit) {
        // Cast from step height to check if we can step up
        Vec3 step_origin = m_position + Vec3{0.0f, m_settings.step_height + 0.05f, 0.0f} + move_dir * (m_settings.radius + 0.1f);
        RaycastHit down_hit = m_world->raycast(step_origin, Vec3{0.0f, -1.0f, 0.0f}, m_settings.step_height + 0.1f);

        if (down_hit.hit && down_hit.distance < m_settings.step_height) {
            // Can step up - adjust position
            float step_up_amount = m_settings.step_height - down_hit.distance + 0.05f;
            m_position.y += step_up_amount;
        }
    }
}

// System function
void character_controller_system(scene::World& world, PhysicsWorld& physics, float dt) {
    // This would iterate over all CharacterControllerComponent entities
    // and call update on each controller

    // auto view = world.view<CharacterControllerComponent, scene::LocalTransform>();
    // for (auto entity : view) {
    //     auto& cc = view.get<CharacterControllerComponent>(entity);
    //     auto& transform = view.get<scene::LocalTransform>(entity);
    //
    //     if (cc.controller) {
    //         cc.controller->update(dt);
    //         transform.position = cc.controller->get_position();
    //     }
    // }
}

} // namespace engine::physics
