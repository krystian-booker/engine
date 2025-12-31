// miniaudio implementation
// This file contains all miniaudio-specific code

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <engine/audio/audio_engine.hpp>
#include <engine/core/log.hpp>
#include <unordered_map>
#include <vector>

namespace engine::audio {

using namespace engine::core;

struct AudioEngine::Impl {
    ma_engine engine;
    bool initialized = false;
    float master_volume = 1.0f;

    struct LoadedSound {
        ma_sound sound;
        std::string path;
        bool loaded = false;
    };

    std::unordered_map<uint32_t, LoadedSound> sounds;
    std::unordered_map<uint32_t, LoadedSound> music;
    uint32_t next_sound_id = 1;
    uint32_t next_music_id = 1;

    Vec3 listener_position{0.0f};
    Vec3 listener_forward{0.0f, 0.0f, -1.0f};
    Vec3 listener_up{0.0f, 1.0f, 0.0f};
};

AudioEngine::Impl* create_audio_impl() {
    return new AudioEngine::Impl();
}

void destroy_audio_impl(AudioEngine::Impl* impl) {
    if (impl) {
        shutdown_audio_impl(impl);
    }
    delete impl;
}

void init_audio_impl(AudioEngine::Impl* impl, const AudioSettings& settings) {
    if (!impl || impl->initialized) return;

    ma_engine_config config = ma_engine_config_init();
    config.channels = settings.channels;
    config.sampleRate = settings.sample_rate;

    if (ma_engine_init(&config, &impl->engine) != MA_SUCCESS) {
        log(LogLevel::Error, "Failed to initialize miniaudio engine");
        return;
    }

    impl->master_volume = settings.master_volume;
    ma_engine_set_volume(&impl->engine, settings.master_volume);

    impl->initialized = true;
}

void shutdown_audio_impl(AudioEngine::Impl* impl) {
    if (!impl || !impl->initialized) return;

    // Uninit all sounds
    for (auto& [id, sound] : impl->sounds) {
        if (sound.loaded) {
            ma_sound_uninit(&sound.sound);
        }
    }
    impl->sounds.clear();

    // Uninit all music
    for (auto& [id, music] : impl->music) {
        if (music.loaded) {
            ma_sound_uninit(&music.sound);
        }
    }
    impl->music.clear();

    ma_engine_uninit(&impl->engine);
    impl->initialized = false;
}

void update_audio_impl(AudioEngine::Impl* /*impl*/) {
    // miniaudio handles updates internally
}

SoundHandle load_sound_impl(AudioEngine::Impl* impl, const std::string& path) {
    if (!impl || !impl->initialized) return SoundHandle{};

    SoundHandle handle{impl->next_sound_id++};

    auto& sound = impl->sounds[handle.id];
    sound.path = path;

    if (ma_sound_init_from_file(&impl->engine, path.c_str(),
                                 MA_SOUND_FLAG_DECODE, nullptr, nullptr,
                                 &sound.sound) != MA_SUCCESS) {
        log(LogLevel::Error, ("Failed to load sound: " + path).c_str());
        impl->sounds.erase(handle.id);
        return SoundHandle{};
    }

    sound.loaded = true;
    return handle;
}

void unload_sound_impl(AudioEngine::Impl* impl, SoundHandle h) {
    if (!impl) return;
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end()) return;

    if (it->second.loaded) {
        ma_sound_uninit(&it->second.sound);
    }
    impl->sounds.erase(it);
}

void play_sound_impl(AudioEngine::Impl* impl, SoundHandle h, const SoundConfig& config) {
    if (!impl || !impl->initialized) return;
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    ma_sound_set_volume(&it->second.sound, config.volume);
    ma_sound_set_pitch(&it->second.sound, config.pitch);
    ma_sound_set_pan(&it->second.sound, config.pan);
    ma_sound_set_looping(&it->second.sound, config.loop);
    ma_sound_start(&it->second.sound);
}

void play_sound_3d_impl(AudioEngine::Impl* impl, SoundHandle h, const Vec3& pos, const SoundConfig& config) {
    if (!impl || !impl->initialized) return;
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    ma_sound_set_volume(&it->second.sound, config.volume);
    ma_sound_set_pitch(&it->second.sound, config.pitch);
    ma_sound_set_looping(&it->second.sound, config.loop);
    ma_sound_set_position(&it->second.sound, pos.x, pos.y, pos.z);
    ma_sound_set_spatialization_enabled(&it->second.sound, MA_TRUE);
    ma_sound_start(&it->second.sound);
}

