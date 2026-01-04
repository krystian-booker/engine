#pragma once

#include <engine/render/types.hpp>
#include <engine/render/render_target.hpp>
#include <engine/core/math.hpp>

namespace engine::render {

using namespace engine::core;

class IRenderer;

// OIT configuration
struct OITConfig {
    bool enabled = true;
    float weight_power = 3.0f;      // Controls depth weighting (higher = more depth-aware)
    float max_distance = 500.0f;    // Maximum depth for weight calculation
};

// Weighted Blended Order-Independent Transparency system
// Based on: http://jcgt.org/published/0002/02/09/
class OITSystem {
public:
    OITSystem() = default;
    ~OITSystem();

    void init(IRenderer* renderer, uint32_t width, uint32_t height);
    void shutdown();

    // Configure OIT
    void set_config(const OITConfig& config) { m_config = config; }
    const OITConfig& get_config() const { return m_config; }

    // Resize OIT buffers
    void resize(uint32_t width, uint32_t height);

    // Begin transparent pass - sets up OIT render targets
    void begin_transparent_pass();

    // End transparent pass and composite to destination
    void composite(RenderTargetHandle destination);

    // Get the accumulation texture (for debugging)
    TextureHandle get_accum_texture() const;
    TextureHandle get_reveal_texture() const;

    // Get render targets for transparent rendering
    RenderTargetHandle get_accum_target() const { return m_accum_target; }
    RenderTargetHandle get_reveal_target() const { return m_reveal_target; }

    bool is_enabled() const { return m_config.enabled && m_initialized; }

private:
    void create_targets();
    void destroy_targets();

    IRenderer* m_renderer = nullptr;
    bool m_initialized = false;

    OITConfig m_config;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Accumulation buffer: RGB = premultiplied color sum, A = alpha sum (weighted)
    RenderTargetHandle m_accum_target;

    // Revealage buffer: R = product of (1 - alpha) = final transparency
    RenderTargetHandle m_reveal_target;

    // MRT framebuffer for transparent rendering
    uint16_t m_mrt_framebuffer = UINT16_MAX;

    // Resources for compositing
    uint16_t m_composite_program = UINT16_MAX;
    uint16_t m_fullscreen_vb = UINT16_MAX;
    uint16_t m_fullscreen_ib = UINT16_MAX;

    // Uniforms
    uint16_t u_accum = UINT16_MAX;
    uint16_t u_reveal = UINT16_MAX;
    uint16_t u_opaque = UINT16_MAX;
};

} // namespace engine::render
