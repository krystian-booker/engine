// miniaudio implementation
// This file contains all miniaudio-specific code
#include <miniaudio.h>
#include <ma_reverb_node.h>

#include <engine/audio/audio_engine.hpp>
#include <engine/core/log.hpp>
#include <unordered_map>
#include <vector>
#include <optional>
#include <algorithm>
#include <chrono>
#include <mutex>

namespace engine::audio {

using namespace engine::core;

struct AudioEngine::Impl {
    ma_engine engine{};
    ma_reverb_node reverb_node{};
    bool initialized = false;
    bool reverb_initialized = false;
    
    // Global lock for thread safety
    std::recursive_mutex m_mutex;

    float master_volume = 1.0f;
    float sound_volume = 1.0f;   // Global volume for all sounds
    float music_volume = 1.0f;   // Global volume for all music

    struct LoadedSound {
        ma_sound sound;
        std::string path;
        bool loaded = false;
        bool was_playing = false;     // For pause_all/resume_all tracking
        ma_uint64 paused_cursor = 0;  // For proper pause/resume position
        
        // Fading state
        bool fading = false;
        float fade_target_vol = 1.0f;
        float fade_start_vol = 0.0f;
        float fade_duration = 0.0f;
        float fade_elapsed = 0.0f;
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
        ma_sound_group group; // Wraps an ma_node
        bool initialized = false;
        float volume = 1.0f;
        bool muted = false;
        AudioBusHandle parent;

        // Filter nodes for the bus
        ma_lpf_node lpf_node{};
        ma_hpf_node hpf_node{};
        bool lpf_initialized = false;
        bool hpf_initialized = false;

        // Filter state
        float lpf_cutoff = 20000.0f;  // Default: effectively disabled
        float hpf_cutoff = 20.0f;     // Default: effectively disabled
        bool lpf_enabled = false;
        bool hpf_enabled = false;
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

    // Voice management for priority-based stealing
    uint32_t max_voices = 32;
    struct ActiveVoice {
        SoundHandle handle;
        float priority = 1.0f;
        bool spatial = false;
    };
    std::vector<ActiveVoice> active_voices;

    // Error handling
    AudioErrorCallback error_callback;
    AudioResult last_error;

    void report_error(AudioError error, const std::string& message) {
        last_error.error = error;
        last_error.message = message;
        if (error_callback) {
            error_callback(error, message);
        }
        log(LogLevel::Error, message.c_str());
    }

    void clear_error() {
        last_error.error = AudioError::None;
        last_error.message.clear();
    }

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

    // Voice management helpers
    void add_active_voice(SoundHandle h, float priority, bool spatial) {
        active_voices.push_back({h, priority, spatial});
    }

    void remove_active_voice(SoundHandle h) {
        active_voices.erase(
            std::remove_if(active_voices.begin(), active_voices.end(),
                [h](const ActiveVoice& v) { return v.handle.id == h.id; }),
            active_voices.end());
    }

