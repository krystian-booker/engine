#include <engine/asset/streaming.hpp>
#include <engine/core/filesystem.hpp>
#include <engine/core/log.hpp>
#include <engine/render/renderer.hpp>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <algorithm>

// dr_libs for audio decoding (implementations are in audio_loader.cpp)
#include <dr_wav.h>
#include <dr_mp3.h>
#include <dr_flac.h>

// stb_vorbis for ogg
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#include <stb_image.h>

namespace engine::asset {

using namespace engine::core;

// =============================================================================
// AudioStream Implementation
// =============================================================================

struct AudioStream::Impl {
    std::string path;
    uint32_t sample_rate = 0;
    uint32_t channels = 0;
    size_t total_frames = 0;
    size_t current_frame = 0;

    enum class Format { None, Wav, Mp3, Flac, Ogg };
    Format format = Format::None;

    // Format-specific decoders
    drwav wav = {};
    drmp3 mp3 = {};
    drflac* flac = nullptr;
    stb_vorbis* vorbis = nullptr;

    bool is_open = false;

    ~Impl() {
        close();
    }

    void close() {
        switch (format) {
            case Format::Wav:
                drwav_uninit(&wav);
                break;
            case Format::Mp3:
                drmp3_uninit(&mp3);
                break;
            case Format::Flac:
                if (flac) drflac_close(flac);
                flac = nullptr;
                break;
            case Format::Ogg:
                if (vorbis) stb_vorbis_close(vorbis);
                vorbis = nullptr;
                break;
            default:
                break;
        }
        format = Format::None;
        is_open = false;
    }
};

AudioStream::AudioStream() : m_impl(std::make_unique<Impl>()) {}
AudioStream::~AudioStream() = default;

AudioStream::AudioStream(AudioStream&& other) noexcept = default;
AudioStream& AudioStream::operator=(AudioStream&& other) noexcept = default;

bool AudioStream::open(const std::string& path) {
    m_impl->close();
    m_impl->path = path;

    // Detect format from extension
    std::string ext;
    size_t dot_pos = path.rfind('.');
    if (dot_pos != std::string::npos) {
        ext = path.substr(dot_pos);
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }

    if (ext == ".wav") {
        if (!drwav_init_file(&m_impl->wav, path.c_str(), nullptr)) {
            log(LogLevel::Error, ("Failed to open WAV stream: " + path).c_str());
            return false;
        }
        m_impl->format = Impl::Format::Wav;
        m_impl->sample_rate = m_impl->wav.sampleRate;
        m_impl->channels = m_impl->wav.channels;
        m_impl->total_frames = m_impl->wav.totalPCMFrameCount;
    }
    else if (ext == ".mp3") {
        if (!drmp3_init_file(&m_impl->mp3, path.c_str(), nullptr)) {
            log(LogLevel::Error, ("Failed to open MP3 stream: " + path).c_str());
            return false;
        }
        m_impl->format = Impl::Format::Mp3;
        m_impl->sample_rate = m_impl->mp3.sampleRate;
        m_impl->channels = m_impl->mp3.channels;
        m_impl->total_frames = drmp3_get_pcm_frame_count(&m_impl->mp3);
    }
    else if (ext == ".flac") {
        m_impl->flac = drflac_open_file(path.c_str(), nullptr);
        if (!m_impl->flac) {
            log(LogLevel::Error, ("Failed to open FLAC stream: " + path).c_str());
            return false;
        }
        m_impl->format = Impl::Format::Flac;
        m_impl->sample_rate = m_impl->flac->sampleRate;
        m_impl->channels = m_impl->flac->channels;
        m_impl->total_frames = m_impl->flac->totalPCMFrameCount;
    }
    else if (ext == ".ogg") {
        int error;
        m_impl->vorbis = stb_vorbis_open_filename(path.c_str(), &error, nullptr);
        if (!m_impl->vorbis) {
            log(LogLevel::Error, ("Failed to open OGG stream: " + path).c_str());
            return false;
        }
        m_impl->format = Impl::Format::Ogg;
        stb_vorbis_info info = stb_vorbis_get_info(m_impl->vorbis);
        m_impl->sample_rate = info.sample_rate;
        m_impl->channels = info.channels;
        m_impl->total_frames = stb_vorbis_stream_length_in_samples(m_impl->vorbis);
    }
    else {
        log(LogLevel::Error, ("Unsupported audio format for streaming: " + path).c_str());
        return false;
    }

    m_impl->current_frame = 0;
    m_impl->is_open = true;
    log(LogLevel::Debug, ("Opened audio stream: " + path).c_str());
    return true;
}

void AudioStream::close() {
    m_impl->close();
}

bool AudioStream::is_open() const {
    return m_impl->is_open;
}

size_t AudioStream::read(int16_t* buffer, size_t sample_count) {
    if (!m_impl->is_open) return 0;

    size_t frames_to_read = sample_count / m_impl->channels;
    size_t frames_read = 0;

    switch (m_impl->format) {
        case Impl::Format::Wav:
            frames_read = drwav_read_pcm_frames_s16(&m_impl->wav, frames_to_read, buffer);
            break;
        case Impl::Format::Mp3:
            frames_read = drmp3_read_pcm_frames_s16(&m_impl->mp3, frames_to_read, buffer);
            break;
        case Impl::Format::Flac:
            frames_read = drflac_read_pcm_frames_s16(m_impl->flac, frames_to_read, buffer);
            break;
        case Impl::Format::Ogg: {
            // stb_vorbis works with shorts directly
            int samples = stb_vorbis_get_samples_short_interleaved(
                m_impl->vorbis,
                static_cast<int>(m_impl->channels),
                buffer,
                static_cast<int>(sample_count)
            );
            frames_read = static_cast<size_t>(samples);
            break;
        }
        default:
            break;
    }

    m_impl->current_frame += frames_read;
    return frames_read * m_impl->channels;
}

size_t AudioStream::read_float(float* buffer, size_t sample_count) {
    if (!m_impl->is_open) return 0;

    size_t frames_to_read = sample_count / m_impl->channels;
    size_t frames_read = 0;

    switch (m_impl->format) {
        case Impl::Format::Wav:
            frames_read = drwav_read_pcm_frames_f32(&m_impl->wav, frames_to_read, buffer);
            break;
        case Impl::Format::Mp3:
            frames_read = drmp3_read_pcm_frames_f32(&m_impl->mp3, frames_to_read, buffer);
            break;
        case Impl::Format::Flac:
            frames_read = drflac_read_pcm_frames_f32(m_impl->flac, frames_to_read, buffer);
            break;
        case Impl::Format::Ogg: {
            int samples = stb_vorbis_get_samples_float_interleaved(
                m_impl->vorbis,
                static_cast<int>(m_impl->channels),
                buffer,
                static_cast<int>(sample_count)
            );
            frames_read = static_cast<size_t>(samples);
            break;
        }
        default:
            break;
    }

    m_impl->current_frame += frames_read;
    return frames_read * m_impl->channels;
}

bool AudioStream::seek(size_t sample_offset) {
    if (!m_impl->is_open) return false;

    bool success = false;
    size_t frame_offset = sample_offset / m_impl->channels;

    switch (m_impl->format) {
        case Impl::Format::Wav:
            success = drwav_seek_to_pcm_frame(&m_impl->wav, frame_offset);
            break;
        case Impl::Format::Mp3:
            success = drmp3_seek_to_pcm_frame(&m_impl->mp3, frame_offset);
            break;
        case Impl::Format::Flac:
            success = drflac_seek_to_pcm_frame(m_impl->flac, frame_offset);
            break;
        case Impl::Format::Ogg:
            success = stb_vorbis_seek(m_impl->vorbis, static_cast<unsigned int>(frame_offset)) != 0;
            break;
        default:
            break;
    }

    if (success) {
        m_impl->current_frame = frame_offset;
    }
    return success;
}

size_t AudioStream::tell() const {
    return m_impl->current_frame * m_impl->channels;
}

uint32_t AudioStream::sample_rate() const {
    return m_impl->sample_rate;
}

uint32_t AudioStream::channels() const {
    return m_impl->channels;
}

size_t AudioStream::total_samples() const {
    return m_impl->total_frames * m_impl->channels;
}

float AudioStream::duration() const {
    if (m_impl->sample_rate == 0) return 0.0f;
    return static_cast<float>(m_impl->total_frames) / static_cast<float>(m_impl->sample_rate);
}

const std::string& AudioStream::path() const {
    return m_impl->path;
}

// =============================================================================
// TextureStream Implementation
// =============================================================================

struct TextureStream::Impl {
    std::string path;
    render::IRenderer* renderer = nullptr;
    render::TextureHandle handle;

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mip_count = 0;
    uint32_t loaded_mip_level = UINT32_MAX;  // Coarsest loaded mip (higher = coarser)

