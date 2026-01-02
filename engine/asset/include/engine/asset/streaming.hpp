#pragma once

#include <engine/render/types.hpp>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <atomic>

namespace engine::render {
class IRenderer;
}

namespace engine::asset {

// Audio streaming - decode audio on-demand instead of loading entirely into memory
class AudioStream {
public:
    AudioStream();
    ~AudioStream();

    // Non-copyable but movable
    AudioStream(const AudioStream&) = delete;
    AudioStream& operator=(const AudioStream&) = delete;
    AudioStream(AudioStream&& other) noexcept;
    AudioStream& operator=(AudioStream&& other) noexcept;

    // Open an audio file for streaming
    // Returns true on success
    bool open(const std::string& path);

    // Close the stream
    void close();

    // Check if stream is open
    bool is_open() const;

    // Read decoded PCM samples into buffer
    // Returns number of samples actually read (may be less at end of file)
    // Samples are interleaved if stereo (L, R, L, R, ...)
    size_t read(int16_t* buffer, size_t sample_count);

    // Read as float samples (normalized -1.0 to 1.0)
    size_t read_float(float* buffer, size_t sample_count);

    // Seek to a specific sample position
    bool seek(size_t sample_offset);

    // Get current position in samples
    size_t tell() const;

    // Audio properties
    uint32_t sample_rate() const;
    uint32_t channels() const;
    size_t total_samples() const;
    float duration() const;  // Duration in seconds

    // Get file path
    const std::string& path() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Texture streaming - load mipmap levels on demand
class TextureStream {
public:
    TextureStream();
    ~TextureStream();

    // Non-copyable but movable
    TextureStream(const TextureStream&) = delete;
    TextureStream& operator=(const TextureStream&) = delete;
    TextureStream(TextureStream&& other) noexcept;
    TextureStream& operator=(TextureStream&& other) noexcept;

    // Initialize the streaming texture
    // Loads only metadata and optionally a low-res mip initially
    bool open(const std::string& path, render::IRenderer* renderer);

    // Close and release resources
    void close();

    // Check if stream is open
    bool is_open() const;

    // Request a specific mip level to be loaded (0 = highest resolution)
    // This starts an async load if the mip is not already loaded
    void request_mip(uint32_t level);

    // Check if a mip level is currently loaded
    bool is_mip_loaded(uint32_t level) const;

    // Get the lowest (coarsest) mip level that's currently loaded
    uint32_t get_loaded_mip_level() const;

    // Get the texture handle (may point to lower-res mips until high-res loads)
    render::TextureHandle get_handle() const;

    // Poll for streaming updates (call periodically)
    void update();

    // Properties
    uint32_t width() const;
    uint32_t height() const;
    uint32_t mip_count() const;
    const std::string& path() const;

private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;
};

// Stream handle for tracking async streaming operations
class StreamHandle {
public:
    StreamHandle();
    ~StreamHandle();

    StreamHandle(const StreamHandle&) = delete;
    StreamHandle& operator=(const StreamHandle&) = delete;
    StreamHandle(StreamHandle&& other) noexcept;
    StreamHandle& operator=(StreamHandle&& other) noexcept;

    // Check if streaming is complete
    bool is_ready() const;

    // Get streaming progress (0.0 - 1.0)
    float progress() const;

    // Block until streaming is complete
    void wait();

    // Cancel the streaming operation
    void cancel();

    // Check if handle is valid
    bool valid() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    friend class AssetManager;
};

} // namespace engine::asset
