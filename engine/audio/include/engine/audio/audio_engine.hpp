#pragma once

#include <engine/audio/sound.hpp>
#include <engine/core/math.hpp>
#include <engine/core/project_settings.hpp>
#include <memory>
#include <string>

namespace engine::audio {

using namespace engine::core;

// Audio engine - manages all audio playback
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // Non-copyable
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Initialization
    void init(const AudioSettings& settings);
    void shutdown();

    // Call each frame for 3D audio updates
    void update();

    // Sound effects (short, can have many instances)
    SoundHandle load_sound(const std::string& path);
    void unload_sound(SoundHandle h);
    void play_sound(SoundHandle h, const SoundConfig& config = {});
    void play_sound_3d(SoundHandle h, const Vec3& position, const SoundConfig& config = {});
    void stop_sound(SoundHandle h);

    // Music (streaming, typically one at a time)
    MusicHandle load_music(const std::string& path);
    void unload_music(MusicHandle h);
    void play_music(MusicHandle h, bool loop = true);
    void pause_music(MusicHandle h);
    void resume_music(MusicHandle h);
    void stop_music(MusicHandle h);
    void set_music_volume(MusicHandle h, float volume);
    float get_music_position(MusicHandle h) const;
    void set_music_position(MusicHandle h, float seconds);

    // Crossfade between two music tracks
    void crossfade_music(MusicHandle from, MusicHandle to, float duration);

    // Global controls
    void set_master_volume(float volume);
    float get_master_volume() const;
    void set_sound_volume(float volume);   // Volume for all sounds
    void set_music_volume(float volume);   // Volume for all music

    // 3D audio listener (typically the camera/player)
    void set_listener_position(const Vec3& pos);
    void set_listener_orientation(const Vec3& forward, const Vec3& up);
    void set_listener_velocity(const Vec3& vel);

    // Pause/resume all audio
    void pause_all();
    void resume_all();
    void stop_all();

    // Get active sound count
    uint32_t get_playing_sound_count() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Global audio engine instance
AudioEngine& get_audio_engine();

} // namespace engine::audio
