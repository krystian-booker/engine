#pragma once

#include <string>
#include <cstdint>

namespace engine::settings {

// ============================================================================
// Speaker Configuration
// ============================================================================

enum class SpeakerConfig : uint8_t {
    Stereo,
    Quadraphonic,
    Surround_5_1,
    Surround_7_1,
    Auto
};

// ============================================================================
// Audio Quality
// ============================================================================

enum class AudioQuality : uint8_t {
    Low,        // 22kHz
    Medium,     // 44.1kHz
    High,       // 48kHz
    Ultra       // 96kHz
};

// ============================================================================
// Audio Settings
// ============================================================================

struct AudioSettings {
    // ========================================================================
    // Volume Levels (0.0 - 1.0)
    // ========================================================================

    float master_volume = 1.0f;
    float music_volume = 0.8f;
    float sfx_volume = 1.0f;
    float voice_volume = 1.0f;
    float ambient_volume = 0.7f;
    float ui_volume = 0.8f;

    // ========================================================================
    // Output Settings
    // ========================================================================

    int output_device_index = 0;    // Index into available devices
    SpeakerConfig speaker_config = SpeakerConfig::Auto;
    AudioQuality quality = AudioQuality::High;

    // ========================================================================
    // 3D Audio
    // ========================================================================

    bool enable_3d_audio = true;
    bool enable_hrtf = false;        // Head-related transfer function
    float doppler_scale = 1.0f;
    float distance_scale = 1.0f;

    // ========================================================================
    // Voice Chat (if applicable)
    // ========================================================================

    int input_device_index = 0;
    float voice_chat_volume = 0.8f;
    float microphone_sensitivity = 0.5f;
    bool push_to_talk = true;
    bool voice_activity_detection = false;
    float vad_threshold = 0.02f;

    // ========================================================================
    // Behavior
    // ========================================================================

    bool mute_when_unfocused = true;
    bool mute_when_minimized = true;
    bool enable_subtitles_audio_cues = true;

    // ========================================================================
    // Dynamic Range
    // ========================================================================

    bool dynamic_range_compression = false;
    float compression_ratio = 0.5f;  // 0 = full dynamics, 1 = heavy compression

    // ========================================================================
    // Music Settings
    // ========================================================================

    bool crossfade_music = true;
    float crossfade_duration = 2.0f;
    bool adaptive_music = true;

    // ========================================================================
    // Methods
    // ========================================================================

    void validate();
    float get_effective_volume(float base_volume) const;
    bool operator==(const AudioSettings& other) const = default;
};

// ============================================================================
// Helper Functions
// ============================================================================

std::string get_speaker_config_name(SpeakerConfig config);
std::string get_audio_quality_name(AudioQuality quality);
int get_sample_rate(AudioQuality quality);

} // namespace engine::settings