    // Returns true if there's room for the voice (possibly after stealing)
    bool try_acquire_voice_slot(float priority, SoundHandle& stolen_handle) {
        stolen_handle = SoundHandle{};  // Invalid by default

        if (active_voices.size() < max_voices) {
            return true;  // Room available
        }

        // Find lowest priority voice
        auto lowest = std::min_element(active_voices.begin(), active_voices.end(),
            [](const ActiveVoice& a, const ActiveVoice& b) {
                return a.priority < b.priority;
            });

        if (lowest != active_voices.end() && priority > lowest->priority) {
            stolen_handle = lowest->handle;
            return true;  // Can steal this voice
        }

        return false;  // No room and can't steal
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
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

    ma_engine_config config = ma_engine_config_init();
    config.channels = settings.channels;
    config.sampleRate = settings.sample_rate;

    if (ma_engine_init(&config, &impl->engine) != MA_SUCCESS) {
        log(LogLevel::Error, "Failed to initialize miniaudio engine");
        return;
    }

    // Init global reverb node (attached to endpoint)
    // We put it before the endpoint.
    ma_reverb_node_config reverbConfig = ma_reverb_node_config_init(config.channels, config.sampleRate);
    if (ma_reverb_node_init(ma_engine_get_node_graph(&impl->engine), &reverbConfig, NULL, &impl->reverb_node) != MA_SUCCESS) {
         log(LogLevel::Error, "Failed to initialize reverb node");
    } else {
        // By default, attach reverb node output to endpoint
        ma_node_attach_output_bus(&impl->reverb_node, 0, ma_engine_get_endpoint(&impl->engine), 0);
        impl->reverb_initialized = true;
    }

    impl->master_volume = settings.master_volume;
    ma_engine_set_volume(&impl->engine, settings.master_volume);

    // Helper to create bus with filter nodes
    auto create_builtin_bus = [&](BuiltinBus id, const char* name, AudioBusHandle parent) {
        AudioEngine::Impl::AudioBus bus;
        bus.name = name;
        bus.parent = parent;
        bus.volume = 1.0f;
        bus.muted = false;

        // Init sound group
        if (ma_sound_group_init(&impl->engine, 0, NULL, &bus.group) != MA_SUCCESS) {
             log(LogLevel::Error, "Failed to init bus group");
             return;
        }
        bus.initialized = true;

        ma_uint32 channels = config.channels;
        ma_uint32 sampleRate = config.sampleRate;

        // Initialize lowpass filter node (default cutoff at 20kHz - effectively disabled)
        ma_lpf_node_config lpfConfig = ma_lpf_node_config_init(channels, sampleRate, 20000.0, 2);
        if (ma_lpf_node_init(ma_engine_get_node_graph(&impl->engine), &lpfConfig, NULL, &bus.lpf_node) == MA_SUCCESS) {
            bus.lpf_initialized = true;
        }

        // Initialize highpass filter node (default cutoff at 20Hz - effectively disabled)
        ma_hpf_node_config hpfConfig = ma_hpf_node_config_init(channels, sampleRate, 20.0, 2);
        if (ma_hpf_node_init(ma_engine_get_node_graph(&impl->engine), &hpfConfig, NULL, &bus.hpf_node) == MA_SUCCESS) {
            bus.hpf_initialized = true;
        }

        // Signal chain: group -> lpf -> hpf -> output
        // Connect group output to LPF input
        if (bus.lpf_initialized) {
            ma_node_attach_output_bus(&bus.group, 0, &bus.lpf_node, 0);
        }

        // Connect LPF output to HPF input (or group to HPF if LPF failed)
        ma_node* filter_output = bus.lpf_initialized ? (ma_node*)&bus.lpf_node : (ma_node*)&bus.group;
        if (bus.hpf_initialized) {
            ma_node_attach_output_bus(filter_output, 0, &bus.hpf_node, 0);
            filter_output = (ma_node*)&bus.hpf_node;
        }

        // Connect final filter output to parent or reverb
        if (parent.valid()) {
            auto it = impl->buses.find(parent.id);
            if (it != impl->buses.end() && it->second.initialized) {
                // Connect to parent's group input (before its filters)
                ma_node_attach_output_bus(filter_output, 0, &it->second.group, 0);
            }
        } else {
            // Root bus -> attach to Reverb Node
            ma_node_attach_output_bus(filter_output, 0, &impl->reverb_node, 0);
        }

        impl->buses[static_cast<uint32_t>(id)] = std::move(bus);
    };

    create_builtin_bus(BuiltinBus::Master, "Master", {});
    create_builtin_bus(BuiltinBus::Music, "Music", AudioBusHandle{static_cast<uint32_t>(BuiltinBus::Master)});
    create_builtin_bus(BuiltinBus::SFX, "SFX", AudioBusHandle{static_cast<uint32_t>(BuiltinBus::Master)});
    create_builtin_bus(BuiltinBus::Voice, "Voice", AudioBusHandle{static_cast<uint32_t>(BuiltinBus::Master)});
    create_builtin_bus(BuiltinBus::Ambient, "Ambient", AudioBusHandle{static_cast<uint32_t>(BuiltinBus::Master)});
    create_builtin_bus(BuiltinBus::UI, "UI", AudioBusHandle{static_cast<uint32_t>(BuiltinBus::Master)});

    impl->initialized = true;
}

void shutdown_audio_impl(AudioEngine::Impl* impl) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

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
    
    // Uninit buses (including filter nodes)
    for (auto& [id, bus] : impl->buses) {
        if (bus.lpf_initialized) {
            ma_lpf_node_uninit(&bus.lpf_node, NULL);
        }
        if (bus.hpf_initialized) {
            ma_hpf_node_uninit(&bus.hpf_node, NULL);
        }
        if (bus.initialized) {
            ma_sound_group_uninit(&bus.group);
        }
    }
    impl->buses.clear();

    if (impl->reverb_initialized) {
        log(LogLevel::Info, "Uninit reverb node...");
        ma_reverb_node_uninit(&impl->reverb_node, NULL);
        log(LogLevel::Info, "Uninit reverb node done.");
    }
    log(LogLevel::Info, "Uninit engine...");
    ma_engine_uninit(&impl->engine);
    log(LogLevel::Info, "Uninit engine done.");
    impl->initialized = false;
}

void update_audio_impl(AudioEngine::Impl* impl, float delta_time) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

    // Process active crossfade (global volume is handled by Music bus, so use local volume only)
    if (impl->active_crossfade) {
        auto& cf = *impl->active_crossfade;
        cf.elapsed += delta_time;

        float t = std::min(cf.elapsed / cf.duration, 1.0f);

        // Update volumes (local per-track volume only; bus handles global)
        auto from_it = impl->music.find(cf.from.id);
        auto to_it = impl->music.find(cf.to.id);

        if (from_it != impl->music.end() && from_it->second.loaded) {
            float vol = cf.from_start_volume * (1.0f - t);
            ma_sound_set_volume(&from_it->second.sound, vol);
        }

        if (to_it != impl->music.end() && to_it->second.loaded) {
            float vol = t;  // Fade in to full volume
            ma_sound_set_volume(&to_it->second.sound, vol);
        }

        // Complete crossfade
        if (t >= 1.0f) {
            if (from_it != impl->music.end() && from_it->second.loaded) {
                ma_sound_stop(&from_it->second.sound);
            }
            impl->active_crossfade.reset();
        }
    }
    
    // Process fades (global volume is handled by bus, so use local volume only)
    for (auto& [id, sound] : impl->sounds) {
        if (sound.loaded && sound.fading) {
            sound.fade_elapsed += delta_time;
            float t = std::min(sound.fade_duration > 0 ? sound.fade_elapsed / sound.fade_duration : 1.0f, 1.0f);

            float current = sound.fade_start_vol + (sound.fade_target_vol - sound.fade_start_vol) * t;
            ma_sound_set_volume(&sound.sound, current);

            if (t >= 1.0f) {
                sound.fading = false;
                if (sound.fade_target_vol <= 0.001f) {
                    ma_sound_stop(&sound.sound);
                }
            }
        }
    }

