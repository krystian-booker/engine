// miniaudio implementation
// This file contains all miniaudio-specific code

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <engine/audio/audio_engine.hpp>
#include <engine/core/log.hpp>
#include <unordered_map>
#include <vector>
#include <optional>
#include <algorithm>
#include <chrono>

namespace engine::audio {

using namespace engine::core;

struct AudioEngine::Impl {
    ma_engine engine;
    bool initialized = false;
    float master_volume = 1.0f;
    float sound_volume = 1.0f;   // Global volume for all sounds
    float music_volume = 1.0f;   // Global volume for all music

    struct LoadedSound {
        ma_sound sound;
        std::string path;
        bool loaded = false;
        bool was_playing = false;     // For pause_all/resume_all tracking
        ma_uint64 paused_cursor = 0;  // For proper pause/resume position
    };

    std::unordered_map<uint32_t, LoadedSound> sounds;
    std::unordered_map<uint32_t, LoadedSound> music;
    uint32_t next_sound_id = 1;
    uint32_t next_music_id = 1;

    Vec3 listener_position{0.0f};
    Vec3 listener_forward{0.0f, 0.0f, -1.0f};
    Vec3 listener_up{0.0f, 1.0f, 0.0f};
    Vec3 listener_velocity{0.0f};

    // Audio bus system
    struct AudioBus {
        std::string name;
        float volume = 1.0f;
        bool muted = false;
        AudioBusHandle parent;
    };
    std::unordered_map<uint32_t, AudioBus> buses;
    uint32_t next_bus_id = 100;  // Start after builtin buses

    // Crossfade state
    struct CrossfadeState {
        MusicHandle from;
        MusicHandle to;
        float duration = 1.0f;
        float elapsed = 0.0f;
        float from_start_volume = 1.0f;
        float to_start_volume = 0.0f;
    };
    std::optional<CrossfadeState> active_crossfade;
    float last_update_time = 0.0f;

    // ID allocation with overflow protection
    uint32_t allocate_sound_id() {
        uint32_t start = next_sound_id;
        do {
            if (next_sound_id == UINT32_MAX) next_sound_id = 1;
            if (sounds.find(next_sound_id) == sounds.end()) {
                return next_sound_id++;
            }
            next_sound_id++;
        } while (next_sound_id != start);
        return UINT32_MAX;  // No available IDs
    }

    uint32_t allocate_music_id() {
        uint32_t start = next_music_id;
        do {
            if (next_music_id == UINT32_MAX) next_music_id = 1;
            if (music.find(next_music_id) == music.end()) {
                return next_music_id++;
            }
            next_music_id++;
        } while (next_music_id != start);
        return UINT32_MAX;  // No available IDs
    }

    static float clamp_volume(float v) {
        return std::clamp(v, 0.0f, 2.0f);  // Allow boost up to 2x
    }
};

// Forward declarations for impl functions
AudioEngine::Impl* create_audio_impl();
void shutdown_audio_impl(AudioEngine::Impl* impl);

// Constructor and destructor must be defined here where Impl is complete
AudioEngine::AudioEngine()
    : m_impl(create_audio_impl())
{
}

AudioEngine::~AudioEngine() {
    if (m_impl) {
        shutdown_audio_impl(m_impl.get());
    }
}

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

