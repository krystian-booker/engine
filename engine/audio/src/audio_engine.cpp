#include <engine/audio/audio_engine.hpp>
#include <engine/core/log.hpp>

namespace engine::audio {

using namespace engine::core;

// Forward declarations (implemented in miniaudio_impl.cpp)
struct AudioEngine::Impl;

extern AudioEngine::Impl* create_audio_impl();
extern void destroy_audio_impl(AudioEngine::Impl* impl);
extern void init_audio_impl(AudioEngine::Impl* impl, const AudioSettings& settings);
extern void shutdown_audio_impl(AudioEngine::Impl* impl);
extern void update_audio_impl(AudioEngine::Impl* impl, float delta_time);
extern SoundHandle load_sound_impl(AudioEngine::Impl* impl, const std::string& path);
extern void unload_sound_impl(AudioEngine::Impl* impl, SoundHandle h);
extern void play_sound_impl(AudioEngine::Impl* impl, SoundHandle h, const SoundConfig& config);
extern void play_sound_3d_impl(AudioEngine::Impl* impl, SoundHandle h, const Vec3& pos, const SoundConfig& config);
extern void stop_sound_impl(AudioEngine::Impl* impl, SoundHandle h);
extern void set_sound_position_impl(AudioEngine::Impl* impl, SoundHandle h, const Vec3& pos);
extern void set_sound_velocity_impl(AudioEngine::Impl* impl, SoundHandle h, const Vec3& vel);
extern bool is_sound_playing_impl(AudioEngine::Impl* impl, SoundHandle h);
extern float get_sound_length_impl(AudioEngine::Impl* impl, SoundHandle h);
extern MusicHandle load_music_impl(AudioEngine::Impl* impl, const std::string& path);
extern void unload_music_impl(AudioEngine::Impl* impl, MusicHandle h);
extern void play_music_impl(AudioEngine::Impl* impl, MusicHandle h, bool loop);
extern void pause_music_impl(AudioEngine::Impl* impl, MusicHandle h);
extern void resume_music_impl(AudioEngine::Impl* impl, MusicHandle h);
extern void stop_music_impl(AudioEngine::Impl* impl, MusicHandle h);
extern void set_music_volume_impl(AudioEngine::Impl* impl, MusicHandle h, float volume);
extern void set_master_volume_impl(AudioEngine::Impl* impl, float volume);
extern float get_master_volume_impl(AudioEngine::Impl* impl);
extern void set_listener_position_impl(AudioEngine::Impl* impl, const Vec3& pos);
extern void set_listener_orientation_impl(AudioEngine::Impl* impl, const Vec3& forward, const Vec3& up);
extern void set_listener_velocity_impl(AudioEngine::Impl* impl, const Vec3& vel);
extern void pause_all_impl(AudioEngine::Impl* impl);
extern void resume_all_impl(AudioEngine::Impl* impl);
extern void stop_all_impl(AudioEngine::Impl* impl);
extern uint32_t get_playing_sound_count_impl(AudioEngine::Impl* impl);
extern float get_music_position_impl(AudioEngine::Impl* impl, MusicHandle h);
extern void set_music_position_impl(AudioEngine::Impl* impl, MusicHandle h, float seconds);
extern void crossfade_music_impl(AudioEngine::Impl* impl, MusicHandle from, MusicHandle to, float duration);
extern void set_sound_volume_impl(AudioEngine::Impl* impl, float volume);
extern void set_music_volume_global_impl(AudioEngine::Impl* impl, float volume);
extern AudioBusHandle get_bus_impl(AudioEngine::Impl* impl, BuiltinBus bus);
extern AudioBusHandle create_bus_impl(AudioEngine::Impl* impl, const std::string& name, AudioBusHandle parent);
extern void destroy_bus_impl(AudioEngine::Impl* impl, AudioBusHandle bus);
extern void set_bus_volume_impl(AudioEngine::Impl* impl, AudioBusHandle bus, float volume);
extern float get_bus_volume_impl(AudioEngine::Impl* impl, AudioBusHandle bus);
extern void set_bus_muted_impl(AudioEngine::Impl* impl, AudioBusHandle bus, bool muted);
extern bool is_bus_muted_impl(AudioEngine::Impl* impl, AudioBusHandle bus);
extern void fade_in_impl(AudioEngine::Impl* impl, SoundHandle h, float duration);
extern void fade_out_impl(AudioEngine::Impl* impl, SoundHandle h, float duration);
extern void set_reverb_params_impl(AudioEngine::Impl* impl, const AudioEngine::ReverbParams& params);
extern void set_sound_paused_impl(AudioEngine::Impl* impl, SoundHandle h, bool paused);
extern void set_sound_volume_handle_impl(AudioEngine::Impl* impl, SoundHandle h, float volume);
extern void set_sound_pitch_handle_impl(AudioEngine::Impl* impl, SoundHandle h, float pitch);

extern void set_sound_pitch_handle_impl(AudioEngine::Impl* impl, SoundHandle h, float pitch);
extern void set_sound_attenuation_model_impl(AudioEngine::Impl* impl, SoundHandle h, AttenuationModel model);
extern void set_sound_rolloff_impl(AudioEngine::Impl* impl, SoundHandle h, float rolloff);
extern void set_sound_min_max_distance_impl(AudioEngine::Impl* impl, SoundHandle h, float min_dist, float max_dist);
extern void set_sound_cone_impl(AudioEngine::Impl* impl, SoundHandle h, float inner_angle_deg, float outer_angle_deg, float outer_gain);
extern void set_sound_doppler_factor_impl(AudioEngine::Impl* impl, SoundHandle h, float factor);

// Constructor and destructor defined in miniaudio_impl.cpp where Impl is complete

void AudioEngine::init(const AudioSettings& settings) {
    init_audio_impl(m_impl.get(), settings);
    log(LogLevel::Info, "Audio engine initialized");
}

void AudioEngine::shutdown() {
    shutdown_audio_impl(m_impl.get());
    log(LogLevel::Info, "Audio engine shutdown");
}

void AudioEngine::update(float delta_time) {
    update_audio_impl(m_impl.get(), delta_time);
}

SoundHandle AudioEngine::load_sound(const std::string& path) {
    return load_sound_impl(m_impl.get(), path);
}

void AudioEngine::unload_sound(SoundHandle h) {
    unload_sound_impl(m_impl.get(), h);
}

void AudioEngine::play_sound(SoundHandle h, const SoundConfig& config) {
    play_sound_impl(m_impl.get(), h, config);
}

void AudioEngine::play_sound_3d(SoundHandle h, const Vec3& position, const SoundConfig& config) {
    play_sound_3d_impl(m_impl.get(), h, position, config);
}

void AudioEngine::stop_sound(SoundHandle h) {
    stop_sound_impl(m_impl.get(), h);
}

void AudioEngine::set_sound_position(SoundHandle h, const Vec3& position) {
    set_sound_position_impl(m_impl.get(), h, position);
}

void AudioEngine::set_sound_velocity(SoundHandle h, const Vec3& velocity) {
    set_sound_velocity_impl(m_impl.get(), h, velocity);
}

bool AudioEngine::is_sound_playing(SoundHandle h) const {
    return is_sound_playing_impl(m_impl.get(), h);
}

float AudioEngine::get_sound_length(SoundHandle h) const {
    return get_sound_length_impl(m_impl.get(), h);
}

MusicHandle AudioEngine::load_music(const std::string& path) {
    return load_music_impl(m_impl.get(), path);
}

void AudioEngine::unload_music(MusicHandle h) {
    unload_music_impl(m_impl.get(), h);
}

void AudioEngine::play_music(MusicHandle h, bool loop) {
    play_music_impl(m_impl.get(), h, loop);
}

void AudioEngine::pause_music(MusicHandle h) {
    pause_music_impl(m_impl.get(), h);
}

void AudioEngine::resume_music(MusicHandle h) {
    resume_music_impl(m_impl.get(), h);
}

void AudioEngine::stop_music(MusicHandle h) {
    stop_music_impl(m_impl.get(), h);
}

void AudioEngine::set_music_volume(MusicHandle h, float volume) {
    set_music_volume_impl(m_impl.get(), h, volume);
}

float AudioEngine::get_music_position(MusicHandle h) const {
    return get_music_position_impl(m_impl.get(), h);
}

void AudioEngine::set_music_position(MusicHandle h, float seconds) {
    set_music_position_impl(m_impl.get(), h, seconds);
}

void AudioEngine::crossfade_music(MusicHandle from, MusicHandle to, float duration) {
    crossfade_music_impl(m_impl.get(), from, to, duration);
}

void AudioEngine::set_master_volume(float volume) {
    set_master_volume_impl(m_impl.get(), volume);
}

float AudioEngine::get_master_volume() const {
    return get_master_volume_impl(m_impl.get());
}

void AudioEngine::set_sound_volume(float volume) {
    set_sound_volume_impl(m_impl.get(), volume);
}

void AudioEngine::set_music_volume(float volume) {
    set_music_volume_global_impl(m_impl.get(), volume);
}

void AudioEngine::set_listener_position(const Vec3& pos) {
    set_listener_position_impl(m_impl.get(), pos);
}

void AudioEngine::set_listener_orientation(const Vec3& forward, const Vec3& up) {
    set_listener_orientation_impl(m_impl.get(), forward, up);
}

void AudioEngine::set_listener_velocity(const Vec3& vel) {
    set_listener_velocity_impl(m_impl.get(), vel);
}

void AudioEngine::pause_all() {
    pause_all_impl(m_impl.get());
}

void AudioEngine::resume_all() {
    resume_all_impl(m_impl.get());
}

void AudioEngine::stop_all() {
    stop_all_impl(m_impl.get());
}

uint32_t AudioEngine::get_playing_sound_count() const {
    return get_playing_sound_count_impl(m_impl.get());
}

AudioBusHandle AudioEngine::get_bus(BuiltinBus bus) {
    return get_bus_impl(m_impl.get(), bus);
}

AudioBusHandle AudioEngine::create_bus(const std::string& name, AudioBusHandle parent) {
    return create_bus_impl(m_impl.get(), name, parent);
}

void AudioEngine::destroy_bus(AudioBusHandle bus) {
    destroy_bus_impl(m_impl.get(), bus);
}

void AudioEngine::set_bus_volume(AudioBusHandle bus, float volume) {
    set_bus_volume_impl(m_impl.get(), bus, volume);
}

float AudioEngine::get_bus_volume(AudioBusHandle bus) const {
    return get_bus_volume_impl(m_impl.get(), bus);
}

void AudioEngine::set_bus_muted(AudioBusHandle bus, bool muted) {
    set_bus_muted_impl(m_impl.get(), bus, muted);
}

bool AudioEngine::is_bus_muted(AudioBusHandle bus) const {
    return is_bus_muted_impl(m_impl.get(), bus);
}

SoundHandle AudioEngine::play(const std::string& path, float volume, bool loop) {
    SoundHandle handle = load_sound(path);
    if (!handle.valid()) {
        return {};
    }

    SoundConfig config;
    config.volume = volume;
    config.loop = loop;
    play_sound(handle, config);
    return handle;
}

void AudioEngine::stop(SoundHandle h) {
    stop_sound(h);
}

void AudioEngine::set_reverb_params(const ReverbParams& params) {
    set_reverb_params_impl(m_impl.get(), params);
}

void AudioEngine::pause(SoundHandle h) {
    set_sound_paused_impl(m_impl.get(), h, true);
}

void AudioEngine::resume(SoundHandle h) {
    set_sound_paused_impl(m_impl.get(), h, false);
}

void AudioEngine::set_volume(SoundHandle h, float volume) {
    set_sound_volume_handle_impl(m_impl.get(), h, volume);
}

void AudioEngine::set_pitch(SoundHandle h, float pitch) {
    set_sound_pitch_handle_impl(m_impl.get(), h, pitch);
}

void AudioEngine::fade_in(SoundHandle h, float duration) {
    fade_in_impl(m_impl.get(), h, duration);
}

void AudioEngine::fade_out(SoundHandle h, float duration) {
    fade_out_impl(m_impl.get(), h, duration);
}

void AudioEngine::set_sound_attenuation_model(SoundHandle h, AttenuationModel model) {
    set_sound_attenuation_model_impl(m_impl.get(), h, model);
}

void AudioEngine::set_sound_rolloff(SoundHandle h, float rolloff) {
    set_sound_rolloff_impl(m_impl.get(), h, rolloff);
}

void AudioEngine::set_sound_min_max_distance(SoundHandle h, float min_dist, float max_dist) {
    set_sound_min_max_distance_impl(m_impl.get(), h, min_dist, max_dist);
}

void AudioEngine::set_sound_cone(SoundHandle h, float inner_angle_deg, float outer_angle_deg, float outer_gain) {
    set_sound_cone_impl(m_impl.get(), h, inner_angle_deg, outer_angle_deg, outer_gain);
}

void AudioEngine::set_sound_doppler_factor(SoundHandle h, float factor) {
    set_sound_doppler_factor_impl(m_impl.get(), h, factor);
}

// Global instance
static AudioEngine s_audio_engine;

AudioEngine& get_audio_engine() {
    return s_audio_engine;
}

} // namespace engine::audio
