#include <engine/settings/audio_settings.hpp>
#include <algorithm>

namespace engine::settings {

// ============================================================================
// AudioSettings
// ============================================================================

void AudioSettings::validate() {
    master_volume = std::clamp(master_volume, 0.0f, 1.0f);
    music_volume = std::clamp(music_volume, 0.0f, 1.0f);
    sfx_volume = std::clamp(sfx_volume, 0.0f, 1.0f);
    voice_volume = std::clamp(voice_volume, 0.0f, 1.0f);
    ambient_volume = std::clamp(ambient_volume, 0.0f, 1.0f);
    ui_volume = std::clamp(ui_volume, 0.0f, 1.0f);
    voice_chat_volume = std::clamp(voice_chat_volume, 0.0f, 1.0f);
    microphone_sensitivity = std::clamp(microphone_sensitivity, 0.0f, 1.0f);
    vad_threshold = std::clamp(vad_threshold, 0.0f, 1.0f);
    doppler_scale = std::clamp(doppler_scale, 0.0f, 5.0f);
    distance_scale = std::clamp(distance_scale, 0.1f, 10.0f);
    compression_ratio = std::clamp(compression_ratio, 0.0f, 1.0f);
    crossfade_duration = std::clamp(crossfade_duration, 0.0f, 10.0f);
}

float AudioSettings::get_effective_volume(float base_volume) const {
    return base_volume * master_volume;
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string get_speaker_config_name(SpeakerConfig config) {
    switch (config) {
        case SpeakerConfig::Stereo:       return "Stereo";
        case SpeakerConfig::Quadraphonic: return "Quadraphonic";
        case SpeakerConfig::Surround_5_1: return "5.1 Surround";
        case SpeakerConfig::Surround_7_1: return "7.1 Surround";
        case SpeakerConfig::Auto:         return "Auto";
        default:                          return "Unknown";
    }
}

std::string get_audio_quality_name(AudioQuality quality) {
    switch (quality) {
        case AudioQuality::Low:    return "Low (22kHz)";
        case AudioQuality::Medium: return "Medium (44.1kHz)";
        case AudioQuality::High:   return "High (48kHz)";
        case AudioQuality::Ultra:  return "Ultra (96kHz)";
        default:                   return "Unknown";
    }
}

int get_sample_rate(AudioQuality quality) {
    switch (quality) {
        case AudioQuality::Low:    return 22050;
        case AudioQuality::Medium: return 44100;
        case AudioQuality::High:   return 48000;
        case AudioQuality::Ultra:  return 96000;
        default:                   return 48000;
    }
}

} // namespace engine::settings
