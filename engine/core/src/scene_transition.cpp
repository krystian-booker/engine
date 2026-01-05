#include <engine/core/scene_transition.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <cmath>

namespace engine::core {

// TransitionSettings factory methods
TransitionSettings TransitionSettings::fade_black(float duration) {
    TransitionSettings settings;
    settings.type = TransitionType::Fade;
    settings.fade_color = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
    settings.fade_out_duration = duration;
    settings.fade_in_duration = duration;
    return settings;
}

TransitionSettings TransitionSettings::fade_white(float duration) {
    TransitionSettings settings;
    settings.type = TransitionType::FadeWhite;
    settings.fade_color = Vec4{1.0f, 1.0f, 1.0f, 1.0f};
    settings.fade_out_duration = duration;
    settings.fade_in_duration = duration;
    return settings;
}

TransitionSettings TransitionSettings::instant() {
    TransitionSettings settings;
    settings.type = TransitionType::None;
    settings.fade_out_duration = 0.0f;
    settings.hold_duration = 0.0f;
    settings.fade_in_duration = 0.0f;
    return settings;
}

// SceneTransitionManager implementation
SceneTransitionManager::SceneTransitionManager() = default;

SceneTransitionManager& SceneTransitionManager::instance() {
    static SceneTransitionManager s_instance;
    return s_instance;
}

void SceneTransitionManager::set_scene_loader(SceneLoadCallback loader) {
    m_scene_loader = std::move(loader);
}

void SceneTransitionManager::set_loading_screen_callback(LoadingScreenCallback callback) {
    m_loading_screen_callback = std::move(callback);
}

void SceneTransitionManager::set_minimum_load_time(float seconds) {
    m_minimum_load_time = seconds;
}

void SceneTransitionManager::transition_to(const std::string& scene_path,
                                           const TransitionSettings& settings) {
    if (m_phase != TransitionPhase::Idle) {
        log(LogLevel::Warn, "SceneTransition: Cannot start transition while another is in progress");
        return;
    }

    m_target_scene = scene_path;
    m_settings = settings;
    m_scene_loaded = false;
    m_loading_progress = 0.0f;

    // Handle instant transitions
    if (settings.type == TransitionType::None) {
        if (m_scene_loader) {
            m_scene_loaded = m_scene_loader(m_target_scene);
        }
        if (m_on_transition_complete) {
            m_on_transition_complete();
        }
        return;
    }

    start_fade_out();
}

void SceneTransitionManager::begin_transition(const TransitionSettings& settings) {
    if (m_phase != TransitionPhase::Idle) {
        log(LogLevel::Warn, "SceneTransition: Cannot begin transition while another is in progress");
        return;
    }

    m_target_scene.clear();
    m_settings = settings;
    m_scene_loaded = false;
    m_loading_progress = 0.0f;

    if (settings.type == TransitionType::None) {
        return;
    }

    start_fade_out();
}

void SceneTransitionManager::set_loading_progress(float progress) {
    m_loading_progress = std::clamp(progress, 0.0f, 1.0f);
}

void SceneTransitionManager::end_transition() {
    if (m_phase == TransitionPhase::Loading) {
        m_scene_loaded = true;
    }
}

void SceneTransitionManager::on_fade_out_complete(TransitionCallback callback) {
    m_on_fade_out_complete = std::move(callback);
}

void SceneTransitionManager::on_fade_in_complete(TransitionCallback callback) {
    m_on_fade_in_complete = std::move(callback);
}

void SceneTransitionManager::on_transition_complete(TransitionCallback callback) {
    m_on_transition_complete = std::move(callback);
}

void SceneTransitionManager::update(float dt) {
    if (m_phase == TransitionPhase::Idle) {
        return;
    }

    m_phase_time += dt;

    switch (m_phase) {
        case TransitionPhase::FadingOut: {
            if (m_phase_duration > 0.0f) {
                float t = m_phase_time / m_phase_duration;
                m_fade_alpha = std::clamp(t, 0.0f, 1.0f);
            } else {
                m_fade_alpha = 1.0f;
            }

            if (m_phase_time >= m_phase_duration) {
                m_fade_alpha = 1.0f;
                start_loading();
            }
            break;
        }

        case TransitionPhase::Loading: {
            // Call loading screen callback
            if (m_loading_screen_callback) {
                m_loading_screen_callback(m_loading_progress);
            }

            // Auto-load scene if we have a loader and target
            if (!m_scene_loaded && m_scene_loader && !m_target_scene.empty()) {
                m_scene_loaded = m_scene_loader(m_target_scene);
                m_loading_progress = m_scene_loaded ? 1.0f : 0.0f;
            }

            // Check if we can proceed
            float time_in_loading = m_phase_time;
            bool min_time_elapsed = time_in_loading >= std::max(m_minimum_load_time, m_settings.hold_duration);
            bool load_complete = m_scene_loaded || m_target_scene.empty();

            if (min_time_elapsed && load_complete) {
                start_fade_in();
            }
            break;
        }

        case TransitionPhase::FadingIn: {
            if (m_phase_duration > 0.0f) {
                float t = m_phase_time / m_phase_duration;
                m_fade_alpha = std::clamp(1.0f - t, 0.0f, 1.0f);
            } else {
                m_fade_alpha = 0.0f;
            }

            if (m_phase_time >= m_phase_duration) {
                complete_transition();
            }
            break;
        }

        case TransitionPhase::Idle:
            break;
    }
}

bool SceneTransitionManager::should_render_overlay() const {
    return m_phase != TransitionPhase::Idle && m_fade_alpha > 0.001f;
}

void SceneTransitionManager::start_fade_out() {
    m_phase = TransitionPhase::FadingOut;
    m_phase_time = 0.0f;
    m_phase_duration = m_settings.fade_out_duration;
    m_fade_alpha = 0.0f;

    log(LogLevel::Debug, "SceneTransition: Starting fade out ({}s)", m_phase_duration);
}

void SceneTransitionManager::start_loading() {
    m_phase = TransitionPhase::Loading;
    m_phase_time = 0.0f;
    m_load_start_time = 0.0f;

    log(LogLevel::Debug, "SceneTransition: Starting loading phase");

    if (m_on_fade_out_complete) {
        m_on_fade_out_complete();
    }
}

void SceneTransitionManager::start_fade_in() {
    m_phase = TransitionPhase::FadingIn;
    m_phase_time = 0.0f;
    m_phase_duration = m_settings.fade_in_duration;

    log(LogLevel::Debug, "SceneTransition: Starting fade in ({}s)", m_phase_duration);
}

void SceneTransitionManager::complete_transition() {
    m_phase = TransitionPhase::Idle;
    m_phase_time = 0.0f;
    m_fade_alpha = 0.0f;
    m_loading_progress = 0.0f;

    log(LogLevel::Debug, "SceneTransition: Transition complete");

    if (m_on_fade_in_complete) {
        m_on_fade_in_complete();
    }

    if (m_on_transition_complete) {
        m_on_transition_complete();
    }

    // Clear one-shot callbacks
    m_on_fade_out_complete = nullptr;
    m_on_fade_in_complete = nullptr;
    m_on_transition_complete = nullptr;
}

} // namespace engine::core