    // Clean up finished voices from tracking
    impl->active_voices.erase(
        std::remove_if(impl->active_voices.begin(), impl->active_voices.end(),
            [impl](const AudioEngine::Impl::ActiveVoice& v) {
                auto it = impl->sounds.find(v.handle.id);
                if (it == impl->sounds.end() || !it->second.loaded) {
                    return true;  // Sound no longer exists
                }
                return !ma_sound_is_playing(&it->second.sound);  // Sound finished
            }),
        impl->active_voices.end());
}

SoundHandle load_sound_impl(AudioEngine::Impl* impl, const std::string& path) {
    if (!impl || !impl->initialized) {
        if (impl) impl->report_error(AudioError::DeviceError, "Audio engine not initialized");
        return SoundHandle{};
    }
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    impl->clear_error();

    uint32_t id = impl->allocate_sound_id();
    if (id == UINT32_MAX) {
        impl->report_error(AudioError::OutOfMemory, "Failed to allocate sound ID: all IDs in use");
        return SoundHandle{};
    }
    SoundHandle handle{id};

    auto& sound = impl->sounds[handle.id];
    sound.path = path;

    // Load sound but don't attach to a group yet, or attach to engine master by default
    // We can use flags to decode immediately
    ma_result result = ma_sound_init_from_file(&impl->engine, path.c_str(),
                                 MA_SOUND_FLAG_DECODE, NULL, NULL,
                                 &sound.sound);
    if (result != MA_SUCCESS) {
        AudioError error = (result == MA_DOES_NOT_EXIST) ? AudioError::FileNotFound : AudioError::DecodingFailed;
        impl->report_error(error, "Failed to load sound: " + path);
        impl->sounds.erase(handle.id);
        return SoundHandle{};
    }

    sound.loaded = true;
    return handle;
}

void unload_sound_impl(AudioEngine::Impl* impl, SoundHandle h) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end()) return;

    if (it->second.loaded) {
        ma_sound_uninit(&it->second.sound);
    }
    impl->sounds.erase(it);
}

void play_sound_impl(AudioEngine::Impl* impl, SoundHandle h, const SoundConfig& config) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    // Voice stealing: check if we have room or can steal a lower-priority voice
    SoundHandle stolen;
    if (!impl->try_acquire_voice_slot(config.priority, stolen)) {
        // No room and can't steal - reject playback
        impl->report_error(AudioError::PlaybackFailed, "Max voices reached, cannot play sound (priority too low)");
        return;
    }

    // If we need to steal, stop the stolen voice first
    if (stolen.valid()) {
        auto stolen_it = impl->sounds.find(stolen.id);
        if (stolen_it != impl->sounds.end() && stolen_it->second.loaded) {
            ma_sound_stop(&stolen_it->second.sound);
        }
        impl->remove_active_voice(stolen);
    }

    // Set per-sound properties. Global volume is handled by bus hierarchy.
    float local_volume = AudioEngine::Impl::clamp_volume(config.volume);
    ma_sound_set_volume(&it->second.sound, local_volume);
    ma_sound_set_pitch(&it->second.sound, config.pitch);
    ma_sound_set_pan(&it->second.sound, config.pan);
    ma_sound_set_looping(&it->second.sound, config.loop);

    // Attach to correct bus (bus volume handles global sound_volume)
    AudioBusHandle busHandle = config.bus.valid() ? config.bus : AudioBusHandle{static_cast<uint32_t>(BuiltinBus::SFX)};
    auto busIt = impl->buses.find(busHandle.id);
    if (busIt != impl->buses.end() && busIt->second.initialized) {
        ma_node_attach_output_bus(&it->second.sound, 0, &busIt->second.group, 0);
    } else {
        // Fallback to SFX
        busIt = impl->buses.find((uint32_t)BuiltinBus::SFX);
        if (busIt != impl->buses.end()) {
            ma_node_attach_output_bus(&it->second.sound, 0, &busIt->second.group, 0);
        }
    }

    ma_sound_start(&it->second.sound);
    impl->add_active_voice(h, config.priority, false);
}

void play_sound_3d_impl(AudioEngine::Impl* impl, SoundHandle h, const Vec3& pos, const SoundConfig& config) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    // Voice stealing: check if we have room or can steal a lower-priority voice
    SoundHandle stolen;
    if (!impl->try_acquire_voice_slot(config.priority, stolen)) {
        // No room and can't steal - reject playback
        impl->report_error(AudioError::PlaybackFailed, "Max voices reached, cannot play 3D sound (priority too low)");
        return;
    }

    // If we need to steal, stop the stolen voice first
    if (stolen.valid()) {
        auto stolen_it = impl->sounds.find(stolen.id);
        if (stolen_it != impl->sounds.end() && stolen_it->second.loaded) {
            ma_sound_stop(&stolen_it->second.sound);
        }
        impl->remove_active_voice(stolen);
    }

    // Set per-sound properties. Global volume is handled by bus hierarchy.
    float local_volume = AudioEngine::Impl::clamp_volume(config.volume);
    ma_sound_set_volume(&it->second.sound, local_volume);
    ma_sound_set_pitch(&it->second.sound, config.pitch);
    ma_sound_set_looping(&it->second.sound, config.loop);
    ma_sound_set_position(&it->second.sound, pos.x, pos.y, pos.z);
    ma_sound_set_spatialization_enabled(&it->second.sound, MA_TRUE);

    // Attach to bus (bus volume handles global sound_volume)
    AudioBusHandle busHandle = config.bus.valid() ? config.bus : AudioBusHandle{static_cast<uint32_t>(BuiltinBus::SFX)};
    auto busIt = impl->buses.find(busHandle.id);
    if (busIt != impl->buses.end() && busIt->second.initialized) {
        ma_node_attach_output_bus(&it->second.sound, 0, &busIt->second.group, 0);
    }

    ma_sound_start(&it->second.sound);
    impl->add_active_voice(h, config.priority, true);
}

