#include <engine/cinematic/audio_track.hpp>
#include <engine/scene/world.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace engine::cinematic {

// ============================================================================
// AudioTrack
// ============================================================================

AudioTrack::AudioTrack(const std::string& name)
    : Track(name, TrackType::Audio) {
}

AudioTrack::~AudioTrack() {
    stop_all_sounds();
}

void AudioTrack::add_event(const AudioEvent& event) {
    m_events.push_back(event);
    m_event_triggered.push_back(false);
    sort_events();
}

void AudioTrack::remove_event(size_t index) {
    if (index < m_events.size()) {
        m_events.erase(m_events.begin() + index);
        m_event_triggered.erase(m_event_triggered.begin() + index);
    }
}

void AudioTrack::clear_events() {
    m_events.clear();
    m_event_triggered.clear();
}

void AudioTrack::add_volume_key(const VolumeKeyframe& key) {
    m_volume_keys.push_back(key);
    std::sort(m_volume_keys.begin(), m_volume_keys.end(),
        [](const VolumeKeyframe& a, const VolumeKeyframe& b) {
            return a.time < b.time;
        });
}

void AudioTrack::clear_volume_keys() {
    m_volume_keys.clear();
}

float AudioTrack::sample_volume(float time) const {
    if (m_volume_keys.empty()) {
        return m_master_volume;
    }

    if (time <= m_volume_keys.front().time) {
        return m_volume_keys.front().volume * m_master_volume;
    }

    if (time >= m_volume_keys.back().time) {
        return m_volume_keys.back().volume * m_master_volume;
    }

    for (size_t i = 0; i < m_volume_keys.size() - 1; ++i) {
        if (time >= m_volume_keys[i].time && time < m_volume_keys[i + 1].time) {
            float segment_duration = m_volume_keys[i + 1].time - m_volume_keys[i].time;
            float t = (time - m_volume_keys[i].time) / segment_duration;
            t = apply_easing(t, m_volume_keys[i].easing);
            float volume = interpolate_linear(m_volume_keys[i].volume,
                                              m_volume_keys[i + 1].volume, t);
            return volume * m_master_volume;
        }
    }

    return m_master_volume;
}

float AudioTrack::get_duration() const {
    float duration = 0.0f;
    for (const auto& event : m_events) {
        duration = std::max(duration, event.time);
    }
    if (!m_volume_keys.empty()) {
        duration = std::max(duration, m_volume_keys.back().time);
    }
    return duration;
}

void AudioTrack::evaluate(float time, scene::World& /*world*/) {
    if (!m_enabled || m_muted || !m_audio_engine) {
        return;
    }

    // Handle seeking backwards
    if (time < m_last_time) {
        // Reset triggered state for events after new time
        for (size_t i = 0; i < m_events.size(); ++i) {
            if (m_events[i].time > time) {
                m_event_triggered[i] = false;
            }
        }
    }

    // Process events in time range
    for (size_t i = 0; i < m_events.size(); ++i) {
        if (!m_event_triggered[i] && m_events[i].time > m_last_time && m_events[i].time <= time) {
            process_event(m_events[i], time);
            m_event_triggered[i] = true;
        }
    }

    // Update volume envelope
    float current_volume = sample_volume(time);
    for (const auto& [path, handle] : m_active_sounds) {
        if (handle.valid()) {
            m_audio_engine->set_volume(handle, current_volume);
        }
    }

    m_last_time = time;
}

void AudioTrack::reset() {
    stop_all_sounds();
    std::fill(m_event_triggered.begin(), m_event_triggered.end(), false);
    m_last_time = -1.0f;
}

void AudioTrack::sort_events() {
    // Sort events and triggered flags together
    std::vector<std::pair<AudioEvent, bool>> combined;
    for (size_t i = 0; i < m_events.size(); ++i) {
        combined.emplace_back(m_events[i], m_event_triggered[i]);
    }

    std::sort(combined.begin(), combined.end(),
        [](const auto& a, const auto& b) {
            return a.first.time < b.first.time;
        });

    for (size_t i = 0; i < combined.size(); ++i) {
        m_events[i] = combined[i].first;
        m_event_triggered[i] = combined[i].second;
    }
}

