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
extern void update_audio_impl(AudioEngine::Impl* impl);
extern SoundHandle load_sound_impl(AudioEngine::Impl* impl, const std::string& path);
extern void unload_sound_impl(AudioEngine::Impl* impl, SoundHandle h);
extern void play_sound_impl(AudioEngine::Impl* impl, SoundHandle h, const SoundConfig& config);
extern void play_sound_3d_impl(AudioEngine::Impl* impl, SoundHandle h, const Vec3& pos, const SoundConfig& config);
extern void stop_sound_impl(AudioEngine::Impl* impl, SoundHandle h);
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
extern void pause_all_impl(AudioEngine::Impl* impl);
extern void resume_all_impl(AudioEngine::Impl* impl);
extern void stop_all_impl(AudioEngine::Impl* impl);
extern uint32_t get_playing_sound_count_impl(AudioEngine::Impl* impl);

// Constructor and destructor defined in miniaudio_impl.cpp where Impl is complete

void AudioEngine::init(const AudioSettings& settings) {
    init_audio_impl(m_impl.get(), settings);
    log(LogLevel::Info, "Audio engine initialized");
}

void AudioEngine::shutdown() {
    shutdown_audio_impl(m_impl.get());
    log(LogLevel::Info, "Audio engine shutdown");
}

void AudioEngine::update() {
    update_audio_impl(m_impl.get());
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

float AudioEngine::get_music_position(MusicHandle /*h*/) const {
    return 0.0f;  // TODO: Implement
}

void AudioEngine::set_music_position(MusicHandle /*h*/, float /*seconds*/) {
    // TODO: Implement
}

void AudioEngine::crossfade_music(MusicHandle /*from*/, MusicHandle /*to*/, float /*duration*/) {
    // TODO: Implement crossfade
}

void AudioEngine::set_master_volume(float volume) {
    set_master_volume_impl(m_impl.get(), volume);
}

float AudioEngine::get_master_volume() const {
    return get_master_volume_impl(m_impl.get());
}

void AudioEngine::set_sound_volume(float /*volume*/) {
    // TODO: Implement
}

void AudioEngine::set_music_volume(float /*volume*/) {
    // TODO: Implement
}

void AudioEngine::set_listener_position(const Vec3& pos) {
    set_listener_position_impl(m_impl.get(), pos);
}

void AudioEngine::set_listener_orientation(const Vec3& forward, const Vec3& up) {
    set_listener_orientation_impl(m_impl.get(), forward, up);
}

void AudioEngine::set_listener_velocity(const Vec3& /*vel*/) {
    // TODO: Implement
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

// Global instance
static AudioEngine s_audio_engine;

AudioEngine& get_audio_engine() {
    return s_audio_engine;
}

} // namespace engine::audio
