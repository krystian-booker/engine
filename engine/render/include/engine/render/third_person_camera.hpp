#pragma once

#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/math.hpp>
#include <functional>
#include <cstdint>

namespace engine::render {

// ============================================================================
// Camera Modes
// ============================================================================

enum class ThirdPersonCameraMode : uint8_t {
    Centered,       // Souls-like: character centered, camera orbits around
    OverShoulder,   // RE4-like: character offset to side, over-the-shoulder view
    Aiming,         // Tighter over-the-shoulder for precision aiming
    LockOn,         // Tracking a locked target
    Transition      // Transitioning between modes
};

// ============================================================================
// Camera Preset
// ============================================================================

// Pre-defined camera offsets for different modes
struct CameraPreset {
    Vec3 offset{0.0f, 1.5f, -4.0f};             // Local offset (right, up, back)
    float distance = 4.0f;                       // Distance from pivot
    float fov = 60.0f;                          // Field of view

    float pitch_min = -60.0f;                   // Look down limit
    float pitch_max = 60.0f;                    // Look up limit

    float position_smoothing = 0.1f;            // Position follow lag (0 = instant)
    float rotation_smoothing = 0.05f;           // Rotation follow lag
    float zoom_smoothing = 0.1f;                // Distance transition smoothing

    // Collision
    float collision_radius = 0.3f;              // Camera collision sphere radius
    float collision_recovery_speed = 10.0f;     // Speed to restore distance after collision
};

// ============================================================================
// Third Person Camera Component
// ============================================================================

struct ThirdPersonCameraComponent {
    // Target entity to follow
    scene::Entity target_entity = scene::NullEntity;

    // Current mode
    ThirdPersonCameraMode mode = ThirdPersonCameraMode::Centered;
    ThirdPersonCameraMode previous_mode = ThirdPersonCameraMode::Centered;

    // Mode presets
    CameraPreset centered_preset;
    CameraPreset over_shoulder_preset;
    CameraPreset aiming_preset;
    CameraPreset lock_on_preset;

    // Active preset (interpolated during transitions)
    CameraPreset active_preset;

    // Player input rotation
    float pitch = 15.0f;                        // Vertical angle (degrees)
    float yaw = 0.0f;                           // Horizontal angle (degrees)

    // Input sensitivity
    float sensitivity_x = 2.0f;
    float sensitivity_y = 1.5f;
    bool invert_y = false;

    // Current camera state
    Vec3 current_position{0.0f};
    Quat current_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float current_distance = 4.0f;
    float current_fov = 60.0f;

    // Smoothed state (for interpolation)
    Vec3 pivot_position{0.0f};                  // Where camera orbits around
    Vec3 velocity{0.0f};                        // For smooth damp

    // Collision state
    bool collision_active = false;
    float collision_distance = 0.0f;            // Current distance due to collision
    uint32_t collision_layer_mask = 0xFFFFFFFF;

    // Lock-on state
    scene::Entity lock_on_target = scene::NullEntity;
    Vec3 lock_on_target_position{0.0f};

    // Mode transition
    float transition_progress = 1.0f;           // 0 = start, 1 = complete
    float transition_duration = 0.3f;           // Seconds to transition

    // Side offset (for over-shoulder, 1 = right, -1 = left)
    float shoulder_side = 1.0f;

    // ========================================================================
    // Helpers
    // ========================================================================

    void set_mode(ThirdPersonCameraMode new_mode) {
        if (mode == new_mode) return;
        previous_mode = mode;
        mode = new_mode;
        transition_progress = 0.0f;
    }

    void switch_shoulder() {
        shoulder_side = -shoulder_side;
    }

    bool is_transitioning() const {
        return transition_progress < 1.0f;
    }

    const CameraPreset& get_preset_for_mode(ThirdPersonCameraMode m) const {
        switch (m) {
            case ThirdPersonCameraMode::Centered: return centered_preset;
            case ThirdPersonCameraMode::OverShoulder: return over_shoulder_preset;
            case ThirdPersonCameraMode::Aiming: return aiming_preset;
            case ThirdPersonCameraMode::LockOn: return lock_on_preset;
            default: return centered_preset;
        }
    }