void stop_sound_impl(AudioEngine::Impl* impl, SoundHandle h) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    ma_sound_stop(&it->second.sound);
    impl->remove_active_voice(h);
}

void set_sound_position_impl(AudioEngine::Impl* impl, SoundHandle h, const Vec3& pos) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    ma_sound_set_position(&it->second.sound, pos.x, pos.y, pos.z);
}

void set_sound_velocity_impl(AudioEngine::Impl* impl, SoundHandle h, const Vec3& vel) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    ma_sound_set_velocity(&it->second.sound, vel.x, vel.y, vel.z);
}

bool is_sound_playing_impl(AudioEngine::Impl* impl, SoundHandle h) {
    if (!impl) return false;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return false;

    return ma_sound_is_playing(&it->second.sound);
}

float get_sound_length_impl(AudioEngine::Impl* impl, SoundHandle h) {
    if (!impl || !impl->initialized) return 0.0f;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return 0.0f;

    ma_uint64 length_frames = 0;
    ma_sound_get_length_in_pcm_frames(&it->second.sound, &length_frames);
    ma_uint32 sample_rate = ma_engine_get_sample_rate(&impl->engine);
    return static_cast<float>(length_frames) / static_cast<float>(sample_rate);
}

MusicHandle load_music_impl(AudioEngine::Impl* impl, const std::string& path) {
    if (!impl || !impl->initialized) {
        if (impl) impl->report_error(AudioError::DeviceError, "Audio engine not initialized");
        return MusicHandle{};
    }
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    impl->clear_error();

    uint32_t id = impl->allocate_music_id();
    if (id == UINT32_MAX) {
        impl->report_error(AudioError::OutOfMemory, "Failed to allocate music ID: all IDs in use");
        return MusicHandle{};
    }
    MusicHandle handle{id};

    auto& music = impl->music[handle.id];
    music.path = path;

    ma_result result = ma_sound_init_from_file(&impl->engine, path.c_str(),
                                 MA_SOUND_FLAG_STREAM, NULL, NULL,
                                 &music.sound);
    if (result != MA_SUCCESS) {
        AudioError error = (result == MA_DOES_NOT_EXIST) ? AudioError::FileNotFound : AudioError::DecodingFailed;
        impl->report_error(error, "Failed to load music: " + path);
        impl->music.erase(handle.id);
        return MusicHandle{};
    }

    music.loaded = true;
    
    // Attach default to Music bus
    auto busIt = impl->buses.find((uint32_t)BuiltinBus::Music);
    if (busIt != impl->buses.end()) {
        ma_node_attach_output_bus(&music.sound, 0, &busIt->second.group, 0);
    }
    
    return handle;
}

void unload_music_impl(AudioEngine::Impl* impl, MusicHandle h) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->music.find(h.id);
    if (it == impl->music.end()) return;

    if (it->second.loaded) {
        ma_sound_uninit(&it->second.sound);
    }
    impl->music.erase(it);
}

void play_music_impl(AudioEngine::Impl* impl, MusicHandle h, bool loop) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    ma_sound_set_looping(&it->second.sound, loop);
    ma_sound_start(&it->second.sound);
}

void pause_music_impl(AudioEngine::Impl* impl, MusicHandle h) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    ma_sound_get_cursor_in_pcm_frames(&it->second.sound, &it->second.paused_cursor);
    it->second.was_playing = ma_sound_is_playing(&it->second.sound);
    ma_sound_stop(&it->second.sound);
}

void resume_music_impl(AudioEngine::Impl* impl, MusicHandle h) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    ma_sound_seek_to_pcm_frame(&it->second.sound, it->second.paused_cursor);
    ma_sound_start(&it->second.sound);
}

void stop_music_impl(AudioEngine::Impl* impl, MusicHandle h) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    ma_sound_stop(&it->second.sound);
    ma_sound_seek_to_pcm_frame(&it->second.sound, 0);
}

void set_music_volume_impl(AudioEngine::Impl* impl, MusicHandle h, float volume) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    // Set per-track volume. Global music_volume is handled by Music bus.
    float local_volume = AudioEngine::Impl::clamp_volume(volume);
    ma_sound_set_volume(&it->second.sound, local_volume);
}

void set_master_volume_impl(AudioEngine::Impl* impl, float volume) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    impl->master_volume = volume;
    ma_engine_set_volume(&impl->engine, volume);
}

float get_master_volume_impl(AudioEngine::Impl* impl) {
    if (!impl) return 1.0f;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    return impl->master_volume;
}

void set_listener_position_impl(AudioEngine::Impl* impl, const Vec3& pos) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    impl->listener_position = pos;
    ma_engine_listener_set_position(&impl->engine, 0, pos.x, pos.y, pos.z);
}

void set_listener_orientation_impl(AudioEngine::Impl* impl, const Vec3& forward, const Vec3& up) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    impl->listener_forward = forward;
    impl->listener_up = up;
    ma_engine_listener_set_direction(&impl->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&impl->engine, 0, up.x, up.y, up.z);
}

void pause_all_impl(AudioEngine::Impl* impl) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

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
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

    // Resume sounds that were playing before pause_all
    for (auto& [id, sound] : impl->sounds) {
        if (sound.loaded && sound.was_playing) {
            ma_sound_seek_to_pcm_frame(&sound.sound, sound.paused_cursor);
            ma_sound_start(&sound.sound);
            sound.was_playing = false;
        }
    }
    for (auto& [id, music] : impl->music) {
        if (music.loaded && music.was_playing) {
            ma_sound_seek_to_pcm_frame(&music.sound, music.paused_cursor);
            ma_sound_start(&music.sound);
            music.was_playing = false;
        }
    }
}