void AudioTrack::process_event(const AudioEvent& event, float /*current_time*/) {
    switch (event.type) {
        case AudioEventType::Play: {
            audio::SoundHandle handle;
            if (event.spatial) {
                handle = m_audio_engine->play_3d(
                    event.sound_path,
                    event.position,
                    event.volume * m_master_volume,
                    event.loop
                );
            } else {
                handle = m_audio_engine->play(
                    event.sound_path,
                    event.volume * m_master_volume,
                    event.loop
                );
            }
            if (handle.valid()) {
                m_active_sounds[event.sound_path] = handle;
                if (event.pitch != 1.0f) {
                    m_audio_engine->set_pitch(handle, event.pitch);
                }
            }
            break;
        }

        case AudioEventType::Stop: {
            // Stop specific sound by path
            auto it = m_active_sounds.find(event.sound_path);
            if (it != m_active_sounds.end()) {
                m_audio_engine->stop(it->second);
                m_active_sounds.erase(it);
            }
            break;
        }

        case AudioEventType::Pause:
            for (const auto& [path, handle] : m_active_sounds) {
                m_audio_engine->pause(handle);
            }
            break;

        case AudioEventType::Resume:
            for (const auto& [path, handle] : m_active_sounds) {
                m_audio_engine->resume(handle);
            }
            break;

        case AudioEventType::FadeIn: {
            audio::SoundHandle handle;
            if (event.spatial) {
                handle = m_audio_engine->play_3d(
                    event.sound_path,
                    event.position,
                    0.0f,
                    event.loop
                );
            } else {
                handle = m_audio_engine->play(
                    event.sound_path,
                    0.0f,
                    event.loop
                );
            }
            if (handle.valid()) {
                m_active_sounds[event.sound_path] = handle;
                m_audio_engine->fade_in(handle, event.fade_duration);
            }
            break;
        }

        case AudioEventType::FadeOut:
            for (const auto& [path, handle] : m_active_sounds) {
                m_audio_engine->fade_out(handle, event.fade_duration);
            }
            break;

        case AudioEventType::SetVolume:
            for (const auto& [path, handle] : m_active_sounds) {
                m_audio_engine->set_volume(handle, event.volume);
            }
            break;

        case AudioEventType::SetPitch:
            for (const auto& [path, handle] : m_active_sounds) {
                m_audio_engine->set_pitch(handle, event.pitch);
            }
            break;
    }
}

void AudioTrack::stop_all_sounds() {
    if (m_audio_engine) {
        for (const auto& [path, handle] : m_active_sounds) {
            m_audio_engine->stop(handle);
        }
    }
    m_active_sounds.clear();
}

// ============================================================================
// MusicTrack
// ============================================================================

MusicTrack::MusicTrack(const std::string& name)
    : Track(name, TrackType::Audio) {
}

void MusicTrack::add_music_cue(float time, const std::string& music_path, float fade_duration) {
    m_cues.push_back({time, music_path, fade_duration, false, 0.0f});
    std::sort(m_cues.begin(), m_cues.end(),
        [](const MusicCue& a, const MusicCue& b) { return a.time < b.time; });
}

void MusicTrack::add_stinger(float time, const std::string& stinger_path, float duck_amount) {
    m_cues.push_back({time, stinger_path, 0.0f, true, duck_amount});
    std::sort(m_cues.begin(), m_cues.end(),
        [](const MusicCue& a, const MusicCue& b) { return a.time < b.time; });
}

float MusicTrack::get_duration() const {
    if (m_cues.empty()) {
        return 0.0f;
    }
    return m_cues.back().time;
}

void MusicTrack::evaluate(float time, scene::World& /*world*/) {
    if (!m_enabled || m_muted || !m_audio_engine) {
        return;
    }

    // Find active cue
    for (size_t i = m_current_cue_index; i < m_cues.size(); ++i) {
        if (m_cues[i].time <= time) {
            if (i > m_current_cue_index || !m_current_music.valid()) {
                const MusicCue& cue = m_cues[i];

                if (cue.is_stinger) {
                    // Play stinger, duck main music
                    m_audio_engine->play(cue.music_path, 1.0f, false);
                    if (m_current_music.valid()) {
                        m_audio_engine->set_volume(m_current_music, 1.0f - cue.duck_amount);
                    }
                } else {
                    // Crossfade to new music
                    if (m_current_music.valid()) {
                        m_audio_engine->fade_out(m_current_music, cue.fade_duration);
                    }
                    m_current_music = m_audio_engine->play(cue.music_path, 0.0f, true);
                    m_audio_engine->fade_in(m_current_music, cue.fade_duration);
                }

                m_current_cue_index = i + 1;
            }
        }
    }
}

