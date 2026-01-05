#include <engine/script/bindings.hpp>
#include <engine/audio/audio_engine.hpp>
#include <engine/core/log.hpp>

namespace engine::script {

void register_audio_bindings(sol::state& lua) {
    using namespace engine::audio;
    using namespace engine::core;

    // SoundHandle type
    lua.new_usertype<SoundHandle>("SoundHandle",
        sol::constructors<>(),
        "valid", &SoundHandle::valid,
        "id", sol::readonly(&SoundHandle::id)
    );

    // MusicHandle type
    lua.new_usertype<MusicHandle>("MusicHandle",
        sol::constructors<>(),
        "valid", &MusicHandle::valid,
        "id", sol::readonly(&MusicHandle::id)
    );

    // Create Audio table
    auto audio = lua.create_named_table("Audio");

    // --- Sound Effects ---

    // Play sound (2D)
    audio.set_function("play", [](const std::string& path,
                                  sol::optional<float> volume,
                                  sol::optional<bool> loop) -> SoundHandle {
        auto& engine = get_audio_engine();
        return engine.play(path, volume.value_or(1.0f), loop.value_or(false));
    });

    // Play sound (3D)
    audio.set_function("play_3d", [](const std::string& path, const Vec3& position,
                                     sol::optional<float> volume,
                                     sol::optional<bool> loop) -> SoundHandle {
        auto& engine = get_audio_engine();
        return engine.play_3d(path, position, volume.value_or(1.0f), loop.value_or(false));
    });

    // Stop a sound
    audio.set_function("stop", [](SoundHandle handle) {
        auto& engine = get_audio_engine();
        engine.stop(handle);
    });

    // Pause a sound
    audio.set_function("pause", [](SoundHandle handle) {
        auto& engine = get_audio_engine();
        engine.pause(handle);
    });

    // Resume a sound
    audio.set_function("resume", [](SoundHandle handle) {
        auto& engine = get_audio_engine();
        engine.resume(handle);
    });

    // Set sound volume
    audio.set_function("set_volume", [](SoundHandle handle, float volume) {
        auto& engine = get_audio_engine();
        engine.set_volume(handle, volume);
    });

    // Set sound pitch
    audio.set_function("set_pitch", [](SoundHandle handle, float pitch) {
        auto& engine = get_audio_engine();
        engine.set_pitch(handle, pitch);
    });

    // Update 3D sound position
    audio.set_function("set_position", [](SoundHandle handle, const Vec3& position) {
        auto& engine = get_audio_engine();
        engine.set_sound_position(handle, position);
    });

    // Check if sound is playing
    audio.set_function("is_playing", [](SoundHandle handle) -> bool {
        auto& engine = get_audio_engine();
        return engine.is_sound_playing(handle);
    });

    // Fade in
    audio.set_function("fade_in", [](SoundHandle handle, float duration) {
        auto& engine = get_audio_engine();
        engine.fade_in(handle, duration);
    });

    // Fade out
    audio.set_function("fade_out", [](SoundHandle handle, float duration) {
        auto& engine = get_audio_engine();
        engine.fade_out(handle, duration);
    });

    // --- Music ---

    // Load music (streaming)
    audio.set_function("load_music", [](const std::string& path) -> MusicHandle {
        auto& engine = get_audio_engine();
        return engine.load_music(path);
    });

    // Unload music
    audio.set_function("unload_music", [](MusicHandle handle) {
        auto& engine = get_audio_engine();
        engine.unload_music(handle);
    });

    // Play music
    audio.set_function("play_music", [](MusicHandle handle, sol::optional<bool> loop) {
        auto& engine = get_audio_engine();
        engine.play_music(handle, loop.value_or(true));
    });

    // Pause music
    audio.set_function("pause_music", [](MusicHandle handle) {
        auto& engine = get_audio_engine();
        engine.pause_music(handle);
    });

    // Resume music
    audio.set_function("resume_music", [](MusicHandle handle) {
        auto& engine = get_audio_engine();
        engine.resume_music(handle);
    });

    // Stop music
    audio.set_function("stop_music", [](MusicHandle handle) {
        auto& engine = get_audio_engine();
        engine.stop_music(handle);
    });

    // Set music volume
    audio.set_function("set_music_volume", [](MusicHandle handle, float volume) {
        auto& engine = get_audio_engine();
        engine.set_music_volume(handle, volume);
    });

    // Crossfade between two music tracks
    audio.set_function("crossfade_music", [](MusicHandle from, MusicHandle to, float duration) {
        auto& engine = get_audio_engine();
        engine.crossfade_music(from, to, duration);
    });

    // Get music playback position
    audio.set_function("get_music_position", [](MusicHandle handle) -> float {
        auto& engine = get_audio_engine();
        return engine.get_music_position(handle);
    });

    // Set music playback position
    audio.set_function("set_music_position", [](MusicHandle handle, float seconds) {
        auto& engine = get_audio_engine();
        engine.set_music_position(handle, seconds);
    });

    // --- Global Controls ---

    // Set master volume
    audio.set_function("set_master_volume", [](float volume) {
        auto& engine = get_audio_engine();
        engine.set_master_volume(volume);
    });

    // Get master volume
    audio.set_function("get_master_volume", []() -> float {
        auto& engine = get_audio_engine();
        return engine.get_master_volume();
    });

    // Pause all audio
    audio.set_function("pause_all", []() {
        auto& engine = get_audio_engine();
        engine.pause_all();
    });

    // Resume all audio
    audio.set_function("resume_all", []() {
        auto& engine = get_audio_engine();
        engine.resume_all();
    });

    // Stop all audio
    audio.set_function("stop_all", []() {
        auto& engine = get_audio_engine();
        engine.stop_all();
    });

    // --- 3D Audio Listener (typically the camera/player) ---

    // Set listener position
    audio.set_function("set_listener_position", [](const Vec3& position) {
        auto& engine = get_audio_engine();
        engine.set_listener_position(position);
    });

    // Set listener orientation
    audio.set_function("set_listener_orientation", [](const Vec3& forward, const Vec3& up) {
        auto& engine = get_audio_engine();
        engine.set_listener_orientation(forward, up);
    });

    // Set listener velocity (for doppler effects)
    audio.set_function("set_listener_velocity", [](const Vec3& velocity) {
        auto& engine = get_audio_engine();
        engine.set_listener_velocity(velocity);
    });

    // --- Reverb Presets ---
    audio.set_function("set_reverb_preset", [](int preset) {
        auto& engine = get_audio_engine();
        engine.set_reverb_preset(static_cast<ReverbPreset>(preset));
    });

    // Reverb preset constants
    audio["REVERB_NONE"] = static_cast<int>(ReverbPreset::None);
    audio["REVERB_SMALL_ROOM"] = static_cast<int>(ReverbPreset::SmallRoom);
    audio["REVERB_MEDIUM_ROOM"] = static_cast<int>(ReverbPreset::MediumRoom);
    audio["REVERB_LARGE_ROOM"] = static_cast<int>(ReverbPreset::LargeRoom);
    audio["REVERB_HALL"] = static_cast<int>(ReverbPreset::Hall);
    audio["REVERB_CATHEDRAL"] = static_cast<int>(ReverbPreset::Cathedral);
    audio["REVERB_CAVE"] = static_cast<int>(ReverbPreset::Cave);
    audio["REVERB_UNDERWATER"] = static_cast<int>(ReverbPreset::Underwater);
    audio["REVERB_FOREST"] = static_cast<int>(ReverbPreset::Forest);
}

} // namespace engine::script
