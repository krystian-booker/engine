#include <engine/asset/streaming.hpp>
#include <engine/core/filesystem.hpp>
#include <engine/core/log.hpp>
#include <engine/render/renderer.hpp>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <algorithm>
#include <climits>

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

// RAII wrapper for stbi allocations
struct StbiImageDeleter {
    void operator()(unsigned char* p) const { if (p) stbi_image_free(p); }
};
using StbiImagePtr = std::unique_ptr<unsigned char, StbiImageDeleter>;

struct TextureStream::Impl : public std::enable_shared_from_this<TextureStream::Impl> {
    std::string path;
    render::IRenderer* renderer = nullptr;
    render::TextureHandle handle;

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mip_count = 0;
    uint32_t loaded_mip_level = UINT32_MAX;  // Finest loaded mip (lower = finer)

    // Thread synchronization
    mutable std::mutex mutex;

    // Mip state tracking
    std::vector<bool> mip_loaded;
    std::vector<bool> mip_requested;
    std::vector<bool> mip_loading;  // Currently being loaded async

    // Pending mip data from async loads
    struct PendingMip {
        uint32_t level;
        std::vector<uint8_t> data;
        uint32_t width;
        uint32_t height;
    };
    std::vector<PendingMip> pending_mips;
    std::mutex pending_mutex;

    // Thread management for async mip loading
    std::vector<std::thread> active_threads;
    std::mutex threads_mutex;
    std::atomic<bool> stop_requested{false};

    // Raw file data for streaming (kept in memory for on-demand mip generation)
    std::vector<uint8_t> file_data;
    int original_channels = 0;

    bool is_open = false;

    ~Impl() {
        close();
    }

    void close() {
        // Signal all threads to stop
        stop_requested.store(true);

        // Join all active threads before cleanup
        {
            std::lock_guard<std::mutex> tlock(threads_mutex);
            for (auto& t : active_threads) {
                if (t.joinable()) {
                    t.join();
                }
            }
            active_threads.clear();
        }

        std::lock_guard<std::mutex> lock(mutex);
        if (renderer && handle.valid()) {
            renderer->destroy_texture(handle);
            handle = {};
        }
        file_data.clear();
        mip_loaded.clear();
        mip_requested.clear();
        mip_loading.clear();
        {
            std::lock_guard<std::mutex> plock(pending_mutex);
            pending_mips.clear();
        }
        is_open = false;
        stop_requested.store(false);  // Reset for potential reuse
    }

    // Generate a specific mip level from source data
    std::vector<uint8_t> generate_mip(uint32_t level, uint32_t& out_width, uint32_t& out_height) {
        // Calculate dimensions for this mip level
        out_width = std::max(1u, width >> level);
        out_height = std::max(1u, height >> level);

        // Check file size before casting to int (stbi limitation)
        if (file_data.size() > static_cast<size_t>(INT_MAX)) {
            log(LogLevel::Error, "Texture file too large for streaming (>2GB)");
            return {};
        }

        // Load base texture from file data (RAII wrapper handles cleanup)
        int w, h, c;
        StbiImagePtr base_data(stbi_load_from_memory(
            file_data.data(), static_cast<int>(file_data.size()),
            &w, &h, &c, 4
        ));

        if (!base_data) {
            return {};
        }

        std::vector<uint8_t> result;

        if (level == 0) {
            // Level 0 is the base image
            result.assign(base_data.get(), base_data.get() + w * h * 4);
        } else {
            // Generate downsampled mip using box filter
            std::vector<uint8_t> current(base_data.get(), base_data.get() + w * h * 4);
            uint32_t curr_w = static_cast<uint32_t>(w);
            uint32_t curr_h = static_cast<uint32_t>(h);

            for (uint32_t l = 0; l < level; l++) {
                uint32_t next_w = std::max(1u, curr_w / 2);
                uint32_t next_h = std::max(1u, curr_h / 2);
                std::vector<uint8_t> next(next_w * next_h * 4);

                for (uint32_t y = 0; y < next_h; y++) {
                    for (uint32_t x = 0; x < next_w; x++) {
                        uint32_t src_x = x * 2;
                        uint32_t src_y = y * 2;

                        uint32_t r = 0, g = 0, b = 0, a = 0;
                        int count = 0;

                        for (uint32_t dy = 0; dy < 2 && (src_y + dy) < curr_h; dy++) {
                            for (uint32_t dx = 0; dx < 2 && (src_x + dx) < curr_w; dx++) {
                                size_t idx = ((src_y + dy) * curr_w + (src_x + dx)) * 4;
                                r += current[idx + 0];
                                g += current[idx + 1];
                                b += current[idx + 2];
                                a += current[idx + 3];
                                count++;
                            }
                        }

                        size_t dst_idx = (y * next_w + x) * 4;
                        next[dst_idx + 0] = static_cast<uint8_t>(r / count);
                        next[dst_idx + 1] = static_cast<uint8_t>(g / count);
                        next[dst_idx + 2] = static_cast<uint8_t>(b / count);
                        next[dst_idx + 3] = static_cast<uint8_t>(a / count);
                    }
                }

                current = std::move(next);
                curr_w = next_w;
                curr_h = next_h;
            }

            result = std::move(current);
        }

        // base_data automatically freed by RAII wrapper
        return result;
    }
};

