#pragma once

#include <engine/render/types.hpp>
#include <engine/render/render_target.hpp>
#include <array>
#include <memory>
#include <vector>

namespace engine::render {

// Info about mesh GPU buffers (for direct access by vegetation/particle systems)
struct MeshBufferInfo {
    uint16_t vertex_buffer;   // Native vertex buffer handle (bgfx::VertexBufferHandle::idx)
    uint16_t index_buffer;    // Native index buffer handle (bgfx::IndexBufferHandle::idx)
    uint32_t index_count;     // Number of indices
    bool valid;               // Whether the mesh exists and handles are valid
};

// Abstract renderer interface - hides BGFX implementation details
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Initialization and shutdown
    virtual bool init(void* native_window_handle, uint32_t width, uint32_t height) = 0;
    virtual void shutdown() = 0;

    // Frame management
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;

    // Resource creation
    virtual MeshHandle create_mesh(const MeshData& data) = 0;
    virtual TextureHandle create_texture(const TextureData& data) = 0;
    virtual ShaderHandle create_shader(const ShaderData& data) = 0;
    virtual MaterialHandle create_material(const MaterialData& data) = 0;

    // Primitive mesh creation
    virtual MeshHandle create_primitive(PrimitiveMesh type, float size = 1.0f) = 0;

    // Resource destruction
    virtual void destroy_mesh(MeshHandle h) = 0;
    virtual void destroy_texture(TextureHandle h) = 0;
    virtual void destroy_shader(ShaderHandle h) = 0;
    virtual void destroy_material(MaterialHandle h) = 0;

    // Render target management
    virtual RenderTargetHandle create_render_target(const RenderTargetDesc& desc) = 0;
    virtual void destroy_render_target(RenderTargetHandle h) = 0;

    // Get the texture from a render target attachment
    // attachment: 0 for first color attachment, 1+ for additional color attachments
    // Use UINT32_MAX for depth attachment
    virtual TextureHandle get_render_target_texture(RenderTargetHandle h, uint32_t attachment = 0) = 0;

    // Resize an existing render target (recreates internal textures)
    virtual void resize_render_target(RenderTargetHandle h, uint32_t width, uint32_t height) = 0;

    // View management
    virtual void configure_view(RenderView view, const ViewConfig& config) = 0;
    virtual void set_view_transform(RenderView view, const Mat4& view_matrix, const Mat4& proj_matrix) = 0;

    // Draw call management (queued for batching)
    virtual void queue_draw(const DrawCall& call) = 0;
    virtual void queue_draw(const DrawCall& call, RenderView view) = 0;

    // Camera and lighting
    virtual void set_camera(const Mat4& view, const Mat4& proj) = 0;
    virtual void set_light(uint32_t index, const LightData& light) = 0;
    virtual void clear_lights() = 0;

    // Shadow mapping
    virtual void set_shadow_data(const std::array<Mat4, 4>& cascade_matrices,
                                  const Vec4& cascade_splits,
                                  const Vec4& shadow_params) = 0;
    virtual void set_shadow_texture(uint32_t cascade, TextureHandle texture) = 0;
    virtual void enable_shadows(bool enabled) = 0;

    // Direct mesh submission for specific views
    virtual void submit_mesh(RenderView view, MeshHandle mesh, MaterialHandle material, const Mat4& transform) = 0;
    virtual void submit_skinned_mesh(RenderView view, MeshHandle mesh, MaterialHandle material,
                                      const Mat4& transform, const Mat4* bone_matrices, uint32_t bone_count) = 0;

    // Debug drawing
    virtual void flush_debug_draw(RenderView view) = 0;

    // Screen output
    virtual void blit_to_screen(RenderView view, TextureHandle source) = 0;

    // Skybox rendering
    virtual void submit_skybox(RenderView view, TextureHandle cubemap,
                                const Mat4& inverse_view_proj,
                                float intensity, float rotation) = 0;

    // Billboard rendering
    virtual void submit_billboard(RenderView view, MeshHandle quad_mesh, TextureHandle texture,
                                   const Mat4& transform, const Vec4& color,
                                   const Vec2& uv_offset, const Vec2& uv_scale,
                                   bool depth_test, bool depth_write) = 0;

    // SSAO texture
    virtual void set_ao_texture(TextureHandle texture) = 0;

    // Sorts queued draws by material/mesh and submits to GPU
    virtual void flush() = 0;

    // Clear screen
    virtual void clear(uint32_t color = 0x303030ff, float depth = 1.0f) = 0;

    // Get current viewport dimensions
    virtual uint32_t get_width() const = 0;
    virtual uint32_t get_height() const = 0;

    // VSync control
    virtual void set_vsync(bool enabled) = 0;
    virtual bool get_vsync() const = 0;

    // Quality settings
    virtual void set_render_scale(float scale) = 0;       // 0.5 to 2.0, affects internal resolution
    virtual float get_render_scale() const = 0;
    virtual void set_shadow_quality(int quality) = 0;     // 0=off, 1=low, 2=medium, 3=high, 4=ultra
    virtual int get_shadow_quality() const = 0;
    virtual void set_lod_bias(float bias) = 0;            // -2.0 to 2.0, positive = prefer lower LODs
    virtual float get_lod_bias() const = 0;

    // Post-processing toggles
    virtual void set_bloom_enabled(bool enabled) = 0;
    virtual void set_bloom_intensity(float intensity) = 0;
    virtual bool get_bloom_enabled() const = 0;
    virtual float get_bloom_intensity() const = 0;

    virtual void set_ao_enabled(bool enabled) = 0;
    virtual bool get_ao_enabled() const = 0;

    virtual void set_ibl_intensity(float intensity) = 0;
    virtual float get_ibl_intensity() const = 0;

    virtual void set_motion_blur_enabled(bool enabled) = 0;
    virtual bool get_motion_blur_enabled() const = 0;

    // Native handle access (for post-process effects that need direct GPU access)
    // Returns the native texture handle as uint16_t (bgfx::TextureHandle::idx)
    virtual uint16_t get_native_texture_handle(TextureHandle h) const = 0;

    // Get native mesh buffer handles (for instanced rendering in vegetation/particle systems)
    virtual MeshBufferInfo get_mesh_buffer_info(MeshHandle mesh) const = 0;
};

// Factory function to create BGFX renderer
std::unique_ptr<IRenderer> create_bgfx_renderer();

} // namespace engine::render