void stop_all_impl(AudioEngine::Impl* impl) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

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
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

    uint32_t count = 0;
    for (auto& [id, sound] : impl->sounds) {
        if (sound.loaded && ma_sound_is_playing(&sound.sound)) {
            count++;
        }
    }
    return count;
}

float get_music_position_impl(AudioEngine::Impl* impl, MusicHandle h) {
    if (!impl || !impl->initialized) return 0.0f;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return 0.0f;

    float cursor_seconds = 0.0f;
    ma_sound_get_cursor_in_seconds(&it->second.sound, &cursor_seconds);
    return cursor_seconds;
}

void set_music_position_impl(AudioEngine::Impl* impl, MusicHandle h, float seconds) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->music.find(h.id);
    if (it == impl->music.end() || !it->second.loaded) return;

    ma_uint32 sample_rate = ma_engine_get_sample_rate(&impl->engine);
    ma_uint64 frame = static_cast<ma_uint64>(seconds * sample_rate);
    ma_sound_seek_to_pcm_frame(&it->second.sound, frame);
}

void crossfade_music_impl(AudioEngine::Impl* impl, MusicHandle from, MusicHandle to, float duration) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

    auto from_it = impl->music.find(from.id);
    auto to_it = impl->music.find(to.id);
    if (from_it == impl->music.end() || !from_it->second.loaded) return;
    if (to_it == impl->music.end() || !to_it->second.loaded) return;

    // If there's an active crossfade, complete it immediately before starting new one
    if (impl->active_crossfade.has_value()) {
        auto& cf = *impl->active_crossfade;
        auto prev_from_it = impl->music.find(cf.from.id);
        auto prev_to_it = impl->music.find(cf.to.id);

        // Complete the previous crossfade: stop "from" track, set "to" to full local volume
        if (prev_from_it != impl->music.end() && prev_from_it->second.loaded) {
            ma_sound_stop(&prev_from_it->second.sound);
            ma_sound_set_volume(&prev_from_it->second.sound, 0.0f);
        }
        if (prev_to_it != impl->music.end() && prev_to_it->second.loaded) {
            ma_sound_set_volume(&prev_to_it->second.sound, 1.0f);  // Full local volume; bus handles global
        }
        impl->active_crossfade.reset();
    }

    impl->active_crossfade = AudioEngine::Impl::CrossfadeState{};
    impl->active_crossfade->from = from;
    impl->active_crossfade->to = to;
    impl->active_crossfade->duration = std::max(duration, 0.01f);
    impl->active_crossfade->elapsed = 0.0f;

    float from_vol = ma_sound_get_volume(&from_it->second.sound);
    impl->active_crossfade->from_start_volume = from_vol;
    impl->active_crossfade->to_start_volume = 0.0f;

    ma_sound_set_volume(&to_it->second.sound, 0.0f);
    ma_sound_start(&to_it->second.sound);
}

void set_sound_volume_impl(AudioEngine::Impl* impl, float volume) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    impl->sound_volume = AudioEngine::Impl::clamp_volume(volume);

    // Use bus-based volume control: set the SFX bus volume
    auto sfx_bus_it = impl->buses.find(static_cast<uint32_t>(BuiltinBus::SFX));
    if (sfx_bus_it != impl->buses.end() && sfx_bus_it->second.initialized) {
        sfx_bus_it->second.volume = impl->sound_volume;
        if (!sfx_bus_it->second.muted) {
            ma_sound_group_set_volume(&sfx_bus_it->second.group, impl->sound_volume);
        }
    }
}

void set_music_volume_global_impl(AudioEngine::Impl* impl, float volume) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    impl->music_volume = AudioEngine::Impl::clamp_volume(volume);

    // Use bus-based volume control: set the Music bus volume
    auto music_bus_it = impl->buses.find(static_cast<uint32_t>(BuiltinBus::Music));
    if (music_bus_it != impl->buses.end() && music_bus_it->second.initialized) {
        music_bus_it->second.volume = impl->music_volume;
        if (!music_bus_it->second.muted) {
            ma_sound_group_set_volume(&music_bus_it->second.group, impl->music_volume);
        }
    }
}

void set_listener_velocity_impl(AudioEngine::Impl* impl, const Vec3& vel) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    impl->listener_velocity = vel;
    ma_engine_listener_set_velocity(&impl->engine, 0, vel.x, vel.y, vel.z);
}

AudioBusHandle get_bus_impl(AudioEngine::Impl* impl, BuiltinBus bus) {
    if (!impl) return AudioBusHandle{};
    return AudioBusHandle{static_cast<uint32_t>(bus)};
}

