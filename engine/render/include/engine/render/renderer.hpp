#pragma once

#include <engine/render/types.hpp>
#include <memory>
#include <vector>

namespace engine::render {

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

    // Draw call management (queued for batching)
    virtual void queue_draw(const DrawCall& call) = 0;

    // Camera and lighting
    virtual void set_camera(const Mat4& view, const Mat4& proj) = 0;
    virtual void set_light(uint32_t index, const LightData& light) = 0;
    virtual void clear_lights() = 0;

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
};

// Factory function to create BGFX renderer
std::unique_ptr<IRenderer> create_bgfx_renderer();

} // namespace engine::render
