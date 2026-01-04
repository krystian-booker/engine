#include <engine/ui/ui_animation.hpp>
#include <engine/ui/ui_element.hpp>
#include <algorithm>
#include <cmath>

namespace engine::ui {

// Global animator pointer
static UIAnimator* g_ui_animator = nullptr;

UIAnimator* get_ui_animator() {
    return g_ui_animator;
}

void set_ui_animator(UIAnimator* animator) {
    g_ui_animator = animator;
}

// Easing function implementations
float ease(EaseType type, float t) {
    // Clamp t to [0, 1]
    t = std::clamp(t, 0.0f, 1.0f);

    switch (type) {
        case EaseType::Linear:
            return t;

        case EaseType::EaseInQuad:
            return t * t;

        case EaseType::EaseOutQuad:
            return t * (2.0f - t);

        case EaseType::EaseInOutQuad:
            return t < 0.5f
                ? 2.0f * t * t
                : -1.0f + (4.0f - 2.0f * t) * t;

        case EaseType::EaseInCubic:
            return t * t * t;

        case EaseType::EaseOutCubic: {
            float f = t - 1.0f;
            return f * f * f + 1.0f;
        }

        case EaseType::EaseInOutCubic:
            return t < 0.5f
                ? 4.0f * t * t * t
                : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;

        case EaseType::EaseInBack: {
            const float c1 = 1.70158f;
            const float c3 = c1 + 1.0f;
            return c3 * t * t * t - c1 * t * t;
        }

        case EaseType::EaseOutBack: {
            const float c1 = 1.70158f;
            const float c3 = c1 + 1.0f;
            float f = t - 1.0f;
            return 1.0f + c3 * f * f * f + c1 * f * f;
        }

        case EaseType::EaseInOutBack: {
            const float c1 = 1.70158f;
            const float c2 = c1 * 1.525f;
            return t < 0.5f
                ? (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f
                : (std::pow(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
        }

        default:
            return t;
    }
}

float UITween::current_value() const {
    if (duration <= 0.0f) {
        return end_value;
    }

    float t = std::clamp(elapsed / duration, 0.0f, 1.0f);
    float eased_t = ease(ease_type, t);
    return start_value + (end_value - start_value) * eased_t;
}

void UIAnimator::update(float dt) {
    for (auto& tween : m_tweens) {
        if (tween.completed) continue;

        // Handle delay
        if (!tween.started) {
            tween.delay -= dt;
            if (tween.delay > 0.0f) continue;

            // Delay finished, start animation
            tween.started = true;
            tween.start_value = get_current_value(tween.element, tween.property);
            // Apply any leftover time from delay
            dt = -tween.delay;
            tween.delay = 0.0f;
        }

        // Update elapsed time
        tween.elapsed += dt;

        // Apply current value
        apply_tween_value(tween);

        // Check if finished
        if (tween.elapsed >= tween.duration) {
            tween.completed = true;
            if (tween.on_complete) {
                tween.on_complete();
            }
        }
    }

    // Remove completed tweens
    m_tweens.erase(
        std::remove_if(m_tweens.begin(), m_tweens.end(),
            [](const UITween& t) { return t.completed; }),
        m_tweens.end());
}

uint32_t UIAnimator::animate(UIElement* element, AnimProperty property,
                              float target_value, float duration,
                              EaseType ease_type, float delay,
                              AnimationCallback on_complete) {
    if (!element || duration < 0.0f) {
        return 0;
    }

    // Stop any existing animation on this property
    m_tweens.erase(
        std::remove_if(m_tweens.begin(), m_tweens.end(),
            [element, property](const UITween& t) {
                return t.element == element && t.property == property;
            }),
        m_tweens.end());

    UITween tween;
    tween.id = m_next_id++;
    tween.element = element;
    tween.property = property;
    tween.start_value = get_current_value(element, property);
    tween.end_value = target_value;
    tween.duration = duration;
    tween.elapsed = 0.0f;
    tween.delay = delay;
    tween.ease_type = ease_type;
    tween.on_complete = std::move(on_complete);
    tween.started = (delay <= 0.0f);
    tween.completed = false;

    m_tweens.push_back(std::move(tween));
    return tween.id;
}

void UIAnimator::stop_all(UIElement* element) {
    m_tweens.erase(
        std::remove_if(m_tweens.begin(), m_tweens.end(),
            [element](const UITween& t) { return t.element == element; }),
        m_tweens.end());
}

void UIAnimator::stop(uint32_t tween_id) {
    m_tweens.erase(
        std::remove_if(m_tweens.begin(), m_tweens.end(),
            [tween_id](const UITween& t) { return t.id == tween_id; }),
        m_tweens.end());
}

bool UIAnimator::is_animating(UIElement* element) const {
    return std::any_of(m_tweens.begin(), m_tweens.end(),
        [element](const UITween& t) { return t.element == element && !t.completed; });
}

void UIAnimator::clear() {
    m_tweens.clear();
}

void UIAnimator::apply_tween_value(UITween& tween) {
    if (!tween.element) return;

    float value = tween.current_value();

    switch (tween.property) {
        case AnimProperty::Opacity:
            tween.element->style().opacity = value;
            break;

        case AnimProperty::PositionX: {
            Vec2 pos = tween.element->get_position();
            pos.x = value;
            tween.element->set_position(pos);
            break;
        }

        case AnimProperty::PositionY: {
            Vec2 pos = tween.element->get_position();
            pos.y = value;
            tween.element->set_position(pos);
            break;
        }

        case AnimProperty::SizeWidth: {
            Vec2 size = tween.element->get_size();
            size.x = value;
            tween.element->set_size(size);
            break;
        }

        case AnimProperty::SizeHeight: {
            Vec2 size = tween.element->get_size();
            size.y = value;
            tween.element->set_size(size);
            break;
        }

        case AnimProperty::Scale:
            tween.element->style().scale = value;
            break;
    }
}

float UIAnimator::get_current_value(UIElement* element, AnimProperty property) const {
    if (!element) return 0.0f;

    switch (property) {
        case AnimProperty::Opacity:
            return element->get_style().opacity;

        case AnimProperty::PositionX:
            return element->get_position().x;

        case AnimProperty::PositionY:
            return element->get_position().y;

        case AnimProperty::SizeWidth:
            return element->get_size().x;

        case AnimProperty::SizeHeight:
            return element->get_size().y;

        case AnimProperty::Scale:
            return element->get_style().scale;

        default:
            return 0.0f;
    }
}

} // namespace engine::ui