    // Initialize builtin buses
    impl->buses[static_cast<uint32_t>(BuiltinBus::Master)] = {"Master", 1.0f, false, {}};
    impl->buses[static_cast<uint32_t>(BuiltinBus::Music)] = {"Music", 1.0f, false, AudioBusHandle{static_cast<uint32_t>(BuiltinBus::Master)}};
    impl->buses[static_cast<uint32_t>(BuiltinBus::SFX)] = {"SFX", 1.0f, false, AudioBusHandle{static_cast<uint32_t>(BuiltinBus::Master)}};
    impl->buses[static_cast<uint32_t>(BuiltinBus::Voice)] = {"Voice", 1.0f, false, AudioBusHandle{static_cast<uint32_t>(BuiltinBus::Master)}};
    impl->buses[static_cast<uint32_t>(BuiltinBus::Ambient)] = {"Ambient", 1.0f, false, AudioBusHandle{static_cast<uint32_t>(BuiltinBus::Master)}};

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

void update_audio_impl(AudioEngine::Impl* impl, float delta_time) {
    if (!impl || !impl->initialized) return;

    // Process active crossfade
    if (impl->active_crossfade) {
        auto& cf = *impl->active_crossfade;
        cf.elapsed += delta_time;

        float t = std::min(cf.elapsed / cf.duration, 1.0f);

        // Update volumes
        auto from_it = impl->music.find(cf.from.id);
        auto to_it = impl->music.find(cf.to.id);

        if (from_it != impl->music.end() && from_it->second.loaded) {
            float vol = cf.from_start_volume * (1.0f - t);
            ma_sound_set_volume(&from_it->second.sound, vol * impl->music_volume * impl->master_volume);
        }

        if (to_it != impl->music.end() && to_it->second.loaded) {
            float vol = t;  // Fade in to full volume
            ma_sound_set_volume(&to_it->second.sound, vol * impl->music_volume * impl->master_volume);
        }

        // Complete crossfade
        if (t >= 1.0f) {
            if (from_it != impl->music.end() && from_it->second.loaded) {
                ma_sound_stop(&from_it->second.sound);
            }
            impl->active_crossfade.reset();
        }
    }
}

SoundHandle load_sound_impl(AudioEngine::Impl* impl, const std::string& path) {
    if (!impl || !impl->initialized) return SoundHandle{};

    uint32_t id = impl->allocate_sound_id();
    if (id == UINT32_MAX) {
        log(LogLevel::Error, "Failed to allocate sound ID: all IDs in use");
        return SoundHandle{};
    }
    SoundHandle handle{id};

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

    float final_volume = AudioEngine::Impl::clamp_volume(config.volume) * impl->sound_volume * impl->master_volume;
    ma_sound_set_volume(&it->second.sound, final_volume);
    ma_sound_set_pitch(&it->second.sound, config.pitch);
    ma_sound_set_pan(&it->second.sound, config.pan);
    ma_sound_set_looping(&it->second.sound, config.loop);
    ma_sound_start(&it->second.sound);
}

void play_sound_3d_impl(AudioEngine::Impl* impl, SoundHandle h, const Vec3& pos, const SoundConfig& config) {
    if (!impl || !impl->initialized) return;
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    float final_volume = AudioEngine::Impl::clamp_volume(config.volume) * impl->sound_volume * impl->master_volume;
    ma_sound_set_volume(&it->second.sound, final_volume);
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

void set_sound_position_impl(AudioEngine::Impl* impl, SoundHandle h, const Vec3& pos) {
    if (!impl) return;
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    ma_sound_set_position(&it->second.sound, pos.x, pos.y, pos.z);
}

void set_sound_velocity_impl(AudioEngine::Impl* impl, SoundHandle h, const Vec3& vel) {
    if (!impl) return;
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    ma_sound_set_velocity(&it->second.sound, vel.x, vel.y, vel.z);
}

bool is_sound_playing_impl(AudioEngine::Impl* impl, SoundHandle h) {
    if (!impl) return false;
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return false;

    return ma_sound_is_playing(&it->second.sound);
}

float get_sound_length_impl(AudioEngine::Impl* impl, SoundHandle h) {
    if (!impl || !impl->initialized) return 0.0f;
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return 0.0f;

    ma_uint64 length_frames = 0;
    ma_sound_get_length_in_pcm_frames(&it->second.sound, &length_frames);
    ma_uint32 sample_rate = ma_engine_get_sample_rate(&impl->engine);
    return static_cast<float>(length_frames) / static_cast<float>(sample_rate);
}

MusicHandle load_music_impl(AudioEngine::Impl* impl, const std::string& path) {
    if (!impl || !impl->initialized) return MusicHandle{};

    uint32_t id = impl->allocate_music_id();
    if (id == UINT32_MAX) {
        log(LogLevel::Error, "Failed to allocate music ID: all IDs in use");
        return MusicHandle{};
    }
    MusicHandle handle{id};

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

    // Save cursor position before stopping so we can resume from same spot
    ma_sound_get_cursor_in_pcm_frames(&it->second.sound, &it->second.paused_cursor);
    it->second.was_playing = ma_sound_is_playing(&it->second.sound);
    ma_sound_stop(&it->second.sound);
}

void resume_music_impl(AudioEngine::Impl* impl, MusicHandle h) {
    if (!impl) return;
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    // Seek to saved position and resume
    ma_sound_seek_to_pcm_frame(&it->second.sound, it->second.paused_cursor);
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

    float final_volume = AudioEngine::Impl::clamp_volume(volume) * impl->music_volume * impl->master_volume;
    ma_sound_set_volume(&it->second.sound, final_volume);
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

    // Track which sounds were playing before pausing
    for (auto& [id, sound] : impl->sounds) {
        if (sound.loaded) {
            sound.was_playing = ma_sound_is_playing(&sound.sound);
            if (sound.was_playing) {
                ma_sound_get_cursor_in_pcm_frames(&sound.sound, &sound.paused_cursor);
                ma_sound_stop(&sound.sound);
            }
        }
    }
    for (auto& [id, music] : impl->music) {
        if (music.loaded) {
            music.was_playing = ma_sound_is_playing(&music.sound);
            if (music.was_playing) {
                ma_sound_get_cursor_in_pcm_frames(&music.sound, &music.paused_cursor);
                ma_sound_stop(&music.sound);
            }
        }
    }
}

void resume_all_impl(AudioEngine::Impl* impl) {
    if (!impl || !impl->initialized) return;

    // Resume sounds that were playing before pause_all
    for (auto& [id, sound] : impl->sounds) {
        if (sound.loaded && sound.was_playing) {
            ma_sound_seek_to_pcm_frame(&sound.sound, sound.paused_cursor);
            ma_sound_start(&sound.sound);
            sound.was_playing = false;  // Reset flag
        }
    }
    for (auto& [id, music] : impl->music) {
        if (music.loaded && music.was_playing) {
            ma_sound_seek_to_pcm_frame(&music.sound, music.paused_cursor);
            ma_sound_start(&music.sound);
            music.was_playing = false;  // Reset flag
        }
    }
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

// Phase 2: Music position functions
float get_music_position_impl(AudioEngine::Impl* impl, MusicHandle h) {
    if (!impl || !impl->initialized) return 0.0f;
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return 0.0f;

    float cursor_seconds = 0.0f;
    ma_sound_get_cursor_in_seconds(&it->second.sound, &cursor_seconds);
    return cursor_seconds;
}

void set_music_position_impl(AudioEngine::Impl* impl, MusicHandle h, float seconds) {
    if (!impl || !impl->initialized) return;
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    ma_uint32 sample_rate = ma_engine_get_sample_rate(&impl->engine);
    ma_uint64 frame = static_cast<ma_uint64>(seconds * sample_rate);
    ma_sound_seek_to_pcm_frame(&it->second.sound, frame);
}

// Phase 2: Crossfade functions
void crossfade_music_impl(AudioEngine::Impl* impl, MusicHandle from, MusicHandle to, float duration) {
    if (!impl || !impl->initialized) return;

    // Validate both handles
    auto from_it = impl->music.find(from.id);
    auto to_it = impl->music.find(to.id);
    if (from_it == impl->music.end() || !from_it->second.loaded) return;
    if (to_it == impl->music.end() || !to_it->second.loaded) return;

    // Start the crossfade
    impl->active_crossfade = AudioEngine::Impl::CrossfadeState{};
    impl->active_crossfade->from = from;
    impl->active_crossfade->to = to;
    impl->active_crossfade->duration = std::max(duration, 0.01f);
    impl->active_crossfade->elapsed = 0.0f;

    // Get current volumes
    float from_vol = 0.0f;
    ma_sound_get_volume(&from_it->second.sound, &from_vol);
    impl->active_crossfade->from_start_volume = from_vol;
    impl->active_crossfade->to_start_volume = 0.0f;

    // Start the 'to' track at zero volume
    ma_sound_set_volume(&to_it->second.sound, 0.0f);
    ma_sound_start(&to_it->second.sound);
}

// Phase 2: Global volume functions
void set_sound_volume_impl(AudioEngine::Impl* impl, float volume) {
    if (!impl || !impl->initialized) return;
    impl->sound_volume = AudioEngine::Impl::clamp_volume(volume);

    // Update all currently loaded sounds
    for (auto& [id, sound] : impl->sounds) {
        if (sound.loaded) {
            float current_vol = 0.0f;
            ma_sound_get_volume(&sound.sound, &current_vol);
            // This is approximate - ideally we'd store the base volume per-sound
        }
    }
}

void set_music_volume_global_impl(AudioEngine::Impl* impl, float volume) {
    if (!impl || !impl->initialized) return;
    impl->music_volume = AudioEngine::Impl::clamp_volume(volume);

    // Update all currently loaded music
    for (auto& [id, music] : impl->music) {
        if (music.loaded) {
            // Get current volume and apply new global multiplier
            // Note: This may need refinement if per-track volumes vary
        }
    }
}

// Phase 2: Listener velocity for Doppler
void set_listener_velocity_impl(AudioEngine::Impl* impl, const Vec3& vel) {
    if (!impl || !impl->initialized) return;
    impl->listener_velocity = vel;
    ma_engine_listener_set_velocity(&impl->engine, 0, vel.x, vel.y, vel.z);
}

// Phase 3: Audio bus functions
AudioBusHandle get_bus_impl(AudioEngine::Impl* impl, BuiltinBus bus) {
    if (!impl) return AudioBusHandle{};
    return AudioBusHandle{static_cast<uint32_t>(bus)};
}

AudioBusHandle create_bus_impl(AudioEngine::Impl* impl, const std::string& name, AudioBusHandle parent) {
    if (!impl || !impl->initialized) return AudioBusHandle{};

    uint32_t id = impl->next_bus_id++;
    impl->buses[id] = AudioEngine::Impl::AudioBus{name, 1.0f, false, parent};
    return AudioBusHandle{id};
}

void destroy_bus_impl(AudioEngine::Impl* impl, AudioBusHandle bus) {
    if (!impl) return;
    // Don't allow destroying builtin buses
    if (bus.id < 100) return;
    impl->buses.erase(bus.id);
}

// Compute effective volume of a bus (considering parent hierarchy)
float compute_bus_volume(AudioEngine::Impl* impl, AudioBusHandle bus) {
    if (!impl || !bus.valid()) return 1.0f;

    auto it = impl->buses.find(bus.id);
    if (it == impl->buses.end()) return 1.0f;

    float vol = it->second.muted ? 0.0f : it->second.volume;

    // Apply parent volume
    if (it->second.parent.valid()) {
        vol *= compute_bus_volume(impl, it->second.parent);
    }

    return vol;
}

void set_bus_volume_impl(AudioEngine::Impl* impl, AudioBusHandle bus, float volume) {
    if (!impl) return;
    auto it = impl->buses.find(bus.id);
    if (it == impl->buses.end()) return;
    it->second.volume = AudioEngine::Impl::clamp_volume(volume);
}

float get_bus_volume_impl(AudioEngine::Impl* impl, AudioBusHandle bus) {
    if (!impl) return 1.0f;
    auto it = impl->buses.find(bus.id);
    if (it == impl->buses.end()) return 1.0f;
    return it->second.volume;
}

void set_bus_muted_impl(AudioEngine::Impl* impl, AudioBusHandle bus, bool muted) {
    if (!impl) return;
    auto it = impl->buses.find(bus.id);
    if (it == impl->buses.end()) return;
    it->second.muted = muted;
}

bool is_bus_muted_impl(AudioEngine::Impl* impl, AudioBusHandle bus) {
    if (!impl) return false;
    auto it = impl->buses.find(bus.id);
    if (it == impl->buses.end()) return false;
    return it->second.muted;
}

} // namespace engine::audio
