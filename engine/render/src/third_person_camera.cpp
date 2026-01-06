#include <engine/render/third_person_camera.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <engine/core/log.hpp>
#include <engine/reflect/type_registry.hpp>
#include <cmath>
#include <algorithm>

namespace engine::render {

namespace {

Vec3 get_entity_position(scene::World& world, scene::Entity entity) {
    auto* world_transform = world.try_get<scene::WorldTransform>(entity);
    if (world_transform) {
        return world_transform->get_position();
    }
    auto* local_transform = world.try_get<scene::LocalTransform>(entity);
    if (local_transform) {
        return local_transform->position;
    }
    return Vec3(0.0f);
}

Vec3 get_entity_forward(scene::World& world, scene::Entity entity) {
    auto* world_transform = world.try_get<scene::WorldTransform>(entity);
    if (world_transform) {
        return -Vec3(world_transform->matrix[2][0],
                     world_transform->matrix[2][1],
                     world_transform->matrix[2][2]);
    }
    auto* local_transform = world.try_get<scene::LocalTransform>(entity);
    if (local_transform) {
        return local_transform->forward();
    }
    return Vec3(0.0f, 0.0f, -1.0f);
}

// Smooth damp (exponential decay)
Vec3 smooth_damp(const Vec3& current, const Vec3& target, Vec3& velocity,
                 float smoothTime, float dt) {
    if (smoothTime <= 0.0f) return target;

    float omega = 2.0f / smoothTime;
    float x = omega * dt;
    float exp_factor = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

    Vec3 change = current - target;
    Vec3 temp = (velocity + omega * change) * dt;
    velocity = (velocity - omega * temp) * exp_factor;

    Vec3 result = target + (change + temp) * exp_factor;

    // Prevent overshooting
    if (glm::dot(target - current, result - target) > 0.0f) {
        result = target;
        velocity = Vec3(0.0f);
    }

    return result;
}

float smooth_damp_float(float current, float target, float& velocity,
                        float smoothTime, float dt) {
    if (smoothTime <= 0.0f) return target;

    float omega = 2.0f / smoothTime;
    float x = omega * dt;
    float exp_factor = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

    float change = current - target;
    float temp = (velocity + omega * change) * dt;
    velocity = (velocity - omega * temp) * exp_factor;

    float result = target + (change + temp) * exp_factor;

    if ((target - current > 0.0f) == (result > target)) {
        result = target;
        velocity = 0.0f;
    }

    return result;
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Vec3 lerp_vec3(const Vec3& a, const Vec3& b, float t) {
    return a + (b - a) * t;
}

// Spherical linear interpolation for quaternions
Quat slerp(const Quat& a, const Quat& b, float t) {
    return glm::slerp(a, b, t);
}

// Wrap angle to -180 to 180
float wrap_angle(float angle) {
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

} // anonymous namespace

// ============================================================================
// Default Presets
// ============================================================================

CameraPreset ThirdPersonCameraSystem::get_default_centered_preset() {
    CameraPreset preset;
    preset.offset = Vec3(0.0f, 1.5f, 0.0f);     // Centered behind
    preset.distance = 4.0f;
    preset.fov = 60.0f;
    preset.pitch_min = -60.0f;
    preset.pitch_max = 60.0f;
    preset.position_smoothing = 0.1f;
    preset.rotation_smoothing = 0.05f;
    preset.collision_radius = 0.3f;
    preset.collision_recovery_speed = 10.0f;
    return preset;
}

CameraPreset ThirdPersonCameraSystem::get_default_over_shoulder_preset() {
    CameraPreset preset;
    preset.offset = Vec3(0.6f, 1.4f, 0.0f);     // Offset to side
    preset.distance = 3.5f;
    preset.fov = 55.0f;
    preset.pitch_min = -45.0f;
    preset.pitch_max = 45.0f;
    preset.position_smoothing = 0.08f;
    preset.rotation_smoothing = 0.04f;
    preset.collision_radius = 0.25f;
    preset.collision_recovery_speed = 12.0f;
    return preset;
}

CameraPreset ThirdPersonCameraSystem::get_default_aiming_preset() {
    CameraPreset preset;
    preset.offset = Vec3(0.8f, 1.2f, 0.0f);     // Tighter to shoulder
    preset.distance = 2.5f;
    preset.fov = 50.0f;
    preset.pitch_min = -30.0f;
    preset.pitch_max = 30.0f;
    preset.position_smoothing = 0.05f;
    preset.rotation_smoothing = 0.02f;
    preset.collision_radius = 0.2f;
    preset.collision_recovery_speed = 15.0f;
    return preset;
}

CameraPreset ThirdPersonCameraSystem::get_default_lock_on_preset() {
    CameraPreset preset;
    preset.offset = Vec3(0.3f, 1.6f, 0.0f);     // Slightly offset
    preset.distance = 5.0f;
    preset.fov = 55.0f;
    preset.pitch_min = -45.0f;
    preset.pitch_max = 60.0f;
    preset.position_smoothing = 0.12f;
    preset.rotation_smoothing = 0.08f;
    preset.collision_radius = 0.3f;
    preset.collision_recovery_speed = 8.0f;
    return preset;
}

// ============================================================================
// Third Person Camera System Implementation
// ============================================================================

ThirdPersonCameraSystem::ThirdPersonCameraSystem() {
    // Default collision check - no collision
    m_collision_check = [this](const Vec3& from, const Vec3& to, float radius, uint32_t mask) {
        return default_collision_check(from, to, radius, mask);
    };
}

ThirdPersonCameraSystem& ThirdPersonCameraSystem::instance() {
    static ThirdPersonCameraSystem s_instance;
    return s_instance;
}

void ThirdPersonCameraSystem::apply_look_input(scene::World& world, scene::Entity camera,
                                                float delta_x, float delta_y) {
    auto* cam = world.try_get<ThirdPersonCameraComponent>(camera);
    if (!cam) return;

    // Don't allow free look during lock-on
    if (cam->mode == ThirdPersonCameraMode::LockOn && cam->lock_on_target != scene::NullEntity) {
        return;
    }

    cam->yaw += delta_x * cam->sensitivity_x;
    cam->yaw = wrap_angle(cam->yaw);

    float pitch_delta = delta_y * cam->sensitivity_y;
    if (cam->invert_y) pitch_delta = -pitch_delta;

    cam->pitch += pitch_delta;
    cam->pitch = glm::clamp(cam->pitch, cam->active_preset.pitch_min, cam->active_preset.pitch_max);
}

void ThirdPersonCameraSystem::set_mode(scene::World& world, scene::Entity camera,
                                        ThirdPersonCameraMode mode) {
    auto* cam = world.try_get<ThirdPersonCameraComponent>(camera);
    if (!cam) return;

    if (cam->mode == mode) return;

    ThirdPersonCameraMode old_mode = cam->mode;
    cam->set_mode(mode);

    // Emit event
    CameraModeChangedEvent event;
    event.camera_entity = camera;
    event.old_mode = old_mode;
    event.new_mode = mode;
    core::EventDispatcher::instance().dispatch(event);
}

ThirdPersonCameraMode ThirdPersonCameraSystem::get_mode(scene::World& world, scene::Entity camera) const {
    auto* cam = world.try_get<ThirdPersonCameraComponent>(camera);
    return cam ? cam->mode : ThirdPersonCameraMode::Centered;
}

void ThirdPersonCameraSystem::toggle_shoulder_mode(scene::World& world, scene::Entity camera) {
    auto* cam = world.try_get<ThirdPersonCameraComponent>(camera);
    if (!cam) return;

    if (cam->mode == ThirdPersonCameraMode::Centered) {
        set_mode(world, camera, ThirdPersonCameraMode::OverShoulder);
    } else if (cam->mode == ThirdPersonCameraMode::OverShoulder) {
        set_mode(world, camera, ThirdPersonCameraMode::Centered);
    }
}

void ThirdPersonCameraSystem::switch_shoulder(scene::World& world, scene::Entity camera) {
    auto* cam = world.try_get<ThirdPersonCameraComponent>(camera);
    if (!cam) return;

    cam->switch_shoulder();
}

void ThirdPersonCameraSystem::set_lock_on_target(scene::World& world, scene::Entity camera,
                                                  scene::Entity target) {
    auto* cam = world.try_get<ThirdPersonCameraComponent>(camera);
    if (!cam) return;

    cam->lock_on_target = target;

    if (target != scene::NullEntity) {
        cam->set_mode(ThirdPersonCameraMode::LockOn);
        cam->lock_on_target_position = get_entity_position(world, target);

        CameraLockOnStartedEvent event;
        event.camera_entity = camera;
        event.target_entity = target;
        core::EventDispatcher::instance().dispatch(event);
    }
}

void ThirdPersonCameraSystem::clear_lock_on_target(scene::World& world, scene::Entity camera) {
    auto* cam = world.try_get<ThirdPersonCameraComponent>(camera);
    if (!cam) return;

    if (cam->lock_on_target != scene::NullEntity) {
        cam->lock_on_target = scene::NullEntity;

        // Return to previous mode
        cam->set_mode(cam->previous_mode != ThirdPersonCameraMode::LockOn ?
                      cam->previous_mode : ThirdPersonCameraMode::Centered);

        CameraLockOnEndedEvent event;
        event.camera_entity = camera;
        core::EventDispatcher::instance().dispatch(event);
    }
}

Mat4 ThirdPersonCameraSystem::get_view_matrix(scene::World& world, scene::Entity camera) const {
    auto* cam = world.try_get<ThirdPersonCameraComponent>(camera);
    if (!cam) return Mat4(1.0f);

    return glm::lookAt(cam->current_position,
                       cam->current_position + glm::normalize(cam->current_rotation * Vec3(0.0f, 0.0f, -1.0f)),
                       Vec3(0.0f, 1.0f, 0.0f));
}

Vec3 ThirdPersonCameraSystem::get_camera_position(scene::World& world, scene::Entity camera) const {
    auto* cam = world.try_get<ThirdPersonCameraComponent>(camera);
    return cam ? cam->current_position : Vec3(0.0f);
}

Vec3 ThirdPersonCameraSystem::get_camera_forward(scene::World& world, scene::Entity camera) const {
    auto* cam = world.try_get<ThirdPersonCameraComponent>(camera);
    if (!cam) return Vec3(0.0f, 0.0f, -1.0f);

    return glm::normalize(cam->current_rotation * Vec3(0.0f, 0.0f, -1.0f));
}

Vec3 ThirdPersonCameraSystem::get_aim_direction(scene::World& world, scene::Entity camera) const {
    auto* cam = world.try_get<ThirdPersonCameraComponent>(camera);
    if (!cam) return Vec3(0.0f, 0.0f, -1.0f);

    // During lock-on, aim at target
    if (cam->mode == ThirdPersonCameraMode::LockOn && cam->lock_on_target != scene::NullEntity) {
        Vec3 target_pos = get_entity_position(world, cam->lock_on_target);
        return glm::normalize(target_pos - cam->current_position);
    }

    return get_camera_forward(world, camera);
}

void ThirdPersonCameraSystem::set_collision_check(CameraCollisionCheck check) {
    m_collision_check = std::move(check);
}

void ThirdPersonCameraSystem::update_camera(scene::World& world, scene::Entity camera,
                                             ThirdPersonCameraComponent& cam, float dt) {
    // Validate target
    if (cam.target_entity == scene::NullEntity || !world.valid(cam.target_entity)) {
        return;
    }

    // Update transition
    if (cam.transition_progress < 1.0f) {
        cam.transition_progress += dt / cam.transition_duration;
        cam.transition_progress = std::min(cam.transition_progress, 1.0f);

        // Interpolate active preset
        const CameraPreset& from_preset = cam.get_preset_for_mode(cam.previous_mode);
        const CameraPreset& to_preset = cam.get_preset_for_mode(cam.mode);
        cam.active_preset = interpolate_presets(from_preset, to_preset, cam.transition_progress);
    } else {
        cam.active_preset = cam.get_preset_for_mode(cam.mode);
    }

    // Get target position
    Vec3 target_pos = get_entity_position(world, cam.target_entity);

    // Calculate pivot point
    Vec3 pivot = calculate_pivot(world, cam.target_entity, cam.active_preset, cam.shoulder_side);

    // Smooth pivot follow
    static Vec3 pivot_velocity(0.0f);
    cam.pivot_position = smooth_damp(cam.pivot_position, pivot, pivot_velocity,
                                      cam.active_preset.position_smoothing, dt);

    // Handle lock-on camera
    if (cam.mode == ThirdPersonCameraMode::LockOn && cam.lock_on_target != scene::NullEntity) {
        // Update lock-on target position
        cam.lock_on_target_position = get_entity_position(world, cam.lock_on_target);

        // Calculate direction to look at both player and target
        Vec3 midpoint = (target_pos + cam.lock_on_target_position) * 0.5f;
        Vec3 to_midpoint = midpoint - cam.pivot_position;

        // Calculate yaw to face midpoint
        cam.yaw = glm::degrees(std::atan2(to_midpoint.x, to_midpoint.z));

        // Calculate pitch based on vertical difference
        float horizontal_dist = glm::length(Vec2(to_midpoint.x, to_midpoint.z));
        if (horizontal_dist > 0.1f) {
            cam.pitch = glm::degrees(std::atan2(-to_midpoint.y, horizontal_dist));
            cam.pitch = glm::clamp(cam.pitch, cam.active_preset.pitch_min, cam.active_preset.pitch_max);
        }
    }

    // Calculate ideal camera position
    Vec3 ideal_pos = calculate_ideal_position(cam.pivot_position, cam.pitch, cam.yaw,
                                               cam.active_preset.distance,
                                               cam.active_preset.offset, cam.shoulder_side);

    // Handle collision
    float collision_factor = 1.0f;
    if (m_collision_enabled) {
        collision_factor = handle_collision(cam.pivot_position, ideal_pos,
                                            cam.active_preset.collision_radius,
                                            cam.collision_layer_mask);
    }

    // Apply collision to distance
    float desired_distance = cam.active_preset.distance * collision_factor;

    if (collision_factor < 1.0f) {
        cam.collision_active = true;
        cam.collision_distance = desired_distance;
    } else {
        cam.collision_active = false;
        // Recover distance smoothly
        static float dist_velocity = 0.0f;
        cam.collision_distance = smooth_damp_float(cam.collision_distance, cam.active_preset.distance,
                                                    dist_velocity,
                                                    1.0f / cam.active_preset.collision_recovery_speed, dt);
    }

    float final_distance = std::min(cam.collision_distance, desired_distance);

    // Recalculate position with collision-adjusted distance
    Vec3 camera_pos = calculate_ideal_position(cam.pivot_position, cam.pitch, cam.yaw,
                                                final_distance, cam.active_preset.offset, cam.shoulder_side);

    // Smooth camera position
    cam.current_position = smooth_damp(cam.current_position, camera_pos, cam.velocity,
                                        cam.active_preset.position_smoothing, dt);

    // Calculate rotation
    Quat target_rotation;
    if (cam.mode == ThirdPersonCameraMode::LockOn && cam.lock_on_target != scene::NullEntity) {
        // Look at midpoint between player and target
        Vec3 midpoint = (target_pos + cam.lock_on_target_position) * 0.5f;
        target_rotation = calculate_lock_on_rotation(cam.current_position, midpoint);
    } else {
        // Standard rotation from pitch/yaw
        Quat pitch_rot = glm::angleAxis(glm::radians(-cam.pitch), Vec3(1.0f, 0.0f, 0.0f));
        Quat yaw_rot = glm::angleAxis(glm::radians(cam.yaw), Vec3(0.0f, 1.0f, 0.0f));
        target_rotation = yaw_rot * pitch_rot;
    }

    // Smooth rotation
    cam.current_rotation = slerp(cam.current_rotation, target_rotation,
                                  1.0f - std::pow(cam.active_preset.rotation_smoothing, dt * 60.0f));

    // Update current state
    cam.current_distance = final_distance;
    cam.current_fov = cam.active_preset.fov;
}

Vec3 ThirdPersonCameraSystem::calculate_pivot(scene::World& world, scene::Entity target,
                                               const CameraPreset& preset, float shoulder_side) {
    Vec3 target_pos = get_entity_position(world, target);

    // Apply offset (up component is always world-up)
    Vec3 pivot = target_pos;
    pivot.y += preset.offset.y;

    return pivot;
}

Vec3 ThirdPersonCameraSystem::calculate_ideal_position(const Vec3& pivot, float pitch, float yaw,
                                                        float distance, const Vec3& offset, float shoulder_side) {
    // Convert to radians
    float pitch_rad = glm::radians(pitch);
    float yaw_rad = glm::radians(yaw);

    // Calculate direction from angles
    Vec3 direction;
    direction.x = std::sin(yaw_rad) * std::cos(pitch_rad);
    direction.y = std::sin(pitch_rad);
    direction.z = std::cos(yaw_rad) * std::cos(pitch_rad);

    // Camera position behind the pivot
    Vec3 camera_pos = pivot - direction * distance;

    // Apply horizontal offset (shoulder offset)
    Vec3 right = glm::cross(direction, Vec3(0.0f, 1.0f, 0.0f));
    if (glm::length(right) > 0.001f) {
        right = glm::normalize(right);
        camera_pos += right * offset.x * shoulder_side;
    }

    return camera_pos;
}

float ThirdPersonCameraSystem::handle_collision(const Vec3& pivot, const Vec3& ideal_pos,
                                                 float radius, uint32_t layer_mask) {
    if (!m_collision_check) return 1.0f;

    return m_collision_check(pivot, ideal_pos, radius, layer_mask);
}

CameraPreset ThirdPersonCameraSystem::interpolate_presets(const CameraPreset& from,
                                                           const CameraPreset& to, float t) {
    CameraPreset result;
    result.offset = lerp_vec3(from.offset, to.offset, t);
    result.distance = lerp(from.distance, to.distance, t);
    result.fov = lerp(from.fov, to.fov, t);
    result.pitch_min = lerp(from.pitch_min, to.pitch_min, t);
    result.pitch_max = lerp(from.pitch_max, to.pitch_max, t);
    result.position_smoothing = lerp(from.position_smoothing, to.position_smoothing, t);
    result.rotation_smoothing = lerp(from.rotation_smoothing, to.rotation_smoothing, t);
    result.collision_radius = lerp(from.collision_radius, to.collision_radius, t);
    result.collision_recovery_speed = lerp(from.collision_recovery_speed, to.collision_recovery_speed, t);
    return result;
}

Quat ThirdPersonCameraSystem::calculate_lock_on_rotation(const Vec3& camera_pos, const Vec3& target_pos) {
    Vec3 direction = glm::normalize(target_pos - camera_pos);

    // Calculate yaw and pitch from direction
    float yaw = std::atan2(direction.x, direction.z);
    float pitch = -std::asin(direction.y);

    Quat pitch_rot = glm::angleAxis(pitch, Vec3(1.0f, 0.0f, 0.0f));
    Quat yaw_rot = glm::angleAxis(yaw, Vec3(0.0f, 1.0f, 0.0f));

    return yaw_rot * pitch_rot;
}

float ThirdPersonCameraSystem::default_collision_check(const Vec3& from, const Vec3& to,
                                                        float radius, uint32_t layer_mask) {
    // Default: no collision, return full distance
    return 1.0f;
}

// ============================================================================
// ECS System
// ============================================================================

void third_person_camera_system(scene::World& world, double dt) {
    auto& system = ThirdPersonCameraSystem::instance();

    auto view = world.view<ThirdPersonCameraComponent>();

    for (auto entity : view) {
        auto& cam = view.get<ThirdPersonCameraComponent>(entity);
        system.update_camera(world, entity, cam, static_cast<float>(dt));
    }
}

// ============================================================================
// Component Registration
// ============================================================================

void register_third_person_camera_components() {
    using namespace reflect;

    // ThirdPersonCameraComponent
    TypeRegistry::instance().register_component<ThirdPersonCameraComponent>("ThirdPersonCameraComponent")
        .display_name("Third Person Camera")
        .category("Camera");

    TypeRegistry::instance().register_property<ThirdPersonCameraComponent>("sensitivity_x",
        [](const ThirdPersonCameraComponent& c) { return c.sensitivity_x; },
        [](ThirdPersonCameraComponent& c, float v) { c.sensitivity_x = v; })
        .display_name("Sensitivity X").min(0.1f).max(10.0f);

    TypeRegistry::instance().register_property<ThirdPersonCameraComponent>("sensitivity_y",
        [](const ThirdPersonCameraComponent& c) { return c.sensitivity_y; },
        [](ThirdPersonCameraComponent& c, float v) { c.sensitivity_y = v; })
        .display_name("Sensitivity Y").min(0.1f).max(10.0f);

    TypeRegistry::instance().register_property<ThirdPersonCameraComponent>("invert_y",
        [](const ThirdPersonCameraComponent& c) { return c.invert_y; },
        [](ThirdPersonCameraComponent& c, bool v) { c.invert_y = v; })
        .display_name("Invert Y");

    TypeRegistry::instance().register_property<ThirdPersonCameraComponent>("transition_duration",
        [](const ThirdPersonCameraComponent& c) { return c.transition_duration; },
        [](ThirdPersonCameraComponent& c, float v) { c.transition_duration = v; })
        .display_name("Transition Duration").min(0.1f).max(2.0f);

    core::log(core::LogLevel::Info, "Third person camera components registered");
}

} // namespace engine::render