void MusicTrack::reset() {
    if (m_audio_engine && m_current_music.valid()) {
        m_audio_engine->stop(m_current_music);
    }
    m_current_music = {};
    m_current_cue_index = 0;
}

// ============================================================================
// AudioTrack Serialization
// ============================================================================

void AudioTrack::serialize(nlohmann::json& j) const {
    j["events"] = nlohmann::json::array();
    for (const auto& event : m_events) {
        j["events"].push_back({
            {"time", event.time},
            {"type", static_cast<int>(event.type)},
            {"sound_path", event.sound_path},
            {"volume", event.volume},
            {"pitch", event.pitch},
            {"fade_duration", event.fade_duration},
            {"loop", event.loop},
            {"spatial", event.spatial},
            {"position", {event.position.x, event.position.y, event.position.z}}
        });
    }

    j["volume_keys"] = nlohmann::json::array();
    for (const auto& key : m_volume_keys) {
        j["volume_keys"].push_back({
            {"time", key.time},
            {"volume", key.volume},
            {"easing", static_cast<int>(key.easing)}
        });
    }

    j["master_volume"] = m_master_volume;
}

void AudioTrack::deserialize(const nlohmann::json& j) {
    m_events.clear();
    m_volume_keys.clear();
    m_event_triggered.clear();

    if (j.contains("events")) {
        for (const auto& event_json : j["events"]) {
            AudioEvent event;
            event.time = event_json.value("time", 0.0f);
            event.type = static_cast<AudioEventType>(event_json.value("type", 0));
            event.sound_path = event_json.value("sound_path", "");
            event.volume = event_json.value("volume", 1.0f);
            event.pitch = event_json.value("pitch", 1.0f);
            event.fade_duration = event_json.value("fade_duration", 0.0f);
            event.loop = event_json.value("loop", false);
            event.spatial = event_json.value("spatial", false);
            if (event_json.contains("position")) {
                auto& pos = event_json["position"];
                event.position = Vec3{pos[0], pos[1], pos[2]};
            }
            m_events.push_back(event);
            m_event_triggered.push_back(false);
        }
    }

    if (j.contains("volume_keys")) {
        for (const auto& key_json : j["volume_keys"]) {
            VolumeKeyframe key;
            key.time = key_json.value("time", 0.0f);
            key.volume = key_json.value("volume", 1.0f);
            key.easing = static_cast<EaseType>(key_json.value("easing", 0));
            m_volume_keys.push_back(key);
        }
    }

    m_master_volume = j.value("master_volume", 1.0f);
}

// ============================================================================
// MusicTrack Serialization
// ============================================================================

void MusicTrack::serialize(nlohmann::json& j) const {
    j["cues"] = nlohmann::json::array();
    for (const auto& cue : m_cues) {
        j["cues"].push_back({
            {"time", cue.time},
            {"music_path", cue.music_path},
            {"fade_duration", cue.fade_duration},
            {"is_stinger", cue.is_stinger},
            {"duck_amount", cue.duck_amount}
        });
    }
}

void MusicTrack::deserialize(const nlohmann::json& j) {
    m_cues.clear();

    if (j.contains("cues")) {
        for (const auto& cue_json : j["cues"]) {
            MusicCue cue;
            cue.time = cue_json.value("time", 0.0f);
            cue.music_path = cue_json.value("music_path", "");
            cue.fade_duration = cue_json.value("fade_duration", 1.0f);
            cue.is_stinger = cue_json.value("is_stinger", false);
            cue.duck_amount = cue_json.value("duck_amount", 0.5f);
            m_cues.push_back(cue);
        }
    }
}

} // namespace engine::cinematic
