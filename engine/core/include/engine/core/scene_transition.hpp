#pragma once

#include <engine/core/math.hpp>
#include <functional>
#include <string>
#include <memory>

namespace engine::core {

// Types of scene transitions
enum class TransitionType : uint8_t {
    None,           // Instant switch, no transition
    Fade,           // Fade to black then back
    FadeWhite,      // Fade to white then back
    FadeColor,      // Fade to custom color
    Crossfade,      // Blend between old and new scenes
};

// Current phase of a transition
enum class TransitionPhase : uint8_t {
    Idle,           // No transition active
    FadingOut,      // Transitioning out of current scene
    Loading,        // Loading new scene (at full fade)
    FadingIn,       // Transitioning into new scene
};

// Configuration for a scene transition
struct TransitionSettings {
    TransitionType type = TransitionType::Fade;
    Vec4 fade_color{0.0f, 0.0f, 0.0f, 1.0f};    // Color to fade to
    float fade_out_duration = 0.5f;              // Time to fade out
    float hold_duration = 0.0f;                  // Time to hold at full fade
    float fade_in_duration = 0.5f;               // Time to fade in
    float minimum_load_time = 0.0f;              // Minimum time to show loading

    // Factory methods for common presets
    static TransitionSettings fade_black(float duration = 0.5f);
    static TransitionSettings fade_white(float duration = 0.5f);
    static TransitionSettings instant();
};

// Callback types
using LoadingScreenCallback = std::function<void(float progress)>;
using TransitionCallback = std::function<void()>;
using SceneLoadCallback = std::function<bool(const std::string& scene_path)>;

// Scene transition manager - handles smooth transitions between scenes
class SceneTransitionManager {
public:
    static SceneTransitionManager& instance();

    // Delete copy/move
    SceneTransitionManager(const SceneTransitionManager&) = delete;
    SceneTransitionManager& operator=(const SceneTransitionManager&) = delete;

    // Configuration
    void set_scene_loader(SceneLoadCallback loader);
    void set_loading_screen_callback(LoadingScreenCallback callback);
    void set_minimum_load_time(float seconds);

    // Start a transition to a new scene
    void transition_to(const std::string& scene_path,
                       const TransitionSettings& settings = TransitionSettings{});

    // Manual transition control (for custom loading sequences)
    void begin_transition(const TransitionSettings& settings);
    void set_loading_progress(float progress);  // 0.0 to 1.0
    void end_transition();                       // Call when loading complete

    // State queries
    bool is_transitioning() const { return m_phase != TransitionPhase::Idle; }
    TransitionPhase get_phase() const { return m_phase; }
    float get_fade_alpha() const { return m_fade_alpha; }
    float get_loading_progress() const { return m_loading_progress; }
    const Vec4& get_fade_color() const { return m_settings.fade_color; }

    // Callbacks
    void on_fade_out_complete(TransitionCallback callback);
    void on_fade_in_complete(TransitionCallback callback);
    void on_transition_complete(TransitionCallback callback);

    // Update and render (called by Application)
    void update(float dt);

    // Returns true if the transition overlay should be rendered
    bool should_render_overlay() const;

private:
    SceneTransitionManager();
    ~SceneTransitionManager() = default;

    void start_fade_out();
    void start_loading();
    void start_fade_in();
    void complete_transition();

    // Current state
    TransitionPhase m_phase = TransitionPhase::Idle;
    TransitionSettings m_settings;
    std::string m_target_scene;

    // Timing
    float m_phase_time = 0.0f;
    float m_phase_duration = 0.0f;
    float m_load_start_time = 0.0f;
    float m_loading_progress = 0.0f;
    float m_fade_alpha = 0.0f;
    float m_minimum_load_time = 0.0f;

    // Callbacks
    SceneLoadCallback m_scene_loader;
    LoadingScreenCallback m_loading_screen_callback;
    TransitionCallback m_on_fade_out_complete;
    TransitionCallback m_on_fade_in_complete;
    TransitionCallback m_on_transition_complete;

    // Loading state
    bool m_scene_loaded = false;
};

// Convenience function
inline SceneTransitionManager& scene_transitions() {
    return SceneTransitionManager::instance();
}

} // namespace engine::core
