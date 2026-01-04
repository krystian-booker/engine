#pragma once

#include <engine/render/types.hpp>
#include <engine/render/render_target.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/core/math.hpp>
#include <vector>

namespace engine::render {

using namespace engine::core;

class IRenderer;

// Render-to-texture target info
struct RTTTarget {
    RenderTargetHandle target;
    TextureHandle color_texture;
    TextureHandle depth_texture;
    uint32_t width = 512;
    uint32_t height = 512;
    bool valid = false;
};

// ECS component for entities that render to a texture
// Attach this to a camera entity to make it render to a texture instead of the screen
struct RenderToTextureComponent {
    // Target configuration
    uint32_t width = 512;
    uint32_t height = 512;
    TextureFormat color_format = TextureFormat::RGBA16F;
    bool has_depth = true;

    // Which render passes to include
    RenderPassFlags passes = RenderPassFlags::AllOpaque | RenderPassFlags::Skybox;

    // Update rate (0 = every frame, 1 = every 2nd frame, etc.)
    uint8_t update_rate = 0;

    // Runtime state (managed by the RTT system)
    RTTTarget runtime_target;
    uint32_t frame_counter = 0;
    bool needs_update = true;

    // Get the rendered texture
    TextureHandle get_texture() const { return runtime_target.color_texture; }
    TextureHandle get_depth_texture() const { return runtime_target.depth_texture; }
};

// Camera render entry for multi-camera rendering
struct CameraRenderEntry {
    CameraData camera_data;
    RenderTargetHandle target;  // INVALID = render to backbuffer
    RenderPassFlags passes = RenderPassFlags::All;
    uint8_t priority = 0;       // Lower = renders first
    bool is_rtt = false;        // Is this a render-to-texture camera?
};

// Manages render-to-texture targets and multi-camera rendering
class RenderToTextureSystem {
public:
    RenderToTextureSystem() = default;
    ~RenderToTextureSystem();

    void init(IRenderer* renderer);
    void shutdown();

    // Create/destroy RTT targets
    RTTTarget create_target(uint32_t width, uint32_t height,
                            TextureFormat color_format = TextureFormat::RGBA16F,
                            bool has_depth = true);
    void destroy_target(RTTTarget& target);

    // Resize an existing target
    void resize_target(RTTTarget& target, uint32_t width, uint32_t height);

    // Register a camera for rendering this frame
    void add_camera(const CameraRenderEntry& entry);

    // Get all registered cameras (sorted by priority)
    const std::vector<CameraRenderEntry>& get_cameras() const { return m_cameras; }

    // Clear camera list for next frame
    void clear_cameras();

    // Get number of active RTT targets
    uint32_t get_active_target_count() const { return m_active_target_count; }

private:
    IRenderer* m_renderer = nullptr;
    bool m_initialized = false;

    std::vector<CameraRenderEntry> m_cameras;
    uint32_t m_active_target_count = 0;
};

// Global RTT system instance
RenderToTextureSystem& get_rtt_system();

} // namespace engine::render
