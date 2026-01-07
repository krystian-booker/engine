#include <engine/environment/environment_lighting.hpp>
#include <engine/environment/time_of_day.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/log.hpp>
#include <algorithm>

namespace engine::environment {

// Template specialization for curve evaluation
template<typename T>
T LightingCurve<T>::evaluate(float time) const {
    if (keyframes.empty()) {
        return T{};
    }

    if (keyframes.size() == 1) {
        return keyframes[0].value;
    }

    // Normalize time to 0-24 range
    while (time < 0.0f) time += 24.0f;
    while (time >= 24.0f) time -= 24.0f;

    // Find surrounding keyframes
    size_t next_idx = 0;
    for (size_t i = 0; i < keyframes.size(); ++i) {
        if (keyframes[i].time > time) {
            next_idx = i;
            break;
        }
        if (i == keyframes.size() - 1) {
            next_idx = 0;  // Wrap to first keyframe
        }
    }

    size_t prev_idx = (next_idx == 0) ? keyframes.size() - 1 : next_idx - 1;

    const auto& prev = keyframes[prev_idx];
    const auto& next = keyframes[next_idx];

    // Calculate interpolation factor
    float prev_time = prev.time;
    float next_time = next.time;

    // Handle wrap around midnight
    if (next_time < prev_time) {
        next_time += 24.0f;
        if (time < prev_time) {
            time += 24.0f;
        }
    }

    float duration = next_time - prev_time;
    float t = (duration > 0.0f) ? (time - prev_time) / duration : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    // Linear interpolation
    return glm::mix(prev.value, next.value, t);
}

// Explicit template instantiations
template float LightingCurve<float>::evaluate(float) const;
template Vec3 LightingCurve<Vec3>::evaluate(float) const;

// Pre-built lighting curves
namespace LightingCurves {

LightingCurve<float> default_sun_intensity() {
    LightingCurve<float> curve;
    curve.add(0.0f, 0.0f);    // Midnight
    curve.add(5.0f, 0.0f);    // Before dawn
    curve.add(6.0f, 0.1f);    // Dawn start
    curve.add(7.0f, 0.4f);    // Dawn end
    curve.add(9.0f, 0.8f);    // Morning
    curve.add(12.0f, 1.0f);   // Noon
    curve.add(15.0f, 0.95f);  // Afternoon
    curve.add(17.0f, 0.7f);   // Late afternoon
    curve.add(18.0f, 0.4f);   // Dusk start
    curve.add(19.0f, 0.1f);   // Dusk end
    curve.add(20.0f, 0.0f);   // Night
    return curve;
}

LightingCurve<Vec3> default_sun_color() {
    LightingCurve<Vec3> curve;
    curve.add(0.0f, Vec3(0.1f, 0.1f, 0.2f));     // Midnight (moonlight blue)
    curve.add(5.0f, Vec3(0.1f, 0.1f, 0.15f));    // Before dawn
    curve.add(6.0f, Vec3(1.0f, 0.5f, 0.3f));     // Dawn (orange)
    curve.add(7.0f, Vec3(1.0f, 0.8f, 0.6f));     // Early morning (warm)
    curve.add(9.0f, Vec3(1.0f, 0.95f, 0.9f));    // Morning (slight warm)
    curve.add(12.0f, Vec3(1.0f, 0.98f, 0.95f));  // Noon (white)
    curve.add(15.0f, Vec3(1.0f, 0.95f, 0.85f));  // Afternoon (slight warm)
    curve.add(17.0f, Vec3(1.0f, 0.85f, 0.7f));   // Late afternoon (warm)
    curve.add(18.0f, Vec3(1.0f, 0.6f, 0.4f));    // Dusk (orange)
    curve.add(19.0f, Vec3(0.9f, 0.4f, 0.3f));    // Late dusk (red-orange)
    curve.add(20.0f, Vec3(0.1f, 0.1f, 0.2f));    // Night (moonlight)
    return curve;
}

LightingCurve<float> default_ambient_intensity() {
    LightingCurve<float> curve;
    curve.add(0.0f, 0.05f);   // Midnight
    curve.add(5.0f, 0.05f);   // Before dawn
    curve.add(6.0f, 0.15f);   // Dawn
    curve.add(8.0f, 0.3f);    // Morning
    curve.add(12.0f, 0.4f);   // Noon
    curve.add(17.0f, 0.35f);  // Afternoon
    curve.add(19.0f, 0.15f);  // Dusk
    curve.add(21.0f, 0.05f);  // Night
    return curve;
}

LightingCurve<Vec3> default_ambient_color() {
    LightingCurve<Vec3> curve;
    curve.add(0.0f, Vec3(0.05f, 0.05f, 0.1f));   // Midnight (dark blue)
    curve.add(6.0f, Vec3(0.4f, 0.3f, 0.3f));     // Dawn (warm)
    curve.add(9.0f, Vec3(0.6f, 0.65f, 0.7f));    // Morning (sky blue)
    curve.add(12.0f, Vec3(0.7f, 0.75f, 0.8f));   // Noon (bright blue)
    curve.add(17.0f, Vec3(0.6f, 0.55f, 0.5f));   // Afternoon (warm)
    curve.add(19.0f, Vec3(0.3f, 0.25f, 0.35f));  // Dusk (purple-ish)
    curve.add(21.0f, Vec3(0.05f, 0.05f, 0.1f));  // Night (dark blue)
    return curve;
}

} // namespace LightingCurves

// Implementation struct
struct EnvironmentLighting::Impl {
    bool initialized = false;
    bool enabled = true;
    scene::World* world = nullptr;
    scene::Entity sun_entity;

