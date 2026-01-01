#include <engine/render/camera_effects.hpp>
#include <algorithm>
#include <cmath>

namespace engine::render {

// Global instance
static CameraEffects* s_camera_effects = nullptr;

CameraEffects& get_camera_effects() {
    if (!s_camera_effects) {
        static CameraEffects instance;
        s_camera_effects = &instance;
    }
    return *s_camera_effects;
}

void CameraEffects::update(float dt) {
    m_noise_time += dt;

    update_shakes(dt);
    update_trauma(dt);
}

uint32_t CameraEffects::add_shake(const CameraShake& shake) {
    CameraShake new_shake = shake;
    new_shake.id = m_next_shake_id++;
    new_shake.active = true;
    new_shake.elapsed = 0.0f;

    m_shakes.push_back(new_shake);
    return new_shake.id;
}

void CameraEffects::remove_shake(uint32_t id) {
    m_shakes.erase(
        std::remove_if(m_shakes.begin(), m_shakes.end(),
            [id](const CameraShake& s) { return s.id == id; }),
        m_shakes.end()
    );
}

void CameraEffects::clear_shakes() {
    m_shakes.clear();
}

void CameraEffects::add_trauma(float amount) {
    m_trauma.add_trauma(amount);
}

void CameraEffects::set_trauma(float amount) {
    m_trauma.trauma = std::clamp(amount, 0.0f, m_trauma.max_trauma);
}

float CameraEffects::sample_noise(float t, float seed) const {
    // Simple perlin-like noise approximation
    float x = t + seed;
    float noise = std::sin(x * 1.0f) * 0.5f +
                  std::sin(x * 2.3f) * 0.25f +
                  std::sin(x * 4.1f) * 0.125f +
                  std::sin(x * 8.7f) * 0.0625f;
    return noise / 0.9375f;  // Normalize to roughly -1 to 1
}

void CameraEffects::update_shakes(float dt) {
    Vec3 total_offset(0.0f);
    Vec3 total_rotation(0.0f);

    for (auto it = m_shakes.begin(); it != m_shakes.end();) {
        CameraShake& shake = *it;
        shake.elapsed += dt;

        // Check if expired
        if (shake.duration > 0.0f && shake.elapsed >= shake.duration) {
            it = m_shakes.erase(it);
            continue;
        }

        // Calculate intensity with fade
        float intensity = 1.0f;

        if (shake.fade_in > 0.0f && shake.elapsed < shake.fade_in) {
            intensity *= shake.elapsed / shake.fade_in;
        }

        if (shake.duration > 0.0f && shake.fade_out > 0.0f) {
            float time_left = shake.duration - shake.elapsed;
            if (time_left < shake.fade_out) {
                intensity *= time_left / shake.fade_out;
            }
        }

        if (shake.decay > 0.0f) {
            intensity *= std::exp(-shake.decay * shake.elapsed);
        }

        // Calculate shake based on type
        float t = shake.elapsed * shake.frequency * 6.28318f;
        Vec3 offset(0.0f);
        Vec3 rotation(0.0f);

        switch (shake.type) {
            case ShakeType::Perlin: {
                offset.x = sample_noise(t, 0.0f) * shake.position_amplitude.x;
                offset.y = sample_noise(t, 100.0f) * shake.position_amplitude.y;
                offset.z = sample_noise(t, 200.0f) * shake.position_amplitude.z;

                rotation.x = sample_noise(t, 300.0f) * shake.rotation_amplitude.x;
                rotation.y = sample_noise(t, 400.0f) * shake.rotation_amplitude.y;
                rotation.z = sample_noise(t, 500.0f) * shake.rotation_amplitude.z;
                break;
            }

            case ShakeType::Random: {
                std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
                offset.x = dist(m_rng) * shake.position_amplitude.x;
                offset.y = dist(m_rng) * shake.position_amplitude.y;
                offset.z = dist(m_rng) * shake.position_amplitude.z;

                rotation.x = dist(m_rng) * shake.rotation_amplitude.x;
                rotation.y = dist(m_rng) * shake.rotation_amplitude.y;
                rotation.z = dist(m_rng) * shake.rotation_amplitude.z;
                break;
            }

            case ShakeType::Sine: {
                float sin_t = std::sin(t);
                offset = shake.position_amplitude * sin_t;
                rotation = shake.rotation_amplitude * sin_t;
                break;
            }

            case ShakeType::Directional: {
                float sin_t = std::sin(t);
                offset = shake.direction * length(shake.position_amplitude) * sin_t;
                rotation = shake.rotation_amplitude * sin_t;
                break;
            }
        }

        total_offset = total_offset + offset * intensity;
        total_rotation = total_rotation + rotation * intensity;

        ++it;
    }

    m_shake_offset = total_offset;
    m_shake_rotation = total_rotation;
}

void CameraEffects::update_trauma(float dt) {
    if (m_trauma.trauma <= 0.0f) return;

    // Decay trauma
    m_trauma.trauma = std::max(0.0f, m_trauma.trauma - m_trauma.trauma_decay * dt);

    // Calculate shake from trauma
    float shake = m_trauma.get_shake();

    if (shake > 0.0f) {
        float t = m_noise_time * m_trauma.noise_speed;

        Vec3 trauma_offset;
        trauma_offset.x = sample_noise(t, 0.0f) * m_trauma.max_offset.x * shake;
        trauma_offset.y = sample_noise(t, 100.0f) * m_trauma.max_offset.y * shake;
        trauma_offset.z = sample_noise(t, 200.0f) * m_trauma.max_offset.z * shake;

        Vec3 trauma_rotation;
        trauma_rotation.x = sample_noise(t, 300.0f) * m_trauma.max_rotation.x * shake;
        trauma_rotation.y = sample_noise(t, 400.0f) * m_trauma.max_rotation.y * shake;
        trauma_rotation.z = sample_noise(t, 500.0f) * m_trauma.max_rotation.z * shake;

        m_shake_offset = m_shake_offset + trauma_offset;
        m_shake_rotation = m_shake_rotation + trauma_rotation;
    }
}

void CameraEffects::apply_to_transform(Vec3& position, Quat& rotation) const {
    position = position + m_shake_offset;

    // Apply rotation shake
    if (length(m_shake_rotation) > 0.001f) {
        Quat shake_rot = Quat::from_euler(
            m_shake_rotation.x * 0.0174533f,  // Convert to radians
            m_shake_rotation.y * 0.0174533f,
            m_shake_rotation.z * 0.0174533f
        );
        rotation = shake_rot * rotation;
    }
}

void CameraEffects::set_follow_target(const Vec3& position, const Quat& rotation) {
    m_follow_target_pos = position;
    m_follow_target_rot = rotation;
}

void CameraEffects::update_follow(float dt, Vec3& out_position, Quat& out_rotation) {
    // Calculate desired position
    Vec3 offset_world = m_follow_target_rot * m_follow.offset;
    Vec3 desired_pos = m_follow_target_pos + offset_world;

    // Collision avoidance
    if (m_follow.avoid_collision && m_collision_callback) {
        Vec3 hit_point;
        if (m_collision_callback(m_follow_target_pos, desired_pos, hit_point, m_follow.collision_radius)) {
            // Push camera toward target
            Vec3 to_target = normalize(m_follow_target_pos - desired_pos);
            desired_pos = hit_point + to_target * m_follow.collision_radius;
        }
    }

    // Apply bounds
    if (m_follow.use_bounds) {
        desired_pos = clamp(desired_pos, m_follow.min_bounds, m_follow.max_bounds);
    }

    // Smooth position
    if (m_follow.smooth_position) {
        m_follow_current_pos = CameraEffectsUtils::smooth_damp(
            m_follow_current_pos, desired_pos, m_follow_velocity,
            m_follow.position_smoothing, 1000.0f, dt
        );
    } else {
        float t = 1.0f - std::exp(-m_follow.follow_speed * dt);
        m_follow_current_pos = m_follow_current_pos + (desired_pos - m_follow_current_pos) * t;
    }

    // Calculate look-at rotation
    Vec3 look_target = m_follow_target_pos + m_follow.look_at_offset;
    Quat desired_rot = CameraEffectsUtils::look_at(m_follow_current_pos, look_target);

    // Smooth rotation
    if (m_follow.smooth_rotation) {
        float t = 1.0f - std::exp(-m_follow.rotation_speed * dt);
        m_follow_current_rot = slerp(m_follow_current_rot, desired_rot, t);
    } else {
        m_follow_current_rot = desired_rot;
    }

    out_position = m_follow_current_pos;
    out_rotation = m_follow_current_rot;
}

void CameraEffects::orbit_input(float delta_yaw, float delta_pitch, float delta_zoom) {
    m_orbit.yaw += delta_yaw * m_orbit.orbit_speed;
    m_orbit.pitch = std::clamp(m_orbit.pitch + delta_pitch * m_orbit.orbit_speed,
                                m_orbit.min_pitch, m_orbit.max_pitch);
    m_orbit.distance = std::clamp(m_orbit.distance - delta_zoom * m_orbit.zoom_speed,
                                   m_orbit.min_distance, m_orbit.max_distance);
}

void CameraEffects::update_orbit(float dt, Vec3& out_position, Quat& out_rotation) {
    // Smooth orbit values
    float t = 1.0f - std::exp(-10.0f * dt / std::max(0.001f, m_orbit.smoothing));
    m_orbit_current_yaw = m_orbit_current_yaw + (m_orbit.yaw - m_orbit_current_yaw) * t;
    m_orbit_current_pitch = m_orbit_current_pitch + (m_orbit.pitch - m_orbit_current_pitch) * t;
    m_orbit_current_distance = m_orbit_current_distance + (m_orbit.distance - m_orbit_current_distance) * t;

    // Calculate position on sphere
    float pitch_rad = m_orbit_current_pitch * 0.0174533f;
    float yaw_rad = m_orbit_current_yaw * 0.0174533f;

    Vec3 offset;
    offset.x = std::cos(pitch_rad) * std::sin(yaw_rad) * m_orbit_current_distance;
    offset.y = std::sin(pitch_rad) * m_orbit_current_distance;
    offset.z = std::cos(pitch_rad) * std::cos(yaw_rad) * m_orbit_current_distance;

    out_position = m_orbit.pivot + offset;
    out_rotation = CameraEffectsUtils::look_at(out_position, m_orbit.pivot);
}

// Preset factory functions
CameraShake CameraEffects::create_explosion_shake(float intensity) {
    CameraShake shake;
    shake.type = ShakeType::Perlin;
    shake.position_amplitude = Vec3(0.3f, 0.3f, 0.1f) * intensity;
    shake.rotation_amplitude = Vec3(3.0f, 3.0f, 1.0f) * intensity;
    shake.frequency = 20.0f;
    shake.duration = 0.5f;
    shake.fade_out = 0.3f;
    shake.decay = 3.0f;
    return shake;
}

CameraShake CameraEffects::create_impact_shake(float intensity) {
    CameraShake shake;
    shake.type = ShakeType::Random;
    shake.position_amplitude = Vec3(0.05f, 0.1f, 0.02f) * intensity;
    shake.rotation_amplitude = Vec3(1.0f, 0.5f, 0.5f) * intensity;
    shake.frequency = 30.0f;
    shake.duration = 0.15f;
    shake.fade_out = 0.1f;
    return shake;
}

CameraShake CameraEffects::create_footstep_shake(float intensity) {
    CameraShake shake;
    shake.type = ShakeType::Sine;
    shake.position_amplitude = Vec3(0.0f, 0.02f, 0.0f) * intensity;
    shake.rotation_amplitude = Vec3(0.2f, 0.0f, 0.1f) * intensity;
    shake.frequency = 8.0f;
    shake.duration = 0.1f;
    shake.fade_out = 0.05f;
    return shake;
}

CameraShake CameraEffects::create_continuous_shake(float intensity, float frequency) {
    CameraShake shake;
    shake.type = ShakeType::Perlin;
    shake.position_amplitude = Vec3(0.05f) * intensity;
    shake.rotation_amplitude = Vec3(0.5f) * intensity;
    shake.frequency = frequency;
    shake.duration = 0.0f;  // Infinite
    return shake;
}

} // namespace engine::render
