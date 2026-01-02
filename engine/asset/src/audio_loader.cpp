#include <engine/asset/audio_loader.hpp>
#include <algorithm>
#include <cctype>
#include <cstdio>

// dr_libs implementations
#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>

#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>

// stb_vorbis for OGG support
#include <stb_vorbis.c>

namespace engine::asset {

static std::string s_last_error;

static std::string get_extension(const std::string& path) {
    size_t pos = path.rfind('.');
    if (pos == std::string::npos) return "";

    std::string ext = path.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

static bool load_wav(const std::string& path, std::vector<uint8_t>& out_data, AudioFormat& out_format) {
    drwav wav;
    if (!drwav_init_file(&wav, path.c_str(), nullptr)) {
        s_last_error = "Failed to open WAV file: " + path;
        return false;
    }

    out_format.sample_rate = wav.sampleRate;
    out_format.channels = wav.channels;
    out_format.bits_per_sample = 16;
    out_format.total_frames = wav.totalPCMFrameCount;

    size_t sample_count = static_cast<size_t>(wav.totalPCMFrameCount * wav.channels);
    size_t byte_size = sample_count * sizeof(int16_t);
    out_data.resize(byte_size);

    drwav_uint64 frames_read = drwav_read_pcm_frames_s16(
        &wav,
        wav.totalPCMFrameCount,
        reinterpret_cast<drwav_int16*>(out_data.data())
    );

    drwav_uninit(&wav);

    if (frames_read != wav.totalPCMFrameCount) {
        s_last_error = "Failed to read all WAV frames";
        return false;
    }

    return true;
}

static bool load_mp3(const std::string& path, std::vector<uint8_t>& out_data, AudioFormat& out_format) {
    drmp3 mp3;
    if (!drmp3_init_file(&mp3, path.c_str(), nullptr)) {
        s_last_error = "Failed to open MP3 file: " + path;
        return false;
    }

    out_format.sample_rate = mp3.sampleRate;
    out_format.channels = mp3.channels;
    out_format.bits_per_sample = 16;

    // Get total frame count
    drmp3_uint64 total_frames = drmp3_get_pcm_frame_count(&mp3);
    out_format.total_frames = total_frames;

    size_t sample_count = static_cast<size_t>(total_frames * mp3.channels);
    size_t byte_size = sample_count * sizeof(int16_t);
    out_data.resize(byte_size);

    drmp3_uint64 frames_read = drmp3_read_pcm_frames_s16(
        &mp3,
        total_frames,
        reinterpret_cast<drmp3_int16*>(out_data.data())
    );

    drmp3_uninit(&mp3);

    if (frames_read != total_frames) {
        s_last_error = "Failed to read all MP3 frames";
        return false;
    }

    return true;
}

static bool load_flac(const std::string& path, std::vector<uint8_t>& out_data, AudioFormat& out_format) {
    drflac* flac = drflac_open_file(path.c_str(), nullptr);
    if (!flac) {
        s_last_error = "Failed to open FLAC file: " + path;
        return false;
    }

    out_format.sample_rate = flac->sampleRate;
    out_format.channels = flac->channels;
    out_format.bits_per_sample = 16;
    out_format.total_frames = flac->totalPCMFrameCount;

    size_t sample_count = static_cast<size_t>(flac->totalPCMFrameCount * flac->channels);
    size_t byte_size = sample_count * sizeof(int16_t);
    out_data.resize(byte_size);

    drflac_uint64 frames_read = drflac_read_pcm_frames_s16(
        flac,
        flac->totalPCMFrameCount,
        reinterpret_cast<drflac_int16*>(out_data.data())
    );

    drflac_close(flac);

    if (frames_read != flac->totalPCMFrameCount) {
        s_last_error = "Failed to read all FLAC frames";
        return false;
    }

    return true;
}

static bool load_ogg(const std::string& path, std::vector<uint8_t>& out_data, AudioFormat& out_format) {
    int channels = 0;
    int sample_rate = 0;
    short* samples = nullptr;

    int sample_count = stb_vorbis_decode_filename(
        path.c_str(),
        &channels,
        &sample_rate,
        &samples
    );

    if (sample_count < 0 || !samples) {
        s_last_error = "Failed to decode OGG file: " + path;
        return false;
    }

    out_format.sample_rate = static_cast<uint32_t>(sample_rate);
    out_format.channels = static_cast<uint32_t>(channels);
    out_format.bits_per_sample = 16;
    out_format.total_frames = static_cast<uint64_t>(sample_count);

    size_t byte_size = static_cast<size_t>(sample_count) * static_cast<size_t>(channels) * sizeof(int16_t);
    out_data.resize(byte_size);
    std::memcpy(out_data.data(), samples, byte_size);

    free(samples);

    return true;
}

bool AudioLoader::load(const std::string& path,
                       std::vector<uint8_t>& out_data,
                       AudioFormat& out_format) {
    s_last_error.clear();
    std::string ext = get_extension(path);

    if (ext == ".wav") {
        return load_wav(path, out_data, out_format);
    } else if (ext == ".mp3") {
        return load_mp3(path, out_data, out_format);
    } else if (ext == ".flac") {
        return load_flac(path, out_data, out_format);
    } else if (ext == ".ogg") {
        return load_ogg(path, out_data, out_format);
    }

    s_last_error = "Unsupported audio format: " + ext;
    return false;
}

const std::string& AudioLoader::get_last_error() {
    return s_last_error;
}

} // namespace engine::asset
