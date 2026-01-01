#pragma once

#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace engine::cinematic {

using namespace engine::core;

// Interpolation modes for keyframes
enum class InterpolationMode : uint8_t {
    Linear,     // Linear interpolation
    Step,       // No interpolation (hold value)
    Bezier,     // Cubic bezier curve
    CatmullRom  // Catmull-Rom spline (smooth through points)
};

// Easing functions for animation curves
enum class EaseType : uint8_t {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInElastic,
    EaseOutElastic,
    EaseOutBounce
};

// Base keyframe with time and interpolation settings
struct KeyframeBase {
    float time = 0.0f;
    InterpolationMode interpolation = InterpolationMode::Linear;
    EaseType easing = EaseType::Linear;

    // Bezier control points (relative offsets)
    Vec2 tangent_in{-0.1f, 0.0f};
    Vec2 tangent_out{0.1f, 0.0f};
};

// Typed keyframe
template<typename T>
struct Keyframe : KeyframeBase {
    T value{};

    Keyframe() = default;
    Keyframe(float t, const T& v) : value(v) { time = t; }
};

// Track types
enum class TrackType : uint8_t {
    Camera,
    Animation,
    Audio,
    Event,
    Property,   // Generic property animation
    Transform,  // Entity transform
    Light,      // Light parameters
    PostProcess // Post-processing effects
};

// Abstract base class for all tracks
class Track {
public:
    Track(const std::string& name, TrackType type);
    virtual ~Track() = default;

    // Track info
    const std::string& get_name() const { return m_name; }
    TrackType get_type() const { return m_type; }

    // Enable/disable
    bool is_enabled() const { return m_enabled; }
    void set_enabled(bool enabled) { m_enabled = enabled; }

    // Mute (for audio/animation preview)
    bool is_muted() const { return m_muted; }
    void set_muted(bool muted) { m_muted = muted; }

    // Lock (prevent editing)
    bool is_locked() const { return m_locked; }
    void set_locked(bool locked) { m_locked = locked; }

    // Track duration (based on last keyframe)
    virtual float get_duration() const = 0;

    // Update track at time (applies effects to world)
    virtual void evaluate(float time) = 0;

    // Reset track to initial state
    virtual void reset() = 0;

protected:
    std::string m_name;
    TrackType m_type;
    bool m_enabled = true;
    bool m_muted = false;
    bool m_locked = false;
};

// Easing functions implementation
float apply_easing(float t, EaseType type);

// Interpolation helpers
template<typename T>
T interpolate_linear(const T& a, const T& b, float t);

template<typename T>
T interpolate_bezier(const T& a, const T& b,
                     const Vec2& tangent_out, const Vec2& tangent_in,
                     float t);

template<typename T>
T evaluate_catmull_rom(const T& p0, const T& p1, const T& p2, const T& p3, float t);

// Specializations for quaternions (spherical interpolation)
template<>
Quat interpolate_linear(const Quat& a, const Quat& b, float t);

} // namespace engine::cinematic