    CameraPreset& get_preset_for_mode(ThirdPersonCameraMode m) {
        switch (m) {
            case ThirdPersonCameraMode::Centered: return centered_preset;
            case ThirdPersonCameraMode::OverShoulder: return over_shoulder_preset;
            case ThirdPersonCameraMode::Aiming: return aiming_preset;
            case ThirdPersonCameraMode::LockOn: return lock_on_preset;
            default: return centered_preset;
        }
    }
};

// ============================================================================
// Events
// ============================================================================

struct CameraModeChangedEvent {
    scene::Entity camera_entity;
    ThirdPersonCameraMode old_mode;
    ThirdPersonCameraMode new_mode;
};

struct CameraLockOnStartedEvent {
    scene::Entity camera_entity;
    scene::Entity target_entity;
};

struct CameraLockOnEndedEvent {
    scene::Entity camera_entity;
};

// ============================================================================
// Camera Collision Check
// ============================================================================

// Raycast function for collision detection
using CameraCollisionCheck = std::function<float(const Vec3& from, const Vec3& to,
                                                   float radius, uint32_t layer_mask)>;

// ============================================================================
// Third Person Camera System
// ============================================================================

class ThirdPersonCameraSystem {
public:
    static ThirdPersonCameraSystem& instance();

    // Delete copy/move
    ThirdPersonCameraSystem(const ThirdPersonCameraSystem&) = delete;
    ThirdPersonCameraSystem& operator=(const ThirdPersonCameraSystem&) = delete;

    // ========================================================================
    // Input
    // ========================================================================

    // Apply look input (from mouse or gamepad)
    void apply_look_input(scene::World& world, scene::Entity camera,
                          float delta_x, float delta_y);

    // ========================================================================
    // Mode Control
    // ========================================================================

    // Set camera mode
    void set_mode(scene::World& world, scene::Entity camera, ThirdPersonCameraMode mode);

    // Get current mode
    ThirdPersonCameraMode get_mode(scene::World& world, scene::Entity camera) const;

    // Toggle between centered and over-shoulder
    void toggle_shoulder_mode(scene::World& world, scene::Entity camera);

    // Switch shoulder side
    void switch_shoulder(scene::World& world, scene::Entity camera);

    // ========================================================================
    // Lock-On Integration
    // ========================================================================

    // Set lock-on target (integrates with targeting system)
    void set_lock_on_target(scene::World& world, scene::Entity camera, scene::Entity target);

    // Clear lock-on target
    void clear_lock_on_target(scene::World& world, scene::Entity camera);

    // ========================================================================
    // Queries
    // ========================================================================

    // Get the calculated view matrix
    Mat4 get_view_matrix(scene::World& world, scene::Entity camera) const;

    // Get camera world position
    Vec3 get_camera_position(scene::World& world, scene::Entity camera) const;

    // Get camera forward direction
    Vec3 get_camera_forward(scene::World& world, scene::Entity camera) const;

    // Get the aim direction (may differ from camera forward during lock-on)
    Vec3 get_aim_direction(scene::World& world, scene::Entity camera) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    // Set collision check function (typically uses physics raycast)
    void set_collision_check(CameraCollisionCheck check);

    // Enable/disable collision
    void set_collision_enabled(bool enabled) { m_collision_enabled = enabled; }

    // ========================================================================
    // Default Presets
    // ========================================================================

    // Get default presets (can be used to initialize components)
    static CameraPreset get_default_centered_preset();
    static CameraPreset get_default_over_shoulder_preset();
    static CameraPreset get_default_aiming_preset();
    static CameraPreset get_default_lock_on_preset();

private:
    ThirdPersonCameraSystem();
    ~ThirdPersonCameraSystem() = default;

    // Internal update
    void update_camera(scene::World& world, scene::Entity camera,
                       ThirdPersonCameraComponent& cam, float dt);

    // Calculate pivot position (point camera orbits around)
    Vec3 calculate_pivot(scene::World& world, scene::Entity target,
                         const CameraPreset& preset, float shoulder_side);

    // Calculate ideal camera position before collision
    Vec3 calculate_ideal_position(const Vec3& pivot, float pitch, float yaw,
                                   float distance, const Vec3& offset, float shoulder_side);

    // Handle camera collision
    float handle_collision(const Vec3& pivot, const Vec3& ideal_pos,
                           float radius, uint32_t layer_mask);

    // Interpolate between presets
    CameraPreset interpolate_presets(const CameraPreset& from, const CameraPreset& to, float t);

    // Calculate rotation to look at target
    Quat calculate_lock_on_rotation(const Vec3& camera_pos, const Vec3& target_pos);

    // Default collision check (returns 1.0 if no collision)
    float default_collision_check(const Vec3& from, const Vec3& to, float radius, uint32_t layer_mask);

    CameraCollisionCheck m_collision_check;
    bool m_collision_enabled = true;
};

// Convenience accessor
inline ThirdPersonCameraSystem& third_person_camera() {
    return ThirdPersonCameraSystem::instance();
}

// ============================================================================
// ECS Systems
// ============================================================================

// Main camera update system (PreRender phase, high priority)
void third_person_camera_system(scene::World& world, double dt);

// ============================================================================
// Registration
// ============================================================================

// Register camera components with reflection
void register_third_person_camera_components();

} // namespace engine::render