void stop_sound_impl(AudioEngine::Impl* impl, SoundHandle h) {
    if (!impl) return;
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    ma_sound_stop(&it->second.sound);
}

MusicHandle load_music_impl(AudioEngine::Impl* impl, const std::string& path) {
    if (!impl || !impl->initialized) return MusicHandle{};

    MusicHandle handle{impl->next_music_id++};

    auto& music = impl->music[handle.id];
    music.path = path;

    // Stream music instead of decoding all at once
    if (ma_sound_init_from_file(&impl->engine, path.c_str(),
                                 MA_SOUND_FLAG_STREAM, nullptr, nullptr,
                                 &music.sound) != MA_SUCCESS) {
        log(LogLevel::Error, ("Failed to load music: " + path).c_str());
        impl->music.erase(handle.id);
        return MusicHandle{};
    }

    music.loaded = true;
    return handle;
}

void unload_music_impl(AudioEngine::Impl* impl, MusicHandle h) {
    if (!impl) return;
    auto it = impl->music.find(h.id);
    if (it == impl->music.end()) return;

    if (it->second.loaded) {
        ma_sound_uninit(&it->second.sound);
    }
    impl->music.erase(it);
}

void play_music_impl(AudioEngine::Impl* impl, MusicHandle h, bool loop) {
    if (!impl || !impl->initialized) return;
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    ma_sound_set_looping(&it->second.sound, loop);
    ma_sound_start(&it->second.sound);
}

void pause_music_impl(AudioEngine::Impl* impl, MusicHandle h) {
    if (!impl) return;
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    ma_sound_stop(&it->second.sound);
}

void resume_music_impl(AudioEngine::Impl* impl, MusicHandle h) {
    if (!impl) return;
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    ma_sound_start(&it->second.sound);
}

void stop_music_impl(AudioEngine::Impl* impl, MusicHandle h) {
    if (!impl) return;
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    ma_sound_stop(&it->second.sound);
    ma_sound_seek_to_pcm_frame(&it->second.sound, 0);
}

void set_music_volume_impl(AudioEngine::Impl* impl, MusicHandle h, float volume) {
    if (!impl) return;
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    ma_sound_set_volume(&it->second.sound, volume);
}

void set_master_volume_impl(AudioEngine::Impl* impl, float volume) {
    if (!impl || !impl->initialized) return;
    impl->master_volume = volume;
    ma_engine_set_volume(&impl->engine, volume);
}

float get_master_volume_impl(AudioEngine::Impl* impl) {
    if (!impl) return 1.0f;
    return impl->master_volume;
}

void set_listener_position_impl(AudioEngine::Impl* impl, const Vec3& pos) {
    if (!impl || !impl->initialized) return;
    impl->listener_position = pos;
    ma_engine_listener_set_position(&impl->engine, 0, pos.x, pos.y, pos.z);
}

void set_listener_orientation_impl(AudioEngine::Impl* impl, const Vec3& forward, const Vec3& up) {
    if (!impl || !impl->initialized) return;
    impl->listener_forward = forward;
    impl->listener_up = up;
    ma_engine_listener_set_direction(&impl->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&impl->engine, 0, up.x, up.y, up.z);
}

void pause_all_impl(AudioEngine::Impl* impl) {
    if (!impl || !impl->initialized) return;

    for (auto& [id, sound] : impl->sounds) {
        if (sound.loaded) {
            ma_sound_stop(&sound.sound);
        }
    }
    for (auto& [id, music] : impl->music) {
        if (music.loaded) {
            ma_sound_stop(&music.sound);
        }
    }
}

void resume_all_impl(AudioEngine::Impl* impl) {
    if (!impl || !impl->initialized) return;
    // Would need to track which sounds were playing before pause
}

void stop_all_impl(AudioEngine::Impl* impl) {
    if (!impl || !impl->initialized) return;

    for (auto& [id, sound] : impl->sounds) {
        if (sound.loaded) {
            ma_sound_stop(&sound.sound);
        }
    }
    for (auto& [id, music] : impl->music) {
        if (music.loaded) {
            ma_sound_stop(&music.sound);
            ma_sound_seek_to_pcm_frame(&music.sound, 0);
        }
    }
}

uint32_t get_playing_sound_count_impl(AudioEngine::Impl* impl) {
    if (!impl) return 0;

    uint32_t count = 0;
    for (auto& [id, sound] : impl->sounds) {
        if (sound.loaded && ma_sound_is_playing(&sound.sound)) {
            count++;
        }
    }
    return count;
}

} // namespace engine::audio
