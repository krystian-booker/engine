#pragma once

#include <cstdint>
#include <string>

namespace engine::audio {

// Audio error codes for error handling
enum class AudioError : uint8_t {
    None = 0,        // No error
    FileNotFound,    // Audio file does not exist
    InvalidFormat,   // Unsupported audio format
    DecodingFailed,  // Failed to decode audio data
    DeviceError,     // Audio device initialization failed
    OutOfMemory,     // Failed to allocate resources
    InvalidHandle,   // Invalid sound/music handle
    PlaybackFailed,  // Failed to start playback
    Unknown          // Unknown error
};

// Result structure for operations that can fail
struct AudioResult {
    AudioError error = AudioError::None;
    std::string message;

    bool ok() const { return error == AudioError::None; }
    explicit operator bool() const { return ok(); }
};

// Sound handle (for short sound effects)
struct SoundHandle {
    uint32_t id = UINT32_MAX;
    bool valid() const { return id != UINT32_MAX; }
};

// Music handle (for streaming music)
struct MusicHandle {
    uint32_t id = UINT32_MAX;
    bool valid() const { return id != UINT32_MAX; }
};

// Audio bus handle (for mixing groups)
struct AudioBusHandle {
    uint32_t id = UINT32_MAX;
    bool valid() const { return id != UINT32_MAX; }
};

// Built-in audio bus IDs
enum class BuiltinBus : uint32_t {
    Master = 0,
    Music = 1,
    SFX = 2,
    Voice = 3,
    Ambient = 4,
    UI = 5
};

// Sound playback state
enum class PlaybackState : uint8_t {
    Stopped,
    Playing,
    Paused
};

// Sound configuration
struct SoundConfig {
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;           // -1 = left, 0 = center, 1 = right
    bool loop = false;
    bool spatial = false;       // 3D positioned audio
    AudioBusHandle bus;         // Output bus (default = SFX bus)
    float priority = 1.0f;      // Voice priority (higher = less likely to be stolen)
};

// 3D audio source settings
struct SpatialConfig {
    float min_distance = 1.0f;   // Distance at which sound is at full volume
    float max_distance = 100.0f; // Distance at which sound is inaudible
    float rolloff_factor = 1.0f; // How quickly sound attenuates with distance
};

} // namespace engine::audio