AudioBusHandle create_bus_impl(AudioEngine::Impl* impl, const std::string& name, AudioBusHandle parent) {
    if (!impl || !impl->initialized) return AudioBusHandle{};
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

    uint32_t id = impl->next_bus_id++;
    AudioEngine::Impl::AudioBus bus;
    bus.name = name;
    bus.volume = 1.0f;
    bus.muted = false;
    bus.parent = parent;

    if (ma_sound_group_init(&impl->engine, 0, NULL, &bus.group) != MA_SUCCESS) {
        return AudioBusHandle{};
    }
    bus.initialized = true;

    ma_uint32 channels = ma_engine_get_channels(&impl->engine);
    ma_uint32 sampleRate = ma_engine_get_sample_rate(&impl->engine);

    // Initialize lowpass filter node (default cutoff at 20kHz - effectively disabled)
    ma_lpf_node_config lpfConfig = ma_lpf_node_config_init(channels, sampleRate, 20000.0, 2);
    if (ma_lpf_node_init(ma_engine_get_node_graph(&impl->engine), &lpfConfig, NULL, &bus.lpf_node) == MA_SUCCESS) {
        bus.lpf_initialized = true;
    }

    // Initialize highpass filter node (default cutoff at 20Hz - effectively disabled)
    ma_hpf_node_config hpfConfig = ma_hpf_node_config_init(channels, sampleRate, 20.0, 2);
    if (ma_hpf_node_init(ma_engine_get_node_graph(&impl->engine), &hpfConfig, NULL, &bus.hpf_node) == MA_SUCCESS) {
        bus.hpf_initialized = true;
    }

    // Signal chain: group -> lpf -> hpf -> output
    if (bus.lpf_initialized) {
        ma_node_attach_output_bus(&bus.group, 0, &bus.lpf_node, 0);
    }

    ma_node* filter_output = bus.lpf_initialized ? (ma_node*)&bus.lpf_node : (ma_node*)&bus.group;
    if (bus.hpf_initialized) {
        ma_node_attach_output_bus(filter_output, 0, &bus.hpf_node, 0);
        filter_output = (ma_node*)&bus.hpf_node;
    }

    // Connect to parent or reverb
    if (parent.valid()) {
        auto it = impl->buses.find(parent.id);
        if (it != impl->buses.end()) {
            ma_node_attach_output_bus(filter_output, 0, &it->second.group, 0);
        }
    } else {
        ma_node_attach_output_bus(filter_output, 0, &impl->reverb_node, 0);
    }

    impl->buses[id] = std::move(bus);
    return AudioBusHandle{id};
}

void destroy_bus_impl(AudioEngine::Impl* impl, AudioBusHandle bus) {
    if (!impl) return;
    if (bus.id < 100) return;  // Protect builtin buses
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

    auto it = impl->buses.find(bus.id);
    if (it != impl->buses.end()) {
        // Uninit filter nodes first
        if (it->second.lpf_initialized) {
            ma_lpf_node_uninit(&it->second.lpf_node, NULL);
        }
        if (it->second.hpf_initialized) {
            ma_hpf_node_uninit(&it->second.hpf_node, NULL);
        }
        if (it->second.initialized) {
            ma_sound_group_uninit(&it->second.group);
        }
        impl->buses.erase(it);
    }
}

void set_bus_volume_impl(AudioEngine::Impl* impl, AudioBusHandle bus, float volume) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->buses.find(bus.id);
    if (it != impl->buses.end() && it->second.initialized) {
        it->second.volume = AudioEngine::Impl::clamp_volume(volume);
        ma_sound_group_set_volume(&it->second.group, it->second.volume);
    }
}

float get_bus_volume_impl(AudioEngine::Impl* impl, AudioBusHandle bus) {
    if (!impl) return 1.0f;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->buses.find(bus.id);
    if (it != impl->buses.end()) return it->second.volume;
    return 1.0f;
}

void set_bus_muted_impl(AudioEngine::Impl* impl, AudioBusHandle bus, bool muted) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->buses.find(bus.id);
    if (it != impl->buses.end() && it->second.initialized) {
        it->second.muted = muted;
        // Mute logic: volume 0? or separate mute? ma_sound_group has no mute, need to use volume or stop.
        // Actually miniaudio nodes don't have a mute flag, we simulate it with volume 0,
        // but restoring is hard if we don't cache.
        // We cached 'volume'. So if muted, set group volume to 0. If unmuted, set to 'volume'.
        if (muted) {
            ma_sound_group_set_volume(&it->second.group, 0.0f);
        } else {
            ma_sound_group_set_volume(&it->second.group, it->second.volume);
        }
    }
}

bool is_bus_muted_impl(AudioEngine::Impl* impl, AudioBusHandle bus) {
    if (!impl) return false;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->buses.find(bus.id);
    if (it != impl->buses.end()) return it->second.muted;
    return false;
}

// New Sound Controls
void set_sound_paused_impl(AudioEngine::Impl* impl, SoundHandle h, bool paused) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    if (paused) {
        // Save cursor position before stopping so we can resume from same position
        ma_sound_get_cursor_in_pcm_frames(&it->second.sound, &it->second.paused_cursor);
        ma_sound_stop(&it->second.sound);
    } else {
        // Seek to saved position and resume playback
        ma_sound_seek_to_pcm_frame(&it->second.sound, it->second.paused_cursor);
        ma_sound_start(&it->second.sound);
    }
}

void set_sound_volume_handle_impl(AudioEngine::Impl* impl, SoundHandle h, float volume) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    // Set per-sound volume. Global sound_volume is handled by SFX bus.
    ma_sound_set_volume(&it->second.sound, AudioEngine::Impl::clamp_volume(volume));
}

void set_sound_pitch_handle_impl(AudioEngine::Impl* impl, SoundHandle h, float pitch) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;
    
    ma_sound_set_pitch(&it->second.sound, pitch);
}

void fade_in_impl(AudioEngine::Impl* impl, SoundHandle h, float duration) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;
    
    it->second.fading = true;
    it->second.fade_duration = duration;
    it->second.fade_elapsed = 0.0f;
    it->second.fade_start_vol = 0.0f;
    it->second.fade_target_vol = 1.0f; // Target local volume 1.0 (multiplied by globals later)
    
    ma_sound_set_volume(&it->second.sound, 0.0f);
    ma_sound_start(&it->second.sound);
}

