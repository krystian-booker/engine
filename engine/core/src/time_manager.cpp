#include <engine/core/time_manager.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <cmath>

namespace engine::core {

// ============================================================================
// TimeManager Singleton
// ============================================================================

TimeManager::TimeManager() {
    // Initialize all group scales to 1.0
    m_group_scales.fill(1.0f);
    m_group_dt.fill(0.0f);
}

TimeManager& TimeManager::instance() {
    static TimeManager s_instance;
    return s_instance;
}

// ============================================================================
// Global Time Scale
// ============================================================================

void TimeManager::set_time_scale(float scale) {
    float old_scale = m_global_time_scale;
    m_global_time_scale = std::max(0.0f, scale);

    if (m_on_time_scale_changed && old_scale != m_global_time_scale) {
        m_on_time_scale_changed(m_global_time_scale);
    }

    log_debug("time", "Time scale set to {}", m_global_time_scale);
}

void TimeManager::transition_time_scale(float target, float duration,
                                        std::function<float(float)> easing) {
    m_transition.from_scale = m_global_time_scale;
    m_transition.to_scale = target;
    m_transition.duration = duration;
    m_transition.elapsed = 0.0f;
    m_transition.easing = easing ? easing : ease_out_quad;
    m_transition.active = true;

    log_debug("time", "Starting time scale transition: {} -> {} over {}s",
              m_transition.from_scale, m_transition.to_scale, duration);
}

void TimeManager::cancel_transition() {
    m_transition.active = false;
}

float TimeManager::get_transition_progress() const {
    if (!m_transition.active || m_transition.duration <= 0.0f) {
        return 1.0f;
    }
    return std::min(1.0f, m_transition.elapsed / m_transition.duration);
}

// ============================================================================
// Per-Group Time Scales
// ============================================================================

void TimeManager::set_group_scale(TimeGroup group, float scale) {
    size_t idx = static_cast<size_t>(group);
    if (idx < m_group_scales.size()) {
        m_group_scales[idx] = std::max(0.0f, scale);
    }
}

float TimeManager::get_group_scale(TimeGroup group) const {
    size_t idx = static_cast<size_t>(group);
    if (idx < m_group_scales.size()) {
        return m_group_scales[idx];
    }
    return 1.0f;
}

float TimeManager::get_effective_scale(TimeGroup group) const {
    return calculate_effective_scale(group);
}

// ============================================================================
// Pause System
// ============================================================================

void TimeManager::pause() {
    if (!m_paused) {
        m_paused = true;
        log_info("time", "Game paused");

        if (m_on_pause) {
            m_on_pause(true);
        }
    }
}

void TimeManager::unpause() {
    if (m_paused) {
        m_paused = false;
        log_info("time", "Game unpaused");

        if (m_on_pause) {
            m_on_pause(false);
        }
    }
}

void TimeManager::toggle_pause() {
    if (m_paused) {
        unpause();
    } else {
        pause();
    }
}

void TimeManager::pause_with_transition(float duration) {
    transition_time_scale(0.0f, duration, ease_out_quad);
    // Actual pause will be set when transition completes
}

void TimeManager::unpause_with_transition(float duration) {
    m_paused = false;
    transition_time_scale(1.0f, duration, ease_in_quad);

    if (m_on_pause) {
        m_on_pause(false);
    }
}

// ============================================================================
// Hitstop
// ============================================================================

void TimeManager::apply_hitstop(float duration, float freeze_scale) {
    m_hitstop.duration = duration;
    m_hitstop.elapsed = 0.0f;
    m_hitstop.freeze_scale = freeze_scale;
    m_hitstop.active = true;

    log_debug("time", "Hitstop applied: {}s at scale {}", duration, freeze_scale);

    if (m_on_hitstop) {
        m_on_hitstop(true);
    }
}

void TimeManager::cancel_hitstop() {
    if (m_hitstop.active) {
        m_hitstop.active = false;

        if (m_on_hitstop) {
            m_on_hitstop(false);
        }
    }
}

float TimeManager::get_hitstop_remaining() const {
    if (!m_hitstop.active) return 0.0f;
    return std::max(0.0f, m_hitstop.duration - m_hitstop.elapsed);
}

// ============================================================================
// Delta Time Getters
// ============================================================================

float TimeManager::get_delta_time(TimeGroup group) const {
    size_t idx = static_cast<size_t>(group);
    if (idx < m_group_dt.size()) {
        return m_group_dt[idx];
    }
    return m_unscaled_dt;
}

// ============================================================================
// Slow Motion Presets
// ============================================================================

void TimeManager::bullet_time(float scale, float duration) {
    m_bullet_time_active = true;
    m_bullet_time_scale = scale;

    if (duration > 0.0f) {
        transition_time_scale(scale, 0.1f, ease_out_cubic);
        // Schedule end (handled externally or via timer)
    } else {
        set_time_scale(scale);
    }

    log_info("time", "Bullet time activated: scale {}", scale);
}

void TimeManager::end_bullet_time(float transition_duration) {
    if (m_bullet_time_active) {
        m_bullet_time_active = false;
        transition_time_scale(1.0f, transition_duration, ease_in_quad);

        log_info("time", "Bullet time ended");
    }
}

// ============================================================================
// Callbacks
// ============================================================================

void TimeManager::set_on_pause(std::function<void(bool)> callback) {
    m_on_pause = std::move(callback);
}

void TimeManager::set_on_time_scale_changed(std::function<void(float)> callback) {
    m_on_time_scale_changed = std::move(callback);
}

void TimeManager::set_on_hitstop(std::function<void(bool)> callback) {
    m_on_hitstop = std::move(callback);
}

// ============================================================================
// Update
// ============================================================================

void TimeManager::update(float raw_dt) {
    m_frame_count++;
    m_unscaled_dt = raw_dt;
    m_unscaled_total_time += raw_dt;

    // Update transitions (using unscaled time)
    update_transition(raw_dt);
    update_hitstop(raw_dt);

    // Calculate scaled delta times for each group
    for (size_t i = 0; i < static_cast<size_t>(TimeGroup::Count); ++i) {
        TimeGroup group = static_cast<TimeGroup>(i);
        float effective_scale = calculate_effective_scale(group);
        m_group_dt[i] = raw_dt * effective_scale;
    }

    // Update total times
    float gameplay_scale = calculate_effective_scale(TimeGroup::Gameplay);
    float scaled_dt = raw_dt * gameplay_scale;
    m_total_time += scaled_dt;

    if (!m_paused && gameplay_scale > 0.0f) {
        m_gameplay_time += scaled_dt;
    }
}

void TimeManager::update_transition(float raw_dt) {
    if (!m_transition.active) return;

    m_transition.elapsed += raw_dt;

    if (m_transition.elapsed >= m_transition.duration) {
        // Transition complete
        m_transition.active = false;
        set_time_scale(m_transition.to_scale);

        // If we transitioned to 0, set paused
        if (m_transition.to_scale <= 0.001f && !m_paused) {
            m_paused = true;
            if (m_on_pause) {
                m_on_pause(true);
            }
        }
    } else {
        // Interpolate
        float t = m_transition.elapsed / m_transition.duration;
        if (m_transition.easing) {
            t = m_transition.easing(t);
        }
        float new_scale = m_transition.from_scale + (m_transition.to_scale - m_transition.from_scale) * t;
        m_global_time_scale = new_scale;
    }
}

void TimeManager::update_hitstop(float raw_dt) {
    if (!m_hitstop.active) return;

    m_hitstop.elapsed += raw_dt;

    if (m_hitstop.elapsed >= m_hitstop.duration) {
        m_hitstop.active = false;

        if (m_on_hitstop) {
            m_on_hitstop(false);
        }
    }
}

void TimeManager::reset() {
    m_global_time_scale = 1.0f;
    m_paused = false;
    m_group_scales.fill(1.0f);
    m_group_dt.fill(0.0f);

    m_transition.active = false;
    m_hitstop.active = false;
    m_bullet_time_active = false;

    m_unscaled_dt = 0.0f;
    m_total_time = 0.0;
    m_unscaled_total_time = 0.0;
    m_gameplay_time = 0.0;
    m_frame_count = 0;

    log_info("time", "Time manager reset");
}

// ============================================================================
// Internal
// ============================================================================

float TimeManager::calculate_effective_scale(TimeGroup group) const {
    // UI is never affected by pause or time scale
    if (group == TimeGroup::UI) {
        return m_group_scales[static_cast<size_t>(group)];
    }

    // Start with global scale
    float scale = m_global_time_scale;

    // Apply group-specific scale
    scale *= m_group_scales[static_cast<size_t>(group)];

    // Apply pause (affects all except UI)
    if (m_paused) {
        scale = 0.0f;
    }

    // Apply hitstop (affects gameplay-related groups)
    if (m_hitstop.active) {
        if (group == TimeGroup::Gameplay || group == TimeGroup::Animation) {
            scale = m_hitstop.freeze_scale;
        }
    }

    return scale;
}

} // namespace engine::core
