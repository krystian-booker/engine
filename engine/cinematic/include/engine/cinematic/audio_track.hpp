#pragma once

#include <engine/cinematic/track.hpp>
#include <engine/audio/audio_engine.hpp>
#include <cassert>
#include <string>
#include <unordered_map>

namespace engine::cinematic {

// Audio event types
enum class AudioEventType : uint8_t {
    Play,       // Start playing a sound
    Stop,       // Stop a playing sound
    Pause,      // Pause a sound
    Resume,     // Resume a paused sound
    FadeIn,     // Fade sound in
    FadeOut,    // Fade sound out
    SetVolume,  // Set volume at a point
    SetPitch    // Set pitch at a point
};

// Audio event on the timeline
struct AudioEvent {
    float time = 0.0f;
    AudioEventType type = AudioEventType::Play;
    std::string sound_path;

    // Playback parameters
    float volume = 1.0f;
    float pitch = 1.0f;
    float fade_duration = 0.0f;
    bool loop = false;

    // 3D audio (optional)
    bool spatial = false;
    Vec3 position{0.0f};
};

// Volume envelope keyframe
struct VolumeKeyframe : KeyframeBase {
    float volume = 1.0f;

    VolumeKeyframe() = default;
    VolumeKeyframe(float t, float vol) : volume(vol) { time = t; }
};

// Audio track for controlling sounds during cinematics
class AudioTrack : public Track {
public:
    explicit AudioTrack(const std::string& name);
    ~AudioTrack() override;

    // Set audio engine reference
    void set_audio_engine(audio::AudioEngine* engine) { m_audio_engine = engine; }

    // Add audio events
    void add_event(const AudioEvent& event);
    void remove_event(size_t index);
    void clear_events();

    size_t event_count() const { return m_events.size(); }
    AudioEvent& get_event(size_t index) {
        assert(index < m_events.size() && "Event index out of bounds");
        return m_events[index];
    }
    const AudioEvent& get_event(size_t index) const {
        assert(index < m_events.size() && "Event index out of bounds");
        return m_events[index];
    }

    // Volume envelope (overall track volume)
    void add_volume_key(const VolumeKeyframe& key);
    void clear_volume_keys();
    float sample_volume(float time) const;

    // Master track volume
    void set_master_volume(float volume) { m_master_volume = volume; }
    float get_master_volume() const { return m_master_volume; }

    // Track interface
    float get_duration() const override;
    void evaluate(float time, scene::World& world) override;
    void reset() override;

    // Serialization
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    void sort_events();
    void process_event(const AudioEvent& event, float current_time);
    void stop_all_sounds();

    std::vector<AudioEvent> m_events;
    std::vector<VolumeKeyframe> m_volume_keys;

    audio::AudioEngine* m_audio_engine = nullptr;
    float m_master_volume = 1.0f;

    // Track which events have been triggered
    std::vector<bool> m_event_triggered;
    float m_last_time = -1.0f;

    // Active sound handles for cleanup (path -> handle)
    std::unordered_map<std::string, audio::SoundHandle> m_active_sounds;
};

// Music track (specialized for background music with crossfading)
class MusicTrack : public Track {
public:
    explicit MusicTrack(const std::string& name);
    ~MusicTrack() override = default;

    void set_audio_engine(audio::AudioEngine* engine) { m_audio_engine = engine; }

    // Add music cues
    void add_music_cue(float time, const std::string& music_path, float fade_duration = 1.0f);
    void add_stinger(float time, const std::string& stinger_path, float duck_amount = 0.5f);

    // Track interface
    float get_duration() const override;
    void evaluate(float time, scene::World& world) override;
    void reset() override;

    // Serialization
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    struct MusicCue {
        float time;
        std::string music_path;
        float fade_duration;
        bool is_stinger;
        float duck_amount;
    };

    std::vector<MusicCue> m_cues;
    audio::AudioEngine* m_audio_engine = nullptr;

    audio::SoundHandle m_current_music;
    size_t m_current_cue_index = 0;
};

} // namespace engine::cinematic