void fade_out_impl(AudioEngine::Impl* impl, SoundHandle h, float duration) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    it->second.fading = true;
    it->second.fade_duration = std::max(duration, 0.001f);
    it->second.fade_elapsed = 0.0f;

    // Since we use local volume only (bus handles global), get the current local volume
    it->second.fade_start_vol = ma_sound_get_volume(&it->second.sound);
    it->second.fade_target_vol = 0.0f;
}

void set_reverb_params_impl(AudioEngine::Impl* impl, const AudioEngine::ReverbParams& params) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    
    verblib_set_room_size(&impl->reverb_node.reverb, params.room_size);
    verblib_set_damping(&impl->reverb_node.reverb, params.damping);
    verblib_set_width(&impl->reverb_node.reverb, params.width);
    verblib_set_wet(&impl->reverb_node.reverb, params.wet_volume);
    verblib_set_dry(&impl->reverb_node.reverb, params.dry_volume);
    // mode is not supported in standard miniaudio reverb, it is a basic verblib.
}

ma_attenuation_model map_attenuation_model(AttenuationModel model) {
    switch (model) {
        case AttenuationModel::None: return ma_attenuation_model_none;
        case AttenuationModel::InverseSquare: return ma_attenuation_model_inverse;
        case AttenuationModel::Linear: return ma_attenuation_model_linear;
        case AttenuationModel::Logarithmic: return ma_attenuation_model_exponential;
        default: return ma_attenuation_model_inverse;
    }
}

void set_sound_attenuation_model_impl(AudioEngine::Impl* impl, SoundHandle h, AttenuationModel model) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    ma_sound_set_attenuation_model(&it->second.sound, map_attenuation_model(model));
}

void set_sound_rolloff_impl(AudioEngine::Impl* impl, SoundHandle h, float rolloff) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    // Validate: rolloff must be positive
    float safe_rolloff = std::max(rolloff, 0.001f);
    ma_sound_set_rolloff(&it->second.sound, safe_rolloff);
}

void set_sound_min_max_distance_impl(AudioEngine::Impl* impl, SoundHandle h, float min_dist, float max_dist) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    // Validate: ensure min > 0 and max >= min
    float safe_min = std::max(min_dist, 0.001f);
    float safe_max = std::max(max_dist, safe_min);
    ma_sound_set_min_distance(&it->second.sound, safe_min);
    ma_sound_set_max_distance(&it->second.sound, safe_max);
}

void set_sound_cone_impl(AudioEngine::Impl* impl, SoundHandle h, float inner_angle_deg, float outer_angle_deg, float outer_gain) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    // Validate: clamp angles to [0, 360], ensure outer >= inner, clamp gain to [0, 1]
    float inner = std::clamp(inner_angle_deg, 0.0f, 360.0f);
    float outer = std::clamp(outer_angle_deg, 0.0f, 360.0f);
    if (outer < inner) std::swap(inner, outer);
    float gain = std::clamp(outer_gain, 0.0f, 1.0f);

    ma_sound_set_cone(&it->second.sound, glm::radians(inner), glm::radians(outer), gain);
}

void set_sound_doppler_factor_impl(AudioEngine::Impl* impl, SoundHandle h, float factor) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    if (it == impl->sounds.end() || !it->second.loaded) return;

    // Validate: clamp doppler factor to reasonable range [0, 10]
    float safe_factor = std::clamp(factor, 0.0f, 10.0f);
    ma_sound_set_doppler_factor(&it->second.sound, safe_factor);
}

// Error handling functions
void set_error_callback_impl(AudioEngine::Impl* impl, AudioErrorCallback callback) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    impl->error_callback = std::move(callback);
}

AudioResult get_last_error_impl(AudioEngine::Impl* impl) {
    if (!impl) return {AudioError::Unknown, "Invalid audio engine"};
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    return impl->last_error;
}

bool is_sound_valid_impl(AudioEngine::Impl* impl, SoundHandle h) {
    if (!impl || !h.valid()) return false;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->sounds.find(h.id);
    return it != impl->sounds.end() && it->second.loaded;
}

bool is_music_valid_impl(AudioEngine::Impl* impl, MusicHandle h) {
    if (!impl || !h.valid()) return false;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->music.find(h.id);
    return it != impl->music.end() && it->second.loaded;
}

bool is_bus_valid_impl(AudioEngine::Impl* impl, AudioBusHandle h) {
    if (!impl || !h.valid()) return false;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    auto it = impl->buses.find(h.id);
    return it != impl->buses.end() && it->second.initialized;
}

// Bus filter control functions
void set_bus_lowpass_impl(AudioEngine::Impl* impl, AudioBusHandle bus, float cutoff_hz, bool enabled) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

    auto it = impl->buses.find(bus.id);
    if (it == impl->buses.end() || !it->second.initialized) return;

    // Clamp cutoff to valid range [20, 20000] Hz
    cutoff_hz = std::clamp(cutoff_hz, 20.0f, 20000.0f);
    it->second.lpf_cutoff = cutoff_hz;
    it->second.lpf_enabled = enabled;

    // Apply filter settings to the node
    if (it->second.lpf_initialized) {
        ma_uint32 channels = ma_engine_get_channels(&impl->engine);
        ma_uint32 sampleRate = ma_engine_get_sample_rate(&impl->engine);

        // When disabled, set cutoff to max (20kHz) to effectively bypass
        double effectiveCutoff = enabled ? static_cast<double>(cutoff_hz) : 20000.0;

        ma_lpf_node_config lpfConfig = ma_lpf_node_config_init(channels, sampleRate, effectiveCutoff, 2);
        ma_lpf_node_reinit(&lpfConfig.lpf, &it->second.lpf_node);
    }
}

