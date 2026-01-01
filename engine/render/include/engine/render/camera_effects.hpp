#pragma once

#include <engine/core/math.hpp>
#include <vector>
#include <functional>
#include <random>

namespace engine::render {

using namespace engine::core;

// Camera shake type
enum class ShakeType : uint8_t {
    Perlin,         // Smooth perlin noise shake
    Random,         // Random impulse shake
    Sine,           // Sinusoidal shake
    Directional     // Shake in specific direction
};

// Camera shake definition
struct CameraShake {
    ShakeType type = ShakeType::Perlin;

    // Amplitude
    Vec3 position_amplitude = Vec3(0.1f);  // Position shake amount
    Vec3 rotation_amplitude = Vec3(1.0f);  // Rotation shake in degrees

    // Frequency
    float frequency = 10.0f;               // Shake frequency (Hz)
    float frequency_variation = 0.2f;      // Random frequency variation

    // Timing
    float duration = 0.5f;                 // Shake duration (0 = infinite)
    float fade_in = 0.0f;                  // Fade in time
    float fade_out = 0.2f;                 // Fade out time

    // Decay
    float decay = 0.0f;                    // Exponential decay rate

    // Direction (for directional shake)
    Vec3 direction = Vec3(0.0f, 1.0f, 0.0f);

    // State
    float elapsed = 0.0f;
    bool active = false;
    uint32_t id = 0;
};

// Trauma shake (Vlambeer-style)
struct TraumaShake {
    float trauma = 0.0f;                   // Current trauma (0-1)
    float max_trauma = 1.0f;               // Maximum trauma
    float trauma_decay = 1.0f;             // Trauma decay per second
    float trauma_power = 2.0f;             // Shake = trauma^power

    // Shake parameters
    Vec3 max_offset = Vec3(0.5f);          // Max position offset
    Vec3 max_rotation = Vec3(5.0f);        // Max rotation in degrees
    float noise_speed = 5.0f;              // Perlin noise speed

    // Add trauma (clamped to max)
    void add_trauma(float amount) {
        trauma = std::min(trauma + amount, max_trauma);
    }

    // Get current shake intensity
    float get_shake() const {
        return std::pow(trauma, trauma_power);
    }
};

// Camera follow settings
struct CameraFollowSettings {
    Vec3 offset = Vec3(0.0f, 2.0f, -5.0f);  // Offset from target
    float follow_speed = 5.0f;              // Position follow speed
    float rotation_speed = 5.0f;            // Rotation follow speed

    // Collision avoidance
    bool avoid_collision = true;
    float collision_radius = 0.3f;
    float collision_push_speed = 10.0f;
    uint32_t collision_layer_mask = 0xFFFFFFFF;

    // Bounds
    bool use_bounds = false;
    Vec3 min_bounds = Vec3(-100.0f);
    Vec3 max_bounds = Vec3(100.0f);

    // Smoothing
    bool smooth_position = true;
    bool smooth_rotation = true;
    float position_smoothing = 0.1f;
    float rotation_smoothing = 0.1f;

    // Look at offset
    Vec3 look_at_offset = Vec3(0.0f, 1.0f, 0.0f);
};

// Camera orbit settings
struct CameraOrbitSettings {
    float distance = 5.0f;
    float min_distance = 1.0f;
    float max_distance = 20.0f;

    float pitch = 30.0f;                    // Degrees
    float yaw = 0.0f;                       // Degrees
    float min_pitch = -89.0f;
    float max_pitch = 89.0f;

    float orbit_speed = 180.0f;             // Degrees per second
    float zoom_speed = 5.0f;
    float smoothing = 0.1f;

    Vec3 pivot = Vec3(0.0f);
};

// Camera effects system
class CameraEffects {
public:
    CameraEffects() = default;
    ~CameraEffects() = default;

    // Update (call each frame)
    void update(float dt);

    // Screen shake
    uint32_t add_shake(const CameraShake& shake);
    void remove_shake(uint32_t id);
    void clear_shakes();

    // Trauma system
    void add_trauma(float amount);
    void set_trauma(float amount);
    float get_trauma() const { return m_trauma.trauma; }
    TraumaShake& get_trauma_settings() { return m_trauma; }

    // Get combined shake offset/rotation
    Vec3 get_shake_offset() const { return m_shake_offset; }
    Vec3 get_shake_rotation() const { return m_shake_rotation; }

    // Apply shake to transform
    void apply_to_transform(Vec3& position, Quat& rotation) const;

    // Follow camera
    void set_follow_target(const Vec3& position, const Quat& rotation);
    void update_follow(float dt, Vec3& out_position, Quat& out_rotation);
    CameraFollowSettings& get_follow_settings() { return m_follow; }

    // Orbit camera
    void orbit_input(float delta_yaw, float delta_pitch, float delta_zoom);
    void update_orbit(float dt, Vec3& out_position, Quat& out_rotation);
    CameraOrbitSettings& get_orbit_settings() { return m_orbit; }

    // Collision callback
    using CollisionCallback = std::function<bool(const Vec3& start, const Vec3& end,
                                                   Vec3& hit_point, float radius)>;
    void set_collision_callback(CollisionCallback callback) { m_collision_callback = callback; }

