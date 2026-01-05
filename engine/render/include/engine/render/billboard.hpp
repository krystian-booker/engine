#pragma once

#include <engine/render/types.hpp>
#include <engine/render/render_target.hpp>
#include <engine/core/math.hpp>
#include <vector>

namespace engine::render {

using namespace engine::core;

class IRenderer;
enum class RenderView : uint16_t;
struct CameraData;

// Billboard orientation mode
enum class BillboardMode : uint8_t {
    ScreenAligned,    // Always face camera (full billboard)
    AxisAligned,      // Rotate around Y axis only (cylindrical)
    Fixed             // No automatic rotation
};

// Single billboard instance for batching
struct BillboardInstance {
    Vec3 position{0.0f};
    Vec2 size{1.0f, 1.0f};
    Vec4 color{1.0f};
    Vec2 uv_offset{0.0f};      // UV offset for sprite sheets
    Vec2 uv_scale{1.0f};       // UV scale for sprite sheets
    float rotation = 0.0f;      // Z-axis rotation in radians
};

// Billboard batch for efficient rendering
struct BillboardBatch {
    TextureHandle texture;
    std::vector<BillboardInstance> instances;
    BillboardMode mode = BillboardMode::ScreenAligned;
    bool depth_test = true;
    bool depth_write = false;
    float depth_fade_distance = 0.0f;  // Soft billboard fading (0 = disabled)
    uint8_t blend_mode = 2;  // Default to alpha blend (matches RenderObject blend modes)
};

// Billboard renderer system
class BillboardRenderer {
public:
    BillboardRenderer() = default;
    ~BillboardRenderer();

    void init(IRenderer* renderer);
    void shutdown();

    // Submit a batch of billboards for rendering
    void submit_batch(const BillboardBatch& batch);

    // Render all submitted batches
    void render(RenderView view, const CameraData& camera);

    // Clear submitted batches
    void clear();

    bool is_initialized() const { return m_initialized; }

    // Get number of billboards rendered last frame
    uint32_t get_billboard_count() const { return m_billboard_count; }

private:
    void create_quad_mesh();
    void update_instance_buffer(const BillboardBatch& batch, const CameraData& camera);

    IRenderer* m_renderer = nullptr;
    bool m_initialized = false;

    MeshHandle m_quad_mesh;
    ShaderHandle m_billboard_shader;

    std::vector<BillboardBatch> m_pending_batches;
    uint32_t m_billboard_count = 0;
};

// Global billboard renderer instance
BillboardRenderer& get_billboard_renderer();

} // namespace engine::render
