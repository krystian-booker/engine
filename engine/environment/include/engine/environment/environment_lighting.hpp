#pragma once

#include <engine/core/math.hpp>
#include <engine/scene/entity.hpp>
#include <vector>
#include <memory>

namespace engine::scene {
    class World;
}

namespace engine::environment {

using namespace engine::core;

// Keyframe for value curves
template<typename T>
struct Keyframe {
    float time;  // Hour (0-24)
    T value;

    Keyframe() = default;
    Keyframe(float t, const T& v) : time(t), value(v) {}
};

// Curve of values over time (day cycle)
template<typename T>
struct LightingCurve {
    std::vector<Keyframe<T>> keyframes;

    // Add a keyframe
    void add(float time, const T& value) {
        keyframes.push_back(Keyframe<T>(time, value));
        // Keep sorted by time
        std::sort(keyframes.begin(), keyframes.end(),
            [](const auto& a, const auto& b) { return a.time < b.time; });
    }

    // Evaluate curve at a given time (with wrapping for 24h cycle)
    T evaluate(float time) const;

    // Clear all keyframes
    void clear() { keyframes.clear(); }

    // Check if curve has data
    bool empty() const { return keyframes.empty(); }
};

// Pre-built lighting curves for common setups
namespace LightingCurves {
    // Default sun intensity curve (bright at noon, dark at night)
    LightingCurve<float> default_sun_intensity();

    // Default sun color curve (orange at dawn/dusk, white at noon, blue at night)
    LightingCurve<Vec3> default_sun_color();

    // Default ambient intensity curve
    LightingCurve<float> default_ambient_intensity();

    // Default ambient color curve
    LightingCurve<Vec3> default_ambient_color();
}

// Configuration for environment lighting
struct EnvironmentLightingConfig {
    // Curves for sun directional light
    LightingCurve<float> sun_intensity;
    LightingCurve<Vec3> sun_color;

    // Curves for ambient/environment lighting
    LightingCurve<float> ambient_intensity;
    LightingCurve<Vec3> ambient_color;

    // Shadow settings by time of day
    LightingCurve<float> shadow_intensity;      // Shadow darkness (0 = invisible, 1 = black)
    LightingCurve<float> shadow_distance;       // Shadow draw distance

    // Use default curves if not specified
    bool use_defaults = true;
};

// Environment lighting controller - synchronizes lighting with time of day
class EnvironmentLighting {
public:
    EnvironmentLighting();
    ~EnvironmentLighting();

    // Non-copyable
    EnvironmentLighting(const EnvironmentLighting&) = delete;
    EnvironmentLighting& operator=(const EnvironmentLighting&) = delete;

    // Initialize with a world reference
    void initialize(scene::World& world, const EnvironmentLightingConfig& config = {});

    // Update each frame (syncs with TimeOfDay)
    void update(double dt);

    // Shutdown
    void shutdown();

    // Set the sun entity (directional light to control)
    void set_sun_entity(scene::Entity entity);
    scene::Entity get_sun_entity() const;

    // Set configuration curves
    void set_config(const EnvironmentLightingConfig& config);
    const EnvironmentLightingConfig& get_config() const;

    // Individual curve setters
    void set_sun_intensity_curve(const LightingCurve<float>& curve);
    void set_sun_color_curve(const LightingCurve<Vec3>& curve);
    void set_ambient_intensity_curve(const LightingCurve<float>& curve);
    void set_ambient_color_curve(const LightingCurve<Vec3>& curve);

    // Manual overrides (for cutscenes, weather, etc.)
    void override_sun_intensity(float intensity);
    void override_sun_color(const Vec3& color);
    void override_ambient_intensity(float intensity);
    void override_ambient_color(const Vec3& color);
    void clear_overrides();

    // Query current computed values
    float get_current_sun_intensity() const;
    Vec3 get_current_sun_color() const;
    float get_current_ambient_intensity() const;
    Vec3 get_current_ambient_color() const;

    // Enable/disable automatic updates from TimeOfDay
    void set_enabled(bool enabled);
    bool is_enabled() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Global EnvironmentLighting instance accessor
EnvironmentLighting& get_environment_lighting();

} // namespace engine::environment
