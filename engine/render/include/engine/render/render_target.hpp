#pragma once

#include <engine/render/types.hpp>
#include <cstdint>

namespace engine::render {

// Handle for render targets (framebuffers)
struct RenderTargetHandle {
    uint32_t id = UINT32_MAX;
    bool valid() const { return id != UINT32_MAX; }
};

// Render target description for creation
struct RenderTargetDesc {
    uint32_t width = 0;
    uint32_t height = 0;

    // Color attachment(s)
    TextureFormat color_format = TextureFormat::RGBA16F;
    uint32_t color_attachment_count = 1;

    // Depth attachment
    TextureFormat depth_format = TextureFormat::Depth32F;
    bool has_depth = true;

    // Multisampling
    uint32_t msaa_samples = 1;

    // Generate mipmaps for color attachments
    bool generate_mipmaps = false;

    // Use this render target as a texture (samplable)
    bool samplable = true;

    // Debug name
    const char* debug_name = nullptr;
};

// Predefined render views for the rendering pipeline
enum class RenderView : uint16_t {
    // Shadow passes (4 cascades for CSM)
    ShadowCascade0 = 0,
    ShadowCascade1 = 1,
    ShadowCascade2 = 2,
    ShadowCascade3 = 3,

    // Additional shadow views for point/spot lights
    ShadowSpot0 = 4,
    ShadowSpot1 = 5,
    ShadowSpot2 = 6,
    ShadowSpot3 = 7,

    // Point light shadow cubemap faces (6 faces, up to 4 lights)
    ShadowPoint0Face0 = 8,
    // ... faces 1-5 follow (8-13, 14-19, 20-25, 26-31)

    // Depth pre-pass
    DepthPrepass = 32,

    // G-Buffer pass (for deferred rendering)
    GBuffer = 33,

    // Motion vectors (for TAA)
    MotionVectors = 34,

    // Screen-space effects
    SSAO = 35,
    SSAOBlur = 36,
    SSR = 37,

    // Volumetric lighting
    VolumetricScatter = 38,

    // Skybox rendering (must be before MainOpaque so it paints the background
    // before opaque geometry is drawn on top; uses DEPTH_TEST_LEQUAL without
    // writing depth, so it only fills where depth == 1.0)
    Skybox = 39,

    // Main rendering pass (forward/forward+)
    MainOpaque = 40,
    MainTransparent = 41,

    // Volumetric integration (after main passes, before post-processing)
    VolumetricIntegrate = 44,

    // Post-processing chain
    PostProcess0 = 50,
    PostProcess1 = 51,
    PostProcess2 = 52,
    PostProcess3 = 53,
    Bloom0 = 54,
    Bloom1 = 55,
    Bloom2 = 56,
    Bloom3 = 57,
    Bloom4 = 58,

    // TAA
    TAA = 59,
    TAAResolve = 60,

    // Bloom downsample/upsample passes
    BloomDownsample0 = 70,
    BloomDownsample1 = 71,
    BloomDownsample2 = 72,
    BloomDownsample3 = 73,
    BloomUpsample0 = 74,
    BloomUpsample1 = 75,
    BloomUpsample2 = 76,
    BloomUpsample3 = 77,

    // Tone mapping / final output
    ToneMap = 61,
    Tonemapping = ToneMap,

    // Debug visualization
    Debug = 62,
    DebugOverlay = 65,

    // UI overlay
    UI = 63,

    // Final composite to backbuffer
    Final = 64,

    // Maximum view count
    Count = 80
};

// View configuration for a render pass
struct ViewConfig {
    RenderTargetHandle render_target;  // INVALID for backbuffer
    uint32_t clear_color = 0x000000ff;
    float clear_depth = 1.0f;
    uint8_t clear_stencil = 0;
    bool clear_color_enabled = true;
    bool clear_depth_enabled = true;
    bool clear_stencil_enabled = false;

    // Viewport (default: full render target size)
    uint16_t viewport_x = 0;
    uint16_t viewport_y = 0;
    uint16_t viewport_width = 0;   // 0 = use render target width
    uint16_t viewport_height = 0;  // 0 = use render target height
};

} // namespace engine::render