    // Presets
    static CameraShake create_explosion_shake(float intensity = 1.0f);
    static CameraShake create_impact_shake(float intensity = 1.0f);
    static CameraShake create_footstep_shake(float intensity = 0.2f);
    static CameraShake create_continuous_shake(float intensity = 0.5f, float frequency = 5.0f);

private:
    float sample_noise(float t, float seed) const;
    void update_shakes(float dt);
    void update_trauma(float dt);

    // Active shakes
    std::vector<CameraShake> m_shakes;
    uint32_t m_next_shake_id = 1;

    // Trauma
    TraumaShake m_trauma;

    // Combined shake output
    Vec3 m_shake_offset = Vec3(0.0f);
    Vec3 m_shake_rotation = Vec3(0.0f);

    // Follow
    CameraFollowSettings m_follow;
    Vec3 m_follow_target_pos = Vec3(0.0f);
    Quat m_follow_target_rot = Quat::identity();
    Vec3 m_follow_current_pos = Vec3(0.0f);
    Quat m_follow_current_rot = Quat::identity();
    Vec3 m_follow_velocity = Vec3(0.0f);

    // Orbit
    CameraOrbitSettings m_orbit;
    float m_orbit_current_distance = 5.0f;
    float m_orbit_current_pitch = 30.0f;
    float m_orbit_current_yaw = 0.0f;

    // Collision
    CollisionCallback m_collision_callback;

    // Noise state
    float m_noise_time = 0.0f;
    mutable std::mt19937 m_rng{std::random_device{}()};
};

// Global camera effects instance
CameraEffects& get_camera_effects();

// ECS Component
struct CameraControllerComponent {
    enum class Mode : uint8_t {
        Free,
        Follow,
        Orbit,
        Fixed
    };

    Mode mode = Mode::Free;

    // Follow settings
    CameraFollowSettings follow;
    uint64_t follow_target_entity = 0;

    // Orbit settings
    CameraOrbitSettings orbit;

    // Effects
    bool enable_shake = true;
    float shake_multiplier = 1.0f;
};

// Camera effects utilities
namespace CameraEffectsUtils {

// Smooth damp (like Unity's SmoothDamp)
inline float smooth_damp(float current, float target, float& velocity,
                          float smooth_time, float max_speed, float dt) {
    smooth_time = std::max(0.0001f, smooth_time);
    float omega = 2.0f / smooth_time;

    float x = omega * dt;
    float exp_x = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

    float change = current - target;
    float original_to = target;

    float max_change = max_speed * smooth_time;
    change = std::clamp(change, -max_change, max_change);
    target = current - change;

    float temp = (velocity + omega * change) * dt;
    velocity = (velocity - omega * temp) * exp_x;

    float output = target + (change + temp) * exp_x;

    if ((original_to - current > 0.0f) == (output > original_to)) {
        output = original_to;
        velocity = (output - original_to) / dt;
    }

    return output;
}

inline Vec3 smooth_damp(const Vec3& current, const Vec3& target, Vec3& velocity,
                         float smooth_time, float max_speed, float dt) {
    return Vec3(
        smooth_damp(current.x, target.x, velocity.x, smooth_time, max_speed, dt),
        smooth_damp(current.y, target.y, velocity.y, smooth_time, max_speed, dt),
        smooth_damp(current.z, target.z, velocity.z, smooth_time, max_speed, dt)
    );
}

// Calculate look-at rotation
inline Quat look_at(const Vec3& from, const Vec3& to, const Vec3& up = Vec3(0, 1, 0)) {
    Vec3 forward = normalize(to - from);
    Vec3 right = normalize(cross(up, forward));
    Vec3 actual_up = cross(forward, right);

    Mat4 rot_mat = Mat4::identity();
    rot_mat.m[0][0] = right.x;
    rot_mat.m[1][0] = right.y;
    rot_mat.m[2][0] = right.z;
    rot_mat.m[0][1] = actual_up.x;
    rot_mat.m[1][1] = actual_up.y;
    rot_mat.m[2][1] = actual_up.z;
    rot_mat.m[0][2] = forward.x;
    rot_mat.m[1][2] = forward.y;
    rot_mat.m[2][2] = forward.z;

    return Quat::from_rotation_matrix(rot_mat);
}

// Spherical interpolation for camera positions
inline Vec3 slerp_position(const Vec3& from, const Vec3& to, const Vec3& pivot, float t) {
    Vec3 from_dir = normalize(from - pivot);
    Vec3 to_dir = normalize(to - pivot);

    float from_dist = length(from - pivot);
    float to_dist = length(to - pivot);

    float angle = std::acos(std::clamp(dot(from_dir, to_dir), -1.0f, 1.0f));

    if (angle < 0.001f) {
        return from + (to - from) * t;
    }

    float sin_angle = std::sin(angle);
    float a = std::sin((1.0f - t) * angle) / sin_angle;
    float b = std::sin(t * angle) / sin_angle;

    Vec3 dir = from_dir * a + to_dir * b;
    float dist = from_dist + (to_dist - from_dist) * t;

    return pivot + dir * dist;
}

} // namespace CameraEffectsUtils

} // namespace engine::render
