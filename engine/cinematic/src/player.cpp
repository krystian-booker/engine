#include <engine/cinematic/player.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <cmath>

namespace engine::cinematic {

// ============================================================================
// SequencePlayer
// ============================================================================

SequencePlayer::SequencePlayer() = default;
SequencePlayer::~SequencePlayer() = default;

void SequencePlayer::load(std::unique_ptr<Sequence> sequence) {
    stop();
    m_owned_sequence = std::move(sequence);
    m_sequence = m_owned_sequence.get();
    m_current_time = 0.0f;

    if (m_sequence) {
        m_frame_rate = m_sequence->get_info().frame_rate;
    }
}

void SequencePlayer::load(Sequence* sequence) {
    stop();
    m_owned_sequence.reset();
    m_sequence = sequence;
    m_current_time = 0.0f;

    if (m_sequence) {
        m_frame_rate = m_sequence->get_info().frame_rate;
    }
}

void SequencePlayer::load(const std::string& path) {
    auto sequence = std::make_unique<Sequence>();
    if (sequence->load(path)) {
        load(std::move(sequence));
    } else {
        core::log(core::LogLevel::Error, "Failed to load sequence: {}", path);
    }
}

void SequencePlayer::unload() {
    stop();
    m_owned_sequence.reset();
    m_sequence = nullptr;
}

void SequencePlayer::play() {
    if (!m_sequence) return;

    if (m_state == PlaybackState::Stopped) {
        m_sequence->reset();
        fire_event(PlaybackEvent::Started);
    } else if (m_state == PlaybackState::Paused) {
        fire_event(PlaybackEvent::Resumed);
    }

    m_state = PlaybackState::Playing;
}

void SequencePlayer::pause() {
    if (m_state == PlaybackState::Playing) {
        m_state = PlaybackState::Paused;
        fire_event(PlaybackEvent::Paused);
    }
}

void SequencePlayer::stop() {
    if (m_state != PlaybackState::Stopped) {
        m_state = PlaybackState::Stopped;
        m_current_time = 0.0f;
        if (m_sequence) {
            m_sequence->reset();
        }
        fire_event(PlaybackEvent::Stopped);
    }
}

void SequencePlayer::toggle_play_pause() {
    if (m_state == PlaybackState::Playing) {
        pause();
    } else {
        play();
    }
}

void SequencePlayer::seek(float time) {
    float old_time = m_current_time;
    float duration = get_duration();

    m_current_time = std::clamp(time, 0.0f, duration);

    if (m_sequence) {
        check_markers(old_time, m_current_time);
        check_sections(old_time, m_current_time);
        m_sequence->evaluate(m_current_time);
    }
}

void SequencePlayer::seek_to_start() {
    seek(m_use_play_range ? m_play_range_start : 0.0f);
}

void SequencePlayer::seek_to_end() {
    seek(m_use_play_range ? m_play_range_end : get_duration());
}

void SequencePlayer::seek_to_marker(const std::string& marker_name) {
    if (m_sequence) {
        float time = m_sequence->get_marker_time(marker_name);
        if (time >= 0.0f) {
            seek(time);
        }
    }
}

void SequencePlayer::step_forward() {
    if (m_frame_rate > 0) {
        seek(m_current_time + 1.0f / m_frame_rate);
    }
}

void SequencePlayer::step_backward() {
    if (m_frame_rate > 0) {
        seek(m_current_time - 1.0f / m_frame_rate);
    }
}

void SequencePlayer::set_play_range(float start, float end) {
    m_use_play_range = true;
    m_play_range_start = start;
    m_play_range_end = end;
}

void SequencePlayer::clear_play_range() {
    m_use_play_range = false;
}

float SequencePlayer::get_duration() const {
    return m_sequence ? m_sequence->get_duration() : 0.0f;
}

float SequencePlayer::get_progress() const {
    float duration = get_duration();
    return duration > 0.0f ? m_current_time / duration : 0.0f;
}

void SequencePlayer::update(scene::World& /*world*/, float delta_time) {
    if (m_state != PlaybackState::Playing || !m_sequence) {
        return;
    }

    float old_time = m_current_time;
    float duration = m_use_play_range ? m_play_range_end : get_duration();
    float start = m_use_play_range ? m_play_range_start : 0.0f;

    // Apply playback speed and direction
    float time_delta = delta_time * m_playback_speed;
    if (m_direction == PlaybackDirection::Backward) {
        time_delta = -time_delta;
    }

    m_current_time += time_delta;

    // Handle boundaries
    if (m_direction == PlaybackDirection::Forward) {
        if (m_current_time >= duration) {
            if (m_looping) {
                m_current_time = start + std::fmod(m_current_time - start, duration - start);
                fire_event(PlaybackEvent::Looped);
                m_sequence->reset();
            } else {
                m_current_time = duration;
                stop();
                fire_event(PlaybackEvent::Finished);
                return;
            }
        }
    } else {
        if (m_current_time <= start) {
            if (m_looping) {
                m_current_time = duration - std::fmod(start - m_current_time, duration - start);
                fire_event(PlaybackEvent::Looped);
                m_sequence->reset();
            } else {
                m_current_time = start;
                stop();
                fire_event(PlaybackEvent::Finished);
                return;
            }
        }
    }

    // Check markers and sections
    check_markers(old_time, m_current_time);
    check_sections(old_time, m_current_time);

    // Evaluate all tracks
    m_sequence->evaluate(m_current_time);
}

void SequencePlayer::add_skip_point(float time) {
    m_skip_points.push_back(time);
    std::sort(m_skip_points.begin(), m_skip_points.end());
}

void SequencePlayer::skip_to_next_point() {
    if (!m_skip_enabled || m_skip_points.empty()) {
        return;
    }

    for (float point : m_skip_points) {
        if (point > m_current_time + 0.1f) { // Small epsilon to avoid getting stuck
            seek(point);
            return;
        }
    }

    // No more skip points, go to end
    seek(get_duration());
}

bool SequencePlayer::can_skip() const {
    if (!m_skip_enabled || m_skip_points.empty()) {
        return false;
    }

    for (float point : m_skip_points) {
        if (point > m_current_time + 0.1f) {
            return true;
        }
    }
    return false;
}

float SequencePlayer::get_blend_weight() const {
    float duration = get_duration();
    float weight = 1.0f;

    // Blend in
    if (m_blend_in_time > 0.0f && m_current_time < m_blend_in_time) {
        weight *= m_current_time / m_blend_in_time;
    }

    // Blend out
    if (m_blend_out_time > 0.0f && duration - m_current_time < m_blend_out_time) {
        weight *= (duration - m_current_time) / m_blend_out_time;
    }

    return weight;
}

void SequencePlayer::fire_event(PlaybackEvent event, const std::string& data) {
    if (m_event_callback) {
        m_event_callback(event, data);
    }
}

void SequencePlayer::check_markers(float old_time, float new_time) {
    if (!m_sequence) return;

    for (const auto& [name, time] : m_sequence->get_markers()) {
        bool crossed = (old_time < time && new_time >= time) ||
                       (old_time > time && new_time <= time);
        if (crossed) {
            fire_event(PlaybackEvent::MarkerReached, name);
        }
    }
}

void SequencePlayer::check_sections(float old_time, float new_time) {
    if (!m_sequence) return;

    for (const auto& section : m_sequence->get_sections()) {
        bool was_inside = old_time >= section.start_time && old_time < section.end_time;
        bool is_inside = new_time >= section.start_time && new_time < section.end_time;

        if (!was_inside && is_inside) {
            fire_event(PlaybackEvent::SectionEntered, section.name);
            m_current_section = section.name;
        } else if (was_inside && !is_inside) {
            fire_event(PlaybackEvent::SectionExited, section.name);
            m_current_section.clear();
        }
    }
}

// ============================================================================
// CinematicManager
// ============================================================================

CinematicManager& CinematicManager::instance() {
    static CinematicManager instance;
    return instance;
}

void CinematicManager::register_sequence(const std::string& name, std::unique_ptr<Sequence> sequence) {
    m_sequences[name] = std::move(sequence);
}

void CinematicManager::unregister_sequence(const std::string& name) {
    m_sequences.erase(name);
}

Sequence* CinematicManager::get_sequence(const std::string& name) {
    auto it = m_sequences.find(name);
    return it != m_sequences.end() ? it->second.get() : nullptr;
}

SequencePlayer* CinematicManager::play_sequence(const std::string& name) {
    Sequence* seq = get_sequence(name);
    if (!seq) {
        core::log(core::LogLevel::Warn, "Sequence not found: {}", name);
        return nullptr;
    }

    m_active_player = std::make_unique<SequencePlayer>();
    m_active_player->load(seq);
    m_active_player->play();

    return m_active_player.get();
}

void CinematicManager::stop_all() {
    if (m_active_player) {
        m_active_player->stop();
    }
    for (auto& player : m_background_players) {
        player->stop();
    }
}

void CinematicManager::update(scene::World& world, float delta_time) {
    if (m_active_player) {
        m_active_player->update(world, delta_time);

        // Clean up finished player
        if (m_active_player->is_stopped()) {
            m_active_player.reset();
        }
    }

    // Update background players
    for (auto it = m_background_players.begin(); it != m_background_players.end();) {
        (*it)->update(world, delta_time);
        if ((*it)->is_stopped()) {
            it = m_background_players.erase(it);
        } else {
            ++it;
        }
    }
}

void CinematicManager::preload(const std::string& path) {
    auto sequence = std::make_unique<Sequence>();
    if (sequence->load(path)) {
        register_sequence(sequence->get_name(), std::move(sequence));
    }
}

void CinematicManager::preload_async(const std::string& /*path*/) {
    // Would use job system for async loading
    // JobSystem::instance().schedule([path, this]() {
    //     preload(path);
    // });
}

} // namespace engine::cinematic
