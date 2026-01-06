#pragma once

#include <functional>
#include <array>
#include <cstdint>

namespace engine::core {

// ============================================================================
// Time Group - Different systems can have independent time scales
// ============================================================================

enum class TimeGroup : uint8_t {
    Gameplay,       // Affected by pause and slow-mo
    UI,             // Never paused
    Physics,        // May have separate scale
    Animation,      // May be decoupled
    Audio,          // Music continues, SFX may slow
    Count
};

// ============================================================================
// Time Scale Transition - Smooth time scale changes
// ============================================================================

struct TimeScaleTransition {
    float from_scale = 1.0f;
    float to_scale = 1.0f;
    float duration = 0.0f;
    float elapsed = 0.0f;
    std::function<float(float)> easing;  // Easing function (t -> t')
    bool active = false;
};

// ============================================================================
// Hitstop State
// ============================================================================

struct HitstopState {
    float duration = 0.0f;
    float elapsed = 0.0f;
    float freeze_scale = 0.0f;  // Time scale during hitstop (0 = complete freeze)
    bool active = false;
};

// ============================================================================
// Time Manager - Centralized time control
// ============================================================================

class TimeManager {
public:
    // Singleton access
    static TimeManager& instance();

    // Delete copy/move
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;

    // ========================================================================
    // Global Time Scale
    // ========================================================================

    // Set global time scale (affects Gameplay group)
    void set_time_scale(float scale);
    float get_time_scale() const { return m_global_time_scale; }

    // Smooth time scale transitions
    void transition_time_scale(float target, float duration,
                               std::function<float(float)> easing = nullptr);
    void cancel_transition();
    bool is_transitioning() const { return m_transition.active; }
    float get_transition_progress() const;

    // ========================================================================
    // Per-Group Time Scales
    // ========================================================================

    void set_group_scale(TimeGroup group, float scale);
    float get_group_scale(TimeGroup group) const;
    float get_effective_scale(TimeGroup group) const;

    // ========================================================================
    // Pause System
    // ========================================================================

    void pause();
    void unpause();
    void toggle_pause();
    bool is_paused() const { return m_paused; }

    // Pause/unpause with fade
    void pause_with_transition(float duration);
    void unpause_with_transition(float duration);

    // ========================================================================
    // Hitstop
    // ========================================================================

    // Apply hitstop effect (brief freeze for combat feedback)
    void apply_hitstop(float duration, float freeze_scale = 0.0f);
    void cancel_hitstop();
    bool is_hitstop_active() const { return m_hitstop.active; }
    float get_hitstop_remaining() const;

    // ========================================================================
    // Delta Time Getters
    // ========================================================================

    // Get delta time for a specific group
    float get_delta_time(TimeGroup group = TimeGroup::Gameplay) const;

    // Get raw unscaled delta time
    float get_unscaled_delta_time() const { return m_unscaled_dt; }

    // Get fixed timestep delta time (for physics)
    float get_fixed_delta_time() const { return m_fixed_dt; }

    // Set fixed timestep
    void set_fixed_delta_time(float dt) { m_fixed_dt = dt; }

    // ========================================================================
    // Total Time
    // ========================================================================

    // Total elapsed time (scaled)
    double get_total_time() const { return m_total_time; }

    // Total elapsed time (unscaled, real time)
    double get_unscaled_total_time() const { return m_unscaled_total_time; }

    // Total gameplay time (paused time not counted)
    double get_gameplay_time() const { return m_gameplay_time; }

    // ========================================================================
    // Frame Counting
    // ========================================================================

    uint64_t get_frame_count() const { return m_frame_count; }

    // ========================================================================
    // Slow Motion Presets
    // ========================================================================

    // Apply bullet-time effect
    void bullet_time(float scale = 0.3f, float duration = -1.0f);

    // End bullet-time
    void end_bullet_time(float transition_duration = 0.3f);

    // ========================================================================
    // Callbacks
    // ========================================================================

    void set_on_pause(std::function<void(bool)> callback);
    void set_on_time_scale_changed(std::function<void(float)> callback);
    void set_on_hitstop(std::function<void(bool)> callback);

    // ========================================================================
    // Update (called by Application)
    // ========================================================================

    // Update time manager with raw (unscaled) delta time
    void update(float raw_dt);

    // Reset all time state
    void reset();

    // ========================================================================
    // Common Easing Functions
    // ========================================================================

    static float ease_linear(float t) { return t; }
    static float ease_in_quad(float t) { return t * t; }
    static float ease_out_quad(float t) { return t * (2.0f - t); }
    static float ease_in_out_quad(float t) {
        return t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) / 2.0f;
    }
    static float ease_in_cubic(float t) { return t * t * t; }
    static float ease_out_cubic(float t) {
        float f = t - 1.0f;
        return f * f * f + 1.0f;
    }
    static float ease_in_out_cubic(float t) {
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) / 2.0f;
    }

private:
    TimeManager();
    ~TimeManager() = default;

    void update_transition(float raw_dt);
    void update_hitstop(float raw_dt);
    float calculate_effective_scale(TimeGroup group) const;

    // Global time scale
    float m_global_time_scale = 1.0f;
    bool m_paused = false;

    // Per-group scales
    std::array<float, static_cast<size_t>(TimeGroup::Count)> m_group_scales;

    // Transitions
    TimeScaleTransition m_transition;
    HitstopState m_hitstop;

    // Bullet time state
    bool m_bullet_time_active = false;
    float m_bullet_time_scale = 0.3f;

    // Delta times
    float m_unscaled_dt = 0.0f;
    float m_fixed_dt = 1.0f / 60.0f;  // Default 60 FPS fixed step
    std::array<float, static_cast<size_t>(TimeGroup::Count)> m_group_dt;

    // Total times
    double m_total_time = 0.0;
    double m_unscaled_total_time = 0.0;
    double m_gameplay_time = 0.0;

    // Frame count
    uint64_t m_frame_count = 0;

    // Callbacks
    std::function<void(bool)> m_on_pause;
    std::function<void(float)> m_on_time_scale_changed;
    std::function<void(bool)> m_on_hitstop;
};

// ============================================================================
// Global Access
// ============================================================================

inline TimeManager& time_manager() { return TimeManager::instance(); }

} // namespace engine::core