TextureStream::TextureStream() : m_impl(std::make_shared<Impl>()) {}
TextureStream::~TextureStream() = default;

TextureStream::TextureStream(TextureStream&& other) noexcept = default;
TextureStream& TextureStream::operator=(TextureStream&& other) noexcept = default;

bool TextureStream::open(const std::string& path, render::IRenderer* renderer) {
    m_impl->close();

    if (!renderer) {
        log(LogLevel::Error, "TextureStream::open: renderer is null");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    m_impl->path = path;
    m_impl->renderer = renderer;

    // Load file data into memory for streaming
    m_impl->file_data = FileSystem::read_binary(path);
    if (m_impl->file_data.empty()) {
        log(LogLevel::Error, ("Failed to open texture for streaming: " + path).c_str());
        return false;
    }

    // Check file size before casting to int (stbi limitation)
    if (m_impl->file_data.size() > static_cast<size_t>(INT_MAX)) {
        log(LogLevel::Error, ("Texture file too large for streaming (>2GB): " + path).c_str());
        m_impl->file_data.clear();
        return false;
    }

    // Get image dimensions without fully decoding
    int width, height, channels;
    if (!stbi_info_from_memory(m_impl->file_data.data(),
                               static_cast<int>(m_impl->file_data.size()),
                               &width, &height, &channels)) {
        log(LogLevel::Error, ("Failed to read texture info: " + path).c_str());
        return false;
    }

    if (width == 0 || height == 0) {
        log(LogLevel::Error, ("Invalid texture dimensions: " + path).c_str());
        return false;
    }

    m_impl->width = static_cast<uint32_t>(width);
    m_impl->height = static_cast<uint32_t>(height);
    m_impl->original_channels = channels;

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
    m_impl->mip_loading.resize(m_impl->mip_count, false);

    // Load the coarsest mip level immediately (for quick preview)
    uint32_t coarsest_level = m_impl->mip_count - 1;
    uint32_t mip_w, mip_h;
    auto coarsest_data = m_impl->generate_mip(coarsest_level, mip_w, mip_h);

    if (coarsest_data.empty()) {
        log(LogLevel::Error, ("Failed to generate coarsest mip for streaming: " + path).c_str());
        return false;
    }

    // Create initial texture with coarsest mip
    render::TextureData tex_data;
    tex_data.width = mip_w;
    tex_data.height = mip_h;
    tex_data.format = render::TextureFormat::RGBA8;
    tex_data.pixels = std::move(coarsest_data);
    tex_data.mip_levels = 1;

    m_impl->handle = renderer->create_texture(tex_data);
    if (!m_impl->handle.valid()) {
        log(LogLevel::Error, ("Failed to create streaming texture: " + path).c_str());
        return false;
    }

    m_impl->mip_loaded[coarsest_level] = true;
    m_impl->loaded_mip_level = coarsest_level;

    m_impl->is_open = true;
    log(LogLevel::Debug, ("Opened texture stream: " + path + " (mip " +
        std::to_string(coarsest_level) + "/" + std::to_string(m_impl->mip_count - 1) + ")").c_str());
    return true;
}

void TextureStream::close() {
    m_impl->close();
}

bool TextureStream::is_open() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->is_open;
}

