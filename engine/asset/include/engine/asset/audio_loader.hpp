#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace engine::asset {

struct AudioFormat {
    uint32_t sample_rate = 0;
    uint32_t channels = 0;
    uint32_t bits_per_sample = 16;
    uint64_t total_frames = 0;
};

struct AudioLoader {
    // Parse and decode audio file to PCM samples (16-bit signed)
    // Supports: .wav, .mp3, .flac, .ogg
    static bool load(
        const std::string& path,
        std::vector<uint8_t>& out_pcm_data,
        AudioFormat& out_format);

    // Get last error message (returns reference to static error string)
    static const std::string& get_last_error();
};

} // namespace engine::asset