    EnvironmentLightingConfig config;

    // Override values (-1 means no override for float, zero vec for color)
    float sun_intensity_override = -1.0f;
    Vec3 sun_color_override{-1.0f};
    float ambient_intensity_override = -1.0f;
    Vec3 ambient_color_override{-1.0f};

    // Current computed values
    float current_sun_intensity = 1.0f;
    Vec3 current_sun_color{1.0f};
    float current_ambient_intensity = 0.3f;
    Vec3 current_ambient_color{0.5f, 0.55f, 0.6f};
};

EnvironmentLighting::EnvironmentLighting() : m_impl(std::make_unique<Impl>()) {}

EnvironmentLighting::~EnvironmentLighting() = default;

void EnvironmentLighting::initialize(scene::World& world, const EnvironmentLightingConfig& config) {
    m_impl->world = &world;
    m_impl->config = config;

    // Set up default curves if requested
    if (config.use_defaults) {
        if (m_impl->config.sun_intensity.empty()) {
            m_impl->config.sun_intensity = LightingCurves::default_sun_intensity();
        }
        if (m_impl->config.sun_color.empty()) {
            m_impl->config.sun_color = LightingCurves::default_sun_color();
        }
        if (m_impl->config.ambient_intensity.empty()) {
            m_impl->config.ambient_intensity = LightingCurves::default_ambient_intensity();
        }
        if (m_impl->config.ambient_color.empty()) {
            m_impl->config.ambient_color = LightingCurves::default_ambient_color();
        }
    }

    m_impl->initialized = true;

    core::log(core::LogLevel::Info, "[Environment] EnvironmentLighting initialized");
}

void EnvironmentLighting::update(double /*dt*/) {
    if (!m_impl->initialized || !m_impl->enabled) return;

    float current_hour = get_time_of_day().get_time();

    // Evaluate curves
    if (!m_impl->config.sun_intensity.empty()) {
        m_impl->current_sun_intensity = m_impl->config.sun_intensity.evaluate(current_hour);
    }
    if (!m_impl->config.sun_color.empty()) {
        m_impl->current_sun_color = m_impl->config.sun_color.evaluate(current_hour);
    }
    if (!m_impl->config.ambient_intensity.empty()) {
        m_impl->current_ambient_intensity = m_impl->config.ambient_intensity.evaluate(current_hour);
    }
    if (!m_impl->config.ambient_color.empty()) {
        m_impl->current_ambient_color = m_impl->config.ambient_color.evaluate(current_hour);
    }

    // Apply overrides
    if (m_impl->sun_intensity_override >= 0.0f) {
        m_impl->current_sun_intensity = m_impl->sun_intensity_override;
    }
    if (m_impl->sun_color_override.x >= 0.0f) {
        m_impl->current_sun_color = m_impl->sun_color_override;
    }
    if (m_impl->ambient_intensity_override >= 0.0f) {
        m_impl->current_ambient_intensity = m_impl->ambient_intensity_override;
    }
    if (m_impl->ambient_color_override.x >= 0.0f) {
        m_impl->current_ambient_color = m_impl->ambient_color_override;
    }

    // Apply to sun entity if set
    // Note: Actual Light component update would go here
    // This would involve getting the Light component and setting its properties
    // For now, values are available via getters for the render pipeline to use
}

void EnvironmentLighting::shutdown() {
    m_impl->world = nullptr;
    m_impl->sun_entity = scene::Entity{};
    m_impl->initialized = false;
}

void EnvironmentLighting::set_sun_entity(scene::Entity entity) {
    m_impl->sun_entity = entity;
}

scene::Entity EnvironmentLighting::get_sun_entity() const {
    return m_impl->sun_entity;
}

void EnvironmentLighting::set_config(const EnvironmentLightingConfig& config) {
    m_impl->config = config;
}

const EnvironmentLightingConfig& EnvironmentLighting::get_config() const {
    return m_impl->config;
}

void EnvironmentLighting::set_sun_intensity_curve(const LightingCurve<float>& curve) {
    m_impl->config.sun_intensity = curve;
}

void EnvironmentLighting::set_sun_color_curve(const LightingCurve<Vec3>& curve) {
    m_impl->config.sun_color = curve;
}

void EnvironmentLighting::set_ambient_intensity_curve(const LightingCurve<float>& curve) {
    m_impl->config.ambient_intensity = curve;
}

void EnvironmentLighting::set_ambient_color_curve(const LightingCurve<Vec3>& curve) {
    m_impl->config.ambient_color = curve;
}

void EnvironmentLighting::override_sun_intensity(float intensity) {
    m_impl->sun_intensity_override = intensity;
}

void EnvironmentLighting::override_sun_color(const Vec3& color) {
    m_impl->sun_color_override = color;
}

void EnvironmentLighting::override_ambient_intensity(float intensity) {
    m_impl->ambient_intensity_override = intensity;
}

void EnvironmentLighting::override_ambient_color(const Vec3& color) {
    m_impl->ambient_color_override = color;
}

void EnvironmentLighting::clear_overrides() {
    m_impl->sun_intensity_override = -1.0f;
    m_impl->sun_color_override = Vec3{-1.0f};
    m_impl->ambient_intensity_override = -1.0f;
    m_impl->ambient_color_override = Vec3{-1.0f};
}

float EnvironmentLighting::get_current_sun_intensity() const {
    return m_impl->current_sun_intensity;
}

Vec3 EnvironmentLighting::get_current_sun_color() const {
    return m_impl->current_sun_color;
}

float EnvironmentLighting::get_current_ambient_intensity() const {
    return m_impl->current_ambient_intensity;
}

Vec3 EnvironmentLighting::get_current_ambient_color() const {
    return m_impl->current_ambient_color;
}

void EnvironmentLighting::set_enabled(bool enabled) {
    m_impl->enabled = enabled;
}

bool EnvironmentLighting::is_enabled() const {
    return m_impl->enabled;
}

// Global instance
EnvironmentLighting& get_environment_lighting() {
    static EnvironmentLighting instance;
    return instance;
}

} // namespace engine::environment
