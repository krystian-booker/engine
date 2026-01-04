#pragma once

#include <engine/core/math.hpp>
#include <functional>
#include <vector>
#include <cstdint>

namespace engine::ui {

using namespace engine::core;

// Forward declarations
class UIElement;

// Easing function types for UI animations
enum class EaseType : uint8_t {
    Linear,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInBack,
    EaseOutBack,
    EaseInOutBack
};

// Evaluate easing function for t in [0, 1]
float ease(EaseType type, float t);

// Animation target property types
enum class AnimProperty : uint8_t {
    Opacity,
    PositionX,
    PositionY,
    SizeWidth,
    SizeHeight,
    Scale
};

// Callback when animation completes
using AnimationCallback = std::function<void()>;

// Single property tween
struct UITween {
    uint32_t id = 0;                    // Unique tween ID
    UIElement* element = nullptr;        // Target element
    AnimProperty property;               // Property being animated
    float start_value = 0.0f;
    float end_value = 0.0f;
    float duration = 0.0f;               // Total duration in seconds
    float elapsed = 0.0f;                // Current elapsed time
    float delay = 0.0f;                  // Delay before starting
    EaseType ease_type = EaseType::EaseOutQuad;
    AnimationCallback on_complete;
    bool started = false;                // True once delay has passed
    bool completed = false;

    // Calculate current value
    float current_value() const;

    // Check if animation is finished
    bool is_finished() const { return completed; }
};

// Animation manager - handles all active tweens
class UIAnimator {
public:
    UIAnimator() = default;
    ~UIAnimator() = default;

    // Update all active animations
    void update(float dt);

    // Create a new tween and return its ID
    uint32_t animate(UIElement* element, AnimProperty property,
                     float target_value, float duration,
                     EaseType ease = EaseType::EaseOutQuad,
                     float delay = 0.0f,
                     AnimationCallback on_complete = nullptr);

    // Stop all animations on an element
    void stop_all(UIElement* element);

    // Stop a specific animation by ID
    void stop(uint32_t tween_id);

    // Check if element has any active animations
    bool is_animating(UIElement* element) const;

    // Clear all animations
    void clear();

    // Get number of active animations
    size_t active_count() const { return m_tweens.size(); }

private:
    void apply_tween_value(UITween& tween);
    float get_current_value(UIElement* element, AnimProperty property) const;

    std::vector<UITween> m_tweens;
    uint32_t m_next_id = 1;
};

// Global animator access (managed by UIContext)
UIAnimator* get_ui_animator();
void set_ui_animator(UIAnimator* animator);

} // namespace engine::ui