void TextureStream::request_mip(uint32_t level) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    if (!m_impl->is_open || level >= m_impl->mip_count) return;
    if (m_impl->mip_loaded[level] || m_impl->mip_loading[level]) return;

    m_impl->mip_requested[level] = true;
    m_impl->mip_loading[level] = true;

    // Capture weak_ptr to safely check if Impl is still alive in worker thread
    std::weak_ptr<Impl> weak_impl = m_impl;
    uint32_t target_level = level;

    // Create thread and store it for proper cleanup
    std::thread worker([weak_impl, target_level]() {
        // Try to lock the weak_ptr - if it fails, TextureStream was destroyed
        auto impl = weak_impl.lock();
        if (!impl) return;

        // Check if shutdown requested before doing work
        if (impl->stop_requested.load()) {
            std::lock_guard<std::mutex> lock(impl->mutex);
            impl->mip_loading[target_level] = false;
            return;
        }

        uint32_t mip_w, mip_h;
        auto mip_data = impl->generate_mip(target_level, mip_w, mip_h);

        // Re-check if still alive after potentially long operation
        if (impl->stop_requested.load()) {
            std::lock_guard<std::mutex> lock(impl->mutex);
            impl->mip_loading[target_level] = false;
            return;
        }

        if (!mip_data.empty()) {
            std::lock_guard<std::mutex> plock(impl->pending_mutex);
            impl->pending_mips.push_back({target_level, std::move(mip_data), mip_w, mip_h});
        }

        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->mip_loading[target_level] = false;
    });

    // Store thread for proper cleanup in destructor
    {
        std::lock_guard<std::mutex> tlock(m_impl->threads_mutex);
        m_impl->active_threads.push_back(std::move(worker));
    }
}

bool TextureStream::is_mip_loaded(uint32_t level) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_impl->is_open || level >= m_impl->mip_count) return false;
    return m_impl->mip_loaded[level];
}

uint32_t TextureStream::get_loaded_mip_level() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->loaded_mip_level;
}

render::TextureHandle TextureStream::get_handle() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->handle;
}

void TextureStream::update() {
    // Check for completed async mip loads
    std::vector<Impl::PendingMip> completed;
    {
        std::lock_guard<std::mutex> plock(m_impl->pending_mutex);
        completed = std::move(m_impl->pending_mips);
        m_impl->pending_mips.clear();
    }

    if (completed.empty()) return;

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    for (auto& pending : completed) {
        if (pending.level >= m_impl->mip_count) continue;

        // If this mip is finer than current, recreate texture with all loaded mips
        if (pending.level < m_impl->loaded_mip_level) {
            // Create new texture with the finer mip level
            render::TextureData tex_data;
            tex_data.width = pending.width;
            tex_data.height = pending.height;
            tex_data.format = render::TextureFormat::RGBA8;
            tex_data.pixels = std::move(pending.data);
            tex_data.mip_levels = 1;

            // Destroy old texture and create new one with higher resolution
            if (m_impl->handle.valid()) {
                m_impl->renderer->destroy_texture(m_impl->handle);
            }
            m_impl->handle = m_impl->renderer->create_texture(tex_data);

            m_impl->loaded_mip_level = pending.level;

            log(LogLevel::Debug, ("Texture stream updated to mip " +
                std::to_string(pending.level) + ": " + m_impl->path).c_str());
        }

        m_impl->mip_loaded[pending.level] = true;
    }
}

uint32_t TextureStream::width() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->width;
}

uint32_t TextureStream::height() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->height;
}

uint32_t TextureStream::mip_count() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->mip_count;
}

const std::string& TextureStream::path() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
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