    std::vector<bool> mip_loaded;
    std::vector<bool> mip_requested;

    bool is_open = false;

    ~Impl() {
        close();
    }

    void close() {
        if (renderer && handle.valid()) {
            renderer->destroy_texture(handle);
            handle = {};
        }
        is_open = false;
    }
};

TextureStream::TextureStream() : m_impl(std::make_unique<Impl>()) {}
TextureStream::~TextureStream() = default;

TextureStream::TextureStream(TextureStream&& other) noexcept = default;
TextureStream& TextureStream::operator=(TextureStream&& other) noexcept = default;

bool TextureStream::open(const std::string& path, render::IRenderer* renderer) {
    m_impl->close();

    if (!renderer) {
        log(LogLevel::Error, "TextureStream::open: renderer is null");
        return false;
    }

    m_impl->path = path;
    m_impl->renderer = renderer;

    // For now, load the texture normally and track it as fully loaded
    // In a full implementation, this would load only metadata and lowest mips
    auto file_data = FileSystem::read_binary(path);
    if (file_data.empty()) {
        log(LogLevel::Error, ("Failed to open texture for streaming: " + path).c_str());
        return false;
    }

    // Detect dimensions from the file (simplified - loads entire texture for now)
    // A proper implementation would parse headers without loading pixel data
    int width, height, channels;
    stbi_info_from_memory(file_data.data(), static_cast<int>(file_data.size()),
                          &width, &height, &channels);

    if (width == 0 || height == 0) {
        log(LogLevel::Error, ("Invalid texture dimensions: " + path).c_str());
        return false;
    }

    m_impl->width = static_cast<uint32_t>(width);
    m_impl->height = static_cast<uint32_t>(height);

    // Calculate mip count
    m_impl->mip_count = 1;
    uint32_t w = m_impl->width, h = m_impl->height;
    while (w > 1 || h > 1) {
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
        m_impl->mip_count++;
    }

    m_impl->mip_loaded.resize(m_impl->mip_count, false);
    m_impl->mip_requested.resize(m_impl->mip_count, false);

    // Load lowest mip (coarsest) immediately for preview
    // For simplicity, load full texture for now
    unsigned char* data = stbi_load_from_memory(
        file_data.data(), static_cast<int>(file_data.size()),
        &width, &height, &channels, 4
    );

    if (!data) {
        log(LogLevel::Error, ("Failed to decode texture for streaming: " + path).c_str());
        return false;
    }

    render::TextureData tex_data;
    tex_data.width = m_impl->width;
    tex_data.height = m_impl->height;
    tex_data.format = render::TextureFormat::RGBA8;
    tex_data.pixels.assign(data, data + width * height * 4);
    stbi_image_free(data);

    m_impl->handle = renderer->create_texture(tex_data);
    if (!m_impl->handle.valid()) {
        log(LogLevel::Error, ("Failed to create streaming texture: " + path).c_str());
        return false;
    }

    // Mark all mips as loaded (simplified implementation)
    std::fill(m_impl->mip_loaded.begin(), m_impl->mip_loaded.end(), true);
    m_impl->loaded_mip_level = 0;

    m_impl->is_open = true;
    log(LogLevel::Debug, ("Opened texture stream: " + path).c_str());
    return true;
}

void TextureStream::close() {
    m_impl->close();
}

bool TextureStream::is_open() const {
    return m_impl->is_open;
}

void TextureStream::request_mip(uint32_t level) {
    if (!m_impl->is_open || level >= m_impl->mip_count) return;

    if (!m_impl->mip_loaded[level] && !m_impl->mip_requested[level]) {
        m_impl->mip_requested[level] = true;
        // In a full implementation, this would queue an async load
    }
}

bool TextureStream::is_mip_loaded(uint32_t level) const {
    if (!m_impl->is_open || level >= m_impl->mip_count) return false;
    return m_impl->mip_loaded[level];
}

uint32_t TextureStream::get_loaded_mip_level() const {
    return m_impl->loaded_mip_level;
}

render::TextureHandle TextureStream::get_handle() const {
    return m_impl->handle;
}

void TextureStream::update() {
    // In a full implementation, this would check for completed async loads
    // and update the texture with newly loaded mip levels
}

uint32_t TextureStream::width() const {
    return m_impl->width;
}

uint32_t TextureStream::height() const {
    return m_impl->height;
}

uint32_t TextureStream::mip_count() const {
    return m_impl->mip_count;
}

const std::string& TextureStream::path() const {
    return m_impl->path;
}

// =============================================================================
// StreamHandle Implementation
// =============================================================================

struct StreamHandle::Impl {
    std::atomic<bool> ready{false};
    std::atomic<float> progress{0.0f};
    std::atomic<bool> cancelled{false};
    std::mutex mutex;
    std::condition_variable cv;
};

StreamHandle::StreamHandle() : m_impl(std::make_unique<Impl>()) {}
StreamHandle::~StreamHandle() = default;

StreamHandle::StreamHandle(StreamHandle&& other) noexcept = default;
StreamHandle& StreamHandle::operator=(StreamHandle&& other) noexcept = default;

bool StreamHandle::is_ready() const {
    return m_impl && m_impl->ready.load();
}

float StreamHandle::progress() const {
    return m_impl ? m_impl->progress.load() : 0.0f;
}

void StreamHandle::wait() {
    if (!m_impl) return;
    std::unique_lock<std::mutex> lock(m_impl->mutex);
    m_impl->cv.wait(lock, [this] { return m_impl->ready.load() || m_impl->cancelled.load(); });
}

void StreamHandle::cancel() {
    if (m_impl) {
        m_impl->cancelled.store(true);
        m_impl->cv.notify_all();
    }
}

bool StreamHandle::valid() const {
    return m_impl != nullptr;
}

} // namespace engine::asset
