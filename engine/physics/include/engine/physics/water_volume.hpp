#pragma once

#include <engine/physics/body.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::physics {

class PhysicsWorld;

// Water volume shape type
enum class WaterShape : uint8_t {
    Box,        // Rectangular pool
    Sphere,     // Spherical volume
    Infinite    // Infinite plane (ocean)
};

// Wave settings for dynamic water surface
struct WaveSettings {
    bool enabled = false;
    float amplitude = 0.5f;             // Wave height in meters
    float wavelength = 10.0f;           // Distance between wave peaks
    float speed = 2.0f;                 // Wave propagation speed m/s
    glm::vec2 direction{1.0f, 0.0f};    // Wave travel direction (XZ plane)

    // Gerstner wave parameters for realistic ocean waves
    bool use_gerstner = false;
    float steepness = 0.5f;             // 0 = sine wave, 1 = maximum steepness
};

// Water volume component - defines a water region in the world
struct WaterVolumeComponent {
    WaterShape shape = WaterShape::Box;

    // Shape parameters
    Vec3 box_half_extents{10.0f, 5.0f, 10.0f};
    float sphere_radius = 10.0f;
    float surface_height = 0.0f;        // Y position of water surface (world space)

    // Water physical properties
    float density = 1000.0f;            // kg/m^3 (fresh water = 1000, seawater = 1025)
    float linear_drag = 0.5f;           // Resistance to linear motion
    float angular_drag = 0.1f;          // Resistance to rotation
    float surface_drag = 2.0f;          // Additional drag at water surface

    // Flow/current
    Vec3 flow_velocity{0.0f};           // Water current velocity

    // Wave settings
    WaveSettings waves;

    // Rendering hints (for render system)
    Vec3 water_color{0.1f, 0.3f, 0.5f};
    Vec3 deep_color{0.02f, 0.05f, 0.1f};
    float transparency = 0.7f;
    float refraction_strength = 0.5f;
    float foam_threshold = 0.8f;        // Wave height for foam generation

    // Runtime state
    bool initialized = false;
};

// Water volume state for queries
struct WaterVolumeState {
    float current_time = 0.0f;          // For wave animation
    Vec3 world_position{0.0f};
    Quat world_rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

// Water volume - manages water physics region
class WaterVolume {
public:
    WaterVolume();
    ~WaterVolume();

    // Non-copyable, movable
    WaterVolume(const WaterVolume&) = delete;
    WaterVolume& operator=(const WaterVolume&) = delete;
    WaterVolume(WaterVolume&&) noexcept;
    WaterVolume& operator=(WaterVolume&&) noexcept;

    // Initialization
    void init(const WaterVolumeComponent& settings);
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Transform
    void set_position(const Vec3& pos);
    Vec3 get_position() const { return m_state.world_position; }
    void set_rotation(const Quat& rot);
    Quat get_rotation() const { return m_state.world_rotation; }

    // Queries
    bool contains_point(const Vec3& point) const;
    float get_surface_height_at(const Vec3& position) const;  // With wave displacement
    float get_depth_at(const Vec3& position) const;           // Depth below surface
    Vec3 get_flow_velocity_at(const Vec3& position) const;    // Current at position

    // Wave calculations
    float get_wave_height_at(float x, float z, float time) const;
    Vec3 get_wave_normal_at(float x, float z, float time) const;

    // Properties
    float get_density() const { return m_settings.density; }
    void set_density(float density) { m_settings.density = density; }
    float get_linear_drag() const { return m_settings.linear_drag; }
    float get_angular_drag() const { return m_settings.angular_drag; }

    // Update (for wave animation)
    void update(float dt);

    // Get settings
    const WaterVolumeComponent& get_settings() const { return m_settings; }

private:
    WaterVolumeComponent m_settings;
    WaterVolumeState m_state;
    bool m_initialized = false;

    // Wave calculation helpers
    float calculate_sine_wave(float x, float z, float time) const;
    float calculate_gerstner_wave(float x, float z, float time) const;
};

// Water volume controller component for ECS
struct WaterVolumeControllerComponent {
    std::unique_ptr<WaterVolume> volume;

    // Convenience accessors
    bool contains_point(const Vec3& point) const {
        return volume && volume->contains_point(point);
    }

    float get_surface_height_at(const Vec3& pos) const {
        return volume ? volume->get_surface_height_at(pos) : 0.0f;
    }

    float get_depth_at(const Vec3& pos) const {
        return volume ? volume->get_depth_at(pos) : 0.0f;
    }
};

// Global water volume manager for efficient queries
class WaterVolumeManager {
public:
    static WaterVolumeManager& instance();

    void register_volume(const std::string& name, WaterVolume* volume);
    void unregister_volume(const std::string& name);
    void clear();

    // Find water at a position
    WaterVolume* find_volume_at(const Vec3& position) const;
    std::vector<WaterVolume*> find_all_volumes_at(const Vec3& position) const;

    // Get all registered volumes
    const std::unordered_map<std::string, WaterVolume*>& get_all_volumes() const {
        return m_volumes;
    }

    // Update all volumes (wave animation)
    void update_all(float dt);

private:
    WaterVolumeManager() = default;
    std::unordered_map<std::string, WaterVolume*> m_volumes;
};

// Convenience function
inline WaterVolumeManager& get_water_volumes() {
    return WaterVolumeManager::instance();
}

} // namespace engine::physics
