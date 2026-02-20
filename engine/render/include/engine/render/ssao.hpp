#pragma once

#include <engine/render/render_target.hpp>
#include <engine/render/types.hpp>
#include <engine/core/math.hpp>
#include <array>
#include <vector>

namespace engine::render {

using namespace engine::core;

class IRenderer;

// SSAO configuration options
struct SSAOConfig {
    uint32_t sample_count = 32;       // Number of hemisphere samples
    float radius = 0.5f;              // Sample radius in world units
    float bias = 0.025f;              // Depth bias to prevent self-occlusion
    float intensity = 1.5f;           // AO intensity multiplier
    float power = 2.0f;               // Power curve for AO
    bool half_resolution = true;      // Render at half res for performance
    bool blur_enabled = true;         // Enable bilateral blur
    int blur_passes = 2;              // Number of blur passes
};

// SSAO render pass data
struct SSAOPassData {
    RenderTargetHandle ao_target;           // Raw AO output
    RenderTargetHandle blur_target;         // Blurred AO output
    TextureHandle noise_texture;            // Random rotation vectors
    std::vector<Vec4> kernel_samples;       // Hemisphere sample kernel
};

// Screen-Space Ambient Occlusion system
// Implements HBAO-style horizon-based ambient occlusion
class SSAOSystem {
public:
    SSAOSystem() = default;
    ~SSAOSystem();

    // Initialize the SSAO system
    void init(IRenderer* renderer, const SSAOConfig& config);
    void shutdown();

    // Update configuration
    void set_config(const SSAOConfig& config);
    const SSAOConfig& get_config() const { return m_config; }

    // Generate SSAO texture from depth/normal buffers
    // Returns the AO texture that can be applied during lighting
    TextureHandle render(
        TextureHandle depth_texture,
        TextureHandle normal_texture,
        const Mat4& projection,
        const Mat4& view
    );

    // Get the final AO texture
    TextureHandle get_ao_texture() const;

    // Resize SSAO render targets (call on window resize)
    void resize(uint32_t width, uint32_t height);

    RenderView get_ssao_view() const { return RenderView::SSAO; }
    RenderView get_ssao_blur_view() const { return RenderView::SSAOBlur0; }

private:
    void create_render_targets();
    void destroy_render_targets();
    void generate_kernel();
    void create_noise_texture();

    IRenderer* m_renderer = nullptr;
    SSAOConfig m_config;
    bool m_initialized = false;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Render resources
    RenderTargetHandle m_ao_target;
    RenderTargetHandle m_blur_temp_target;
    TextureHandle m_noise_texture;

    // Sample kernel (generated at init)
    std::array<Vec4, 64> m_kernel;
};

// GTAO (Ground Truth Ambient Occlusion) - Higher quality alternative
// Can be used instead of HBAO for better quality at similar cost
namespace gtao {

// Generate the spatial-temporal noise texture
void generate_hilbert_noise(std::vector<uint8_t>& pixels, uint32_t size);

// Compute visibility cone from horizon angles
float integrate_arc(float h1, float h2, float n);

} // namespace gtao

} // namespace engine::render
