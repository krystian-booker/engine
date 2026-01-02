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
    // Forward declaration for pimpl (defined in miniaudio_impl.cpp)
    struct Impl;

    AudioEngine();
    ~AudioEngine();

    // Non-copyable
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Initialization
    void init(const AudioSettings& settings);
    void shutdown();

    // Call each frame for 3D audio updates and crossfade processing
    void update(float delta_time);

    // Sound effects (short, can have many instances)
    SoundHandle load_sound(const std::string& path);
    void unload_sound(SoundHandle h);
    void play_sound(SoundHandle h, const SoundConfig& config = {});
    void play_sound_3d(SoundHandle h, const Vec3& position, const SoundConfig& config = {});
    void stop_sound(SoundHandle h);

    // Update playing sound properties
    void set_sound_position(SoundHandle h, const Vec3& position);
    void set_sound_velocity(SoundHandle h, const Vec3& velocity);
    bool is_sound_playing(SoundHandle h) const;
    float get_sound_length(SoundHandle h) const;

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
    void fade_in(SoundHandle h, float duration);
    void fade_out(SoundHandle h, float duration);

    // Reverb control
    struct ReverbParams {
        float room_size = 0.5f;     // 0.0 to 1.0 (small to huge)
        float damping = 0.5f;       // 0.0 to 1.0
        float width = 1.0f;         // 0.0 to 1.0
        float wet_volume = 0.3f;    // 0.0 to 1.0
        float dry_volume = 1.0f;    // 0.0 to 1.0
        float mode = 0.0f;          // 0.0 = normal, 1.0 = freeze
    };
    void set_reverb_params(const ReverbParams& params);

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

    // Audio bus management
    AudioBusHandle get_bus(BuiltinBus bus);
    AudioBusHandle create_bus(const std::string& name, AudioBusHandle parent = {});
    void destroy_bus(AudioBusHandle bus);
    void set_bus_volume(AudioBusHandle bus, float volume);
    float get_bus_volume(AudioBusHandle bus) const;
    void set_bus_muted(AudioBusHandle bus, bool muted);
    bool is_bus_muted(AudioBusHandle bus) const;

    // Convenience helpers used by cinematic tracks (minimal implementations)
    SoundHandle play(const std::string& path, float volume = 1.0f, bool loop = false);
    void stop(SoundHandle h);
    void pause(SoundHandle h);
    void resume(SoundHandle h);
    void set_volume(SoundHandle h, float volume);
    void set_pitch(SoundHandle h, float pitch);
    friend Impl* create_audio_impl();
    friend void destroy_audio_impl(Impl*);
    friend void init_audio_impl(Impl*, const AudioSettings&);
    friend void set_reverb_params_impl(Impl*, const ReverbParams&);
    friend void shutdown_audio_impl(Impl*);
    friend void update_audio_impl(Impl*);
    friend SoundHandle load_sound_impl(Impl*, const std::string&);
    friend void unload_sound_impl(Impl*, SoundHandle);
    friend void play_sound_impl(Impl*, SoundHandle, const SoundConfig&);
    friend void play_sound_3d_impl(Impl*, SoundHandle, const Vec3&, const SoundConfig&);
    friend void stop_sound_impl(Impl*, SoundHandle);
    friend MusicHandle load_music_impl(Impl*, const std::string&);
    friend void unload_music_impl(Impl*, MusicHandle);
    friend void play_music_impl(Impl*, MusicHandle, bool);
    friend void pause_music_impl(Impl*, MusicHandle);
    friend void resume_music_impl(Impl*, MusicHandle);
    friend void stop_music_impl(Impl*, MusicHandle);
    friend void set_music_volume_impl(Impl*, MusicHandle, float);
    friend void set_master_volume_impl(Impl*, float);
    friend float get_master_volume_impl(Impl*);
    friend void set_listener_position_impl(Impl*, const Vec3&);
    friend void set_listener_orientation_impl(Impl*, const Vec3&, const Vec3&);
    friend void pause_all_impl(Impl*);
    friend void resume_all_impl(Impl*);
    friend void stop_all_impl(Impl*);
    friend uint32_t get_playing_sound_count_impl(Impl*);
};

// Global audio engine instance
AudioEngine& get_audio_engine();

} // namespace engine::audio
