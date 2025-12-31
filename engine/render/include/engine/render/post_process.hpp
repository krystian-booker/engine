#pragma once

#include <engine/render/types.hpp>
#include <engine/render/render_target.hpp>
#include <engine/core/math.hpp>
#include <array>
#include <vector>

namespace engine::render {

using namespace engine::core;

class IRenderer;

// Bloom configuration
struct BloomConfig {
    bool enabled = true;
    float threshold = 1.0f;      // Brightness threshold for bloom extraction
    float intensity = 0.5f;      // Bloom intensity
    float scatter = 0.7f;        // How much bloom spreads (0-1)
    int mip_count = 5;           // Number of blur mip levels (default: 5)
};

// Tone mapping operators
enum class ToneMappingOperator {
    None,               // No tone mapping (linear)
    Reinhard,           // Simple Reinhard
    ReinhardExtended,   // Extended Reinhard with white point
    ACES,               // Academy Color Encoding System (filmic)
    Uncharted2,         // Filmic tone mapping from Uncharted 2
    AgX                 // Modern AgX tonemapper
};

// Tone mapping configuration
struct ToneMappingConfig {
    ToneMappingOperator op = ToneMappingOperator::ACES;
    float exposure = 1.0f;       // Exposure adjustment
    float gamma = 2.2f;          // Gamma correction value
    float white_point = 4.0f;    // For extended Reinhard
    bool auto_exposure = false;  // Enable auto-exposure
    float adaptation_speed = 1.0f;  // Auto-exposure adaptation speed
};

// Combined post-processing configuration
struct PostProcessConfig {
    BloomConfig bloom;
    ToneMappingConfig tonemapping;
    bool vignette_enabled = false;
    float vignette_intensity = 0.5f;
    float vignette_smoothness = 0.5f;
    bool chromatic_aberration = false;
    float ca_intensity = 0.01f;
};

// Post-processing system
// Handles bloom, tone mapping, and final composite
class PostProcessSystem {
public:
    PostProcessSystem() = default;
    ~PostProcessSystem();

    // Initialize post-processing
    void init(IRenderer* renderer, const PostProcessConfig& config);
    void shutdown();

    // Update configuration
    void set_config(const PostProcessConfig& config);
    const PostProcessConfig& get_config() const { return m_config; }

    // Process HDR scene to LDR output
    // Input: HDR scene texture
    // Output: Final LDR image in the target (or backbuffer)
    void process(
        TextureHandle hdr_scene,
        RenderTargetHandle output_target = RenderTargetHandle{}  // Empty = backbuffer
    );

    // Resize post-processing targets
    void resize(uint32_t width, uint32_t height);

    // Get intermediate textures (for debug visualization)
    TextureHandle get_bloom_texture() const;
    float get_current_exposure() const { return m_current_exposure; }

private:
    void create_bloom_chain();
    void destroy_bloom_chain();

    void render_bloom_downsample(TextureHandle input, int mip);
    void render_bloom_upsample(int mip);
    void render_tonemapping(TextureHandle scene, TextureHandle bloom);

    IRenderer* m_renderer = nullptr;
    PostProcessConfig m_config;
    bool m_initialized = false;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Bloom mip chain
    static const int MAX_BLOOM_MIPS = 8;
    std::array<RenderTargetHandle, MAX_BLOOM_MIPS> m_bloom_downsample;
    std::array<RenderTargetHandle, MAX_BLOOM_MIPS> m_bloom_upsample;
    int m_bloom_mip_count = 0;

    // Auto-exposure
    float m_current_exposure = 1.0f;
    RenderTargetHandle m_luminance_target;
    RenderTargetHandle m_avg_luminance;
};

// TAA (Temporal Anti-Aliasing) system
struct TAAConfig {
    bool enabled = true;
    float jitter_scale = 1.0f;    // Jitter intensity
    float feedback_min = 0.88f;   // Minimum history blend
    float feedback_max = 0.97f;   // Maximum history blend
    bool sharpen = true;          // Apply sharpening after TAA
    float sharpen_amount = 0.25f;
};

class TAASystem {
public:
    TAASystem() = default;
    ~TAASystem();

    void init(IRenderer* renderer, const TAAConfig& config);
    void shutdown();

    void set_config(const TAAConfig& config);
    const TAAConfig& get_config() const { return m_config; }

    // Get the jitter offset for this frame (call before rendering)
    Vec2 get_jitter(uint32_t frame_index) const;

    // Apply TAA to the current frame
    // Returns the resolved texture
    TextureHandle resolve(
        TextureHandle current_frame,
        TextureHandle depth_texture,
        TextureHandle motion_vectors
    );

    void resize(uint32_t width, uint32_t height);

private:
    void create_history_buffers();
    void destroy_history_buffers();

    // Halton sequence for jitter
    static float halton(int index, int base);

    IRenderer* m_renderer = nullptr;
    TAAConfig m_config;
    bool m_initialized = false;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_frame_count = 0;

    // Ping-pong history buffers
    RenderTargetHandle m_history[2];
    int m_history_index = 0;

    // Jitter sequence (8 samples Halton)
    static const int JITTER_SAMPLES = 8;
    std::array<Vec2, JITTER_SAMPLES> m_jitter_sequence;
};

} // namespace engine::render
