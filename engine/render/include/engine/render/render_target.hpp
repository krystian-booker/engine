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
    // View 0 is reserved for the backbuffer (bgfx default view)
    Backbuffer = 0,

    // Shadow passes (4 cascades for CSM)
    ShadowCascade0 = 1,
    ShadowCascade1 = 2,
    ShadowCascade2 = 3,
    ShadowCascade3 = 4,

    // Additional shadow views for point/spot lights
    ShadowSpot0 = 5,
    ShadowSpot1 = 6,
    ShadowSpot2 = 7,
    ShadowSpot3 = 8,

    // Point light shadow cubemap faces (6 faces, up to 4 lights)
    ShadowPoint0Face0 = 9,
    // ... faces 1-5 follow (9-14, 15-20, 21-26, 27-32)

    // Depth pre-pass (starts at 33 to avoid collision with point light shadow face views)
    DepthPrepass = 33,

    // G-Buffer pass (for deferred rendering)
    GBuffer = 34,

    // Motion vectors (for TAA)
    MotionVectors = 35,

    // Screen-space effects
    SSAO = 36,
    SSAOBlur0 = 37,
    SSAOBlur1 = 38,
    SSAOBlur2 = 39,
    SSAOBlur3 = 40,
    SSR = 41,

    // Volumetric lighting
    VolumetricScatter = 42,

    // Skybox rendering (must be before MainOpaque so it paints the background
    // before opaque geometry is drawn on top; uses DEPTH_TEST_LEQUAL without
    // writing depth, so it only fills where depth == 1.0)
    Skybox = 43,

    // Main rendering pass (forward)
    MainOpaque = 44,
    MainTransparent = 45,

    // Volumetric integration (after main passes, before post-processing)
    VolumetricIntegrate = 46,

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

    // Tone mapping (after bloom so bloom results are available)
    ToneMap = 78,
    Tonemapping = ToneMap,

    // Debug visualization
    Debug = 79,

    // UI overlay
    UI = 80,

    // Final composite to backbuffer
    Final = 81,

    // Debug overlay (on top of everything)
    DebugOverlay = 82,

    // Maximum view count
    Count = 84
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
