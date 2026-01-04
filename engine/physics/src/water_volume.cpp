#include <engine/physics/water_volume.hpp>
#include <cmath>
#include <algorithm>

namespace engine::physics {

// Constants
constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;

// WaterVolume implementation
WaterVolume::WaterVolume() = default;
WaterVolume::~WaterVolume() = default;

WaterVolume::WaterVolume(WaterVolume&&) noexcept = default;
WaterVolume& WaterVolume::operator=(WaterVolume&&) noexcept = default;

void WaterVolume::init(const WaterVolumeComponent& settings) {
    m_settings = settings;
    m_state = WaterVolumeState{};
    m_initialized = true;
}

void WaterVolume::shutdown() {
    m_initialized = false;
}

void WaterVolume::set_position(const Vec3& pos) {
    m_state.world_position = pos;
}

void WaterVolume::set_rotation(const Quat& rot) {
    m_state.world_rotation = rot;
}

bool WaterVolume::contains_point(const Vec3& point) const {
    if (!m_initialized) return false;

    // Transform point to local space
    Vec3 local_point = point - m_state.world_position;
    // Note: For full rotation support, would need to apply inverse rotation

    switch (m_settings.shape) {
        case WaterShape::Box: {
            const Vec3& half = m_settings.box_half_extents;
            return std::abs(local_point.x) <= half.x &&
                   local_point.y <= m_settings.surface_height &&
                   local_point.y >= (m_settings.surface_height - half.y * 2.0f) &&
                   std::abs(local_point.z) <= half.z;
        }

        case WaterShape::Sphere: {
            float dist_sq = local_point.x * local_point.x +
                           local_point.y * local_point.y +
                           local_point.z * local_point.z;
            return dist_sq <= m_settings.sphere_radius * m_settings.sphere_radius;
        }

        case WaterShape::Infinite:
            // Infinite water plane - only check Y
            return point.y <= get_surface_height_at(point);
    }

    return false;
}

float WaterVolume::get_surface_height_at(const Vec3& position) const {
    if (!m_initialized) return 0.0f;

    float base_height = m_state.world_position.y + m_settings.surface_height;

    // Add wave displacement
    if (m_settings.waves.enabled) {
        base_height += get_wave_height_at(position.x, position.z, m_state.current_time);
    }

    return base_height;
}

float WaterVolume::get_depth_at(const Vec3& position) const {
    float surface = get_surface_height_at(position);
    return surface - position.y;
}

Vec3 WaterVolume::get_flow_velocity_at(const Vec3& /*position*/) const {
    // For now, return uniform flow velocity
    // Could be extended with spatial variation
    return m_settings.flow_velocity;
}

float WaterVolume::get_wave_height_at(float x, float z, float time) const {
    if (!m_settings.waves.enabled) return 0.0f;

    if (m_settings.waves.use_gerstner) {
        return calculate_gerstner_wave(x, z, time);
    } else {
        return calculate_sine_wave(x, z, time);
    }
}

Vec3 WaterVolume::get_wave_normal_at(float x, float z, float time) const {
    if (!m_settings.waves.enabled) {
        return Vec3{0.0f, 1.0f, 0.0f};
    }

    // Calculate normal using finite differences
    constexpr float epsilon = 0.1f;

    float h_center = get_wave_height_at(x, z, time);
    float h_dx = get_wave_height_at(x + epsilon, z, time);
    float h_dz = get_wave_height_at(x, z + epsilon, time);

    Vec3 tangent_x{epsilon, h_dx - h_center, 0.0f};
    Vec3 tangent_z{0.0f, h_dz - h_center, epsilon};

    // Cross product for normal
    Vec3 normal{
        tangent_x.y * tangent_z.z - tangent_x.z * tangent_z.y,
        tangent_x.z * tangent_z.x - tangent_x.x * tangent_z.z,
        tangent_x.x * tangent_z.y - tangent_x.y * tangent_z.x
    };

    // Normalize
    float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (len > 0.0001f) {
        normal.x /= len;
        normal.y /= len;
        normal.z /= len;
    } else {
        normal = Vec3{0.0f, 1.0f, 0.0f};
    }

    return normal;
}

void WaterVolume::update(float dt) {
    m_state.current_time += dt;

    // Wrap time to prevent floating point precision issues
    if (m_state.current_time > 1000.0f) {
        m_state.current_time = std::fmod(m_state.current_time, 1000.0f);
    }
}

float WaterVolume::calculate_sine_wave(float x, float z, float time) const {
    const auto& waves = m_settings.waves;

    // Direction vector
    float dir_x = waves.direction.x;
    float dir_z = waves.direction.y;  // direction.y maps to world Z

    // Normalize direction
    float dir_len = std::sqrt(dir_x * dir_x + dir_z * dir_z);
    if (dir_len > 0.0001f) {
        dir_x /= dir_len;
        dir_z /= dir_len;
    }

    // Calculate wave phase
    float k = TWO_PI / waves.wavelength;  // Wave number
    float omega = k * waves.speed;         // Angular frequency

    // Dot product of position with direction
    float pos_along_wave = x * dir_x + z * dir_z;

    // Simple sine wave
    float phase = k * pos_along_wave - omega * time;
    return waves.amplitude * std::sin(phase);
}

float WaterVolume::calculate_gerstner_wave(float x, float z, float time) const {
    const auto& waves = m_settings.waves;

    // Direction vector
    float dir_x = waves.direction.x;
    float dir_z = waves.direction.y;

    // Normalize direction
    float dir_len = std::sqrt(dir_x * dir_x + dir_z * dir_z);
    if (dir_len > 0.0001f) {
        dir_x /= dir_len;
        dir_z /= dir_len;
    }

    // Wave parameters
    float k = TWO_PI / waves.wavelength;
    float omega = k * waves.speed;
    float A = waves.amplitude;
    float Q = waves.steepness;  // Steepness parameter (0-1)

    // Position along wave direction
    float pos_along_wave = x * dir_x + z * dir_z;

    // Gerstner wave formula (vertical component)
    float phase = k * pos_along_wave - omega * time;

    // The Gerstner wave adds both horizontal and vertical displacement
    // For height, we use the vertical component
    float height = A * std::sin(phase);

    // Add secondary waves for more complex pattern
    // Second harmonic
    float phase2 = 2.0f * k * pos_along_wave - 2.0f * omega * time + 0.5f;
    height += 0.3f * A * std::sin(phase2);

    // Third harmonic (perpendicular direction for cross-waves)
    float perp_pos = -x * dir_z + z * dir_x;
    float phase3 = 0.7f * k * perp_pos - 0.8f * omega * time + 1.2f;
    height += 0.2f * A * std::sin(phase3);

    return height;
}

// WaterVolumeManager implementation
WaterVolumeManager& WaterVolumeManager::instance() {
    static WaterVolumeManager s_instance;
    return s_instance;
}

void WaterVolumeManager::register_volume(const std::string& name, WaterVolume* volume) {
    m_volumes[name] = volume;
}

void WaterVolumeManager::unregister_volume(const std::string& name) {
    m_volumes.erase(name);
}

void WaterVolumeManager::clear() {
    m_volumes.clear();
}

WaterVolume* WaterVolumeManager::find_volume_at(const Vec3& position) const {
    for (const auto& [name, volume] : m_volumes) {
        if (volume && volume->contains_point(position)) {
            return volume;
        }
    }
    return nullptr;
}

std::vector<WaterVolume*> WaterVolumeManager::find_all_volumes_at(const Vec3& position) const {
    std::vector<WaterVolume*> result;
    for (const auto& [name, volume] : m_volumes) {
        if (volume && volume->contains_point(position)) {
            result.push_back(volume);
        }
    }
    return result;
}

void WaterVolumeManager::update_all(float dt) {
    for (auto& [name, volume] : m_volumes) {
        if (volume) {
            volume->update(dt);
        }
    }
}

} // namespace engine::physics