void set_bus_highpass_impl(AudioEngine::Impl* impl, AudioBusHandle bus, float cutoff_hz, bool enabled) {
    if (!impl || !impl->initialized) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

    auto it = impl->buses.find(bus.id);
    if (it == impl->buses.end() || !it->second.initialized) return;

    // Clamp cutoff to valid range [20, 20000] Hz
    cutoff_hz = std::clamp(cutoff_hz, 20.0f, 20000.0f);
    it->second.hpf_cutoff = cutoff_hz;
    it->second.hpf_enabled = enabled;

    // Apply filter settings to the node
    if (it->second.hpf_initialized) {
        ma_uint32 channels = ma_engine_get_channels(&impl->engine);
        ma_uint32 sampleRate = ma_engine_get_sample_rate(&impl->engine);

        // When disabled, set cutoff to min (20Hz) to effectively bypass
        double effectiveCutoff = enabled ? static_cast<double>(cutoff_hz) : 20.0;

        ma_hpf_node_config hpfConfig = ma_hpf_node_config_init(channels, sampleRate, effectiveCutoff, 2);
        ma_hpf_node_reinit(&hpfConfig.hpf, &it->second.hpf_node);
    }
}

AudioEngine::FilterParams get_bus_filters_impl(AudioEngine::Impl* impl, AudioBusHandle bus) {
    AudioEngine::FilterParams params;
    if (!impl) return params;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);

    auto it = impl->buses.find(bus.id);
    if (it != impl->buses.end()) {
        params.lowpass_cutoff = it->second.lpf_cutoff;
        params.lowpass_enabled = it->second.lpf_enabled;
        params.highpass_cutoff = it->second.hpf_cutoff;
        params.highpass_enabled = it->second.hpf_enabled;
    }
    return params;
}

// Voice management functions
void set_max_voices_impl(AudioEngine::Impl* impl, uint32_t count) {
    if (!impl) return;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    impl->max_voices = std::max(count, 1u);  // At least 1 voice
}

uint32_t get_max_voices_impl(AudioEngine::Impl* impl) {
    if (!impl) return 32;
    std::lock_guard<std::recursive_mutex> lock(impl->m_mutex);
    return impl->max_voices;
}

// Reverb preset mappings
AudioEngine::ReverbParams get_reverb_preset_params(ReverbPreset preset) {
    AudioEngine::ReverbParams params;

    switch (preset) {
        case ReverbPreset::None:
            params.room_size = 0.0f;
            params.damping = 0.0f;
            params.width = 0.0f;
            params.wet_volume = 0.0f;
            params.dry_volume = 1.0f;
            break;

        case ReverbPreset::SmallRoom:
            params.room_size = 0.2f;
            params.damping = 0.7f;
            params.width = 0.5f;
            params.wet_volume = 0.2f;
            params.dry_volume = 1.0f;
            break;

        case ReverbPreset::MediumRoom:
            params.room_size = 0.4f;
            params.damping = 0.5f;
            params.width = 0.7f;
            params.wet_volume = 0.3f;
            params.dry_volume = 1.0f;
            break;

        case ReverbPreset::LargeRoom:
            params.room_size = 0.6f;
            params.damping = 0.4f;
            params.width = 0.8f;
            params.wet_volume = 0.35f;
            params.dry_volume = 1.0f;
            break;

        case ReverbPreset::Hall:
            params.room_size = 0.75f;
            params.damping = 0.3f;
            params.width = 1.0f;
            params.wet_volume = 0.4f;
            params.dry_volume = 1.0f;
            break;

        case ReverbPreset::Cathedral:
            params.room_size = 0.9f;
            params.damping = 0.2f;
            params.width = 1.0f;
            params.wet_volume = 0.5f;
            params.dry_volume = 0.9f;
            break;

        case ReverbPreset::Cave:
            params.room_size = 0.85f;
            params.damping = 0.1f;
            params.width = 1.0f;
            params.wet_volume = 0.6f;
            params.dry_volume = 0.8f;
            break;

        case ReverbPreset::Underwater:
            params.room_size = 0.7f;
            params.damping = 0.9f;  // Heavy damping for muffled effect
            params.width = 0.3f;
            params.wet_volume = 0.7f;
            params.dry_volume = 0.5f;
            break;

        case ReverbPreset::Bathroom:
            params.room_size = 0.15f;
            params.damping = 0.2f;  // Reflective tiles
            params.width = 0.4f;
            params.wet_volume = 0.4f;
            params.dry_volume = 1.0f;
            break;

        case ReverbPreset::Arena:
            params.room_size = 0.95f;
            params.damping = 0.35f;
            params.width = 1.0f;
            params.wet_volume = 0.45f;
            params.dry_volume = 0.95f;
            break;

        case ReverbPreset::Forest:
            params.room_size = 0.3f;
            params.damping = 0.8f;  // Trees absorb sound
            params.width = 1.0f;
            params.wet_volume = 0.15f;
            params.dry_volume = 1.0f;
            break;

        case ReverbPreset::Custom:
        default:
            // Return default values for custom
            break;
    }

    return params;
}

} // namespace engine::audio

// Include reverb node implementation
// Include implementations (extern "C" to link correctly)
extern "C" {
    #include <miniaudio.c>
    
    #define VERBLIB_IMPLEMENTATION
    #include <verblib.h>

    #ifndef MA_ZERO_OBJECT
    #define MA_ZERO_OBJECT(p) memset((void*)(p), 0, sizeof(*(p)))
    #endif
    
    #include <ma_reverb_node.c>
}
