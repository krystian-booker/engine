#include <engine/render/water_renderer.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <cmath>
#include <vector>

namespace engine::render {

WaterRenderer::WaterRenderer() = default;
WaterRenderer::~WaterRenderer() = default;

WaterRenderer& WaterRenderer::instance() {
    static WaterRenderer s_instance;
    return s_instance;
}

bool WaterRenderer::init() {
    if (m_initialized) {
        return true;
    }

    core::log(core::LogLevel::Info, "WaterRenderer: Initializing");

    load_default_textures();
    create_shaders();

    m_initialized = true;
    return true;
}

void WaterRenderer::shutdown() {
    if (!m_initialized) {
        return;
    }

    core::log(core::LogLevel::Info, "WaterRenderer: Shutting down");

    // Release resources
    m_default_normal_map = TextureHandle{};
    m_default_foam_texture = TextureHandle{};
    m_default_caustics_texture = TextureHandle{};
    m_water_shader = ShaderHandle{};
    m_underwater_shader = ShaderHandle{};

    m_initialized = false;
}

void WaterRenderer::load_default_textures() {
    // In a full implementation, these would load actual texture files
    // For now, we create placeholder handles that the asset system would fill

    core::log(core::LogLevel::Debug, "WaterRenderer: Loading default textures");

    // Default textures would be loaded from:
    // - engine/render/textures/water_normal.dds
    // - engine/render/textures/water_foam.dds
    // - engine/render/textures/water_caustics.dds
}

void WaterRenderer::create_shaders() {
    // In a full implementation, these would load the compiled shaders
    // The shaders are defined in vs_water.sc and fs_water.sc

    core::log(core::LogLevel::Debug, "WaterRenderer: Creating shaders");
}

void WaterRenderer::begin_frame(float dt) {
    m_water_time += dt;

    // Wrap time to prevent floating point precision issues
    if (m_water_time > 10000.0f) {
        m_water_time = std::fmod(m_water_time, 10000.0f);
    }
}

void WaterRenderer::end_frame() {
    // Cleanup per-frame state if needed
}

void WaterRenderer::render_water_surfaces(const Mat4& view_matrix) {
    if (!m_initialized) {
        return;
    }

    (void)view_matrix;

    // In a full implementation:
    // 1. Iterate all entities with WaterSurfaceComponent
    // 2. For each water surface:
    //    a. Update reflection/refraction textures if needed
    //    b. Build shader uniforms from settings
    //    c. Bind textures (normal maps, foam, reflection, etc.)
    //    d. Submit draw call with water mesh
}

void WaterRenderer::begin_reflection_pass(const WaterSurfaceComponent& water, const Mat4& view_matrix) {
    if (m_rendering_reflection) {
        core::log(core::LogLevel::Warn, "WaterRenderer: Already rendering reflection");
        return;
    }

    m_rendering_reflection = true;

    // Calculate reflection matrix
    // Reflect the view matrix across the water plane (Y = surface height)
    float surface_y = 0.0f;  // Would get from water entity's world transform

    // Reflection matrix for Y plane
    Mat4 reflection_mat = Mat4(1.0f);
    reflection_mat[1][1] = -1.0f;
    reflection_mat[3][1] = 2.0f * surface_y;

    m_reflection_view = reflection_mat * view_matrix;

    // Clip plane for reflection rendering (cull below water)
    m_clip_plane = Vec4(0.0f, 1.0f, 0.0f, -surface_y + water.settings.reflection_clip_offset);

    core::log(core::LogLevel::Debug, "WaterRenderer: Begin reflection pass");
}

void WaterRenderer::end_reflection_pass() {
    m_rendering_reflection = false;
    core::log(core::LogLevel::Debug, "WaterRenderer: End reflection pass");
}

void WaterRenderer::update_underwater_state(const Vec3& camera_pos) {
    // Check if camera is below any water surface
    // This would iterate water volumes and check containment

    m_camera_underwater = camera_pos.y < m_current_surface_height;
}

MeshHandle WaterRenderer::create_water_grid_mesh(int resolution, float size) {
    // Generate a grid mesh for water surface rendering
    // This creates a flat grid that will be displaced by vertex shader

    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    float half_size = size * 0.5f;
    float step = size / static_cast<float>(resolution - 1);

    // Generate vertices
    for (int z = 0; z < resolution; ++z) {
        for (int x = 0; x < resolution; ++x) {
            Vertex v;
            v.position = Vec3{
                -half_size + x * step,
                0.0f,
                -half_size + z * step
            };

            // UV coordinates (0-1 range)
            float u = static_cast<float>(x) / static_cast<float>(resolution - 1);
            float v_coord = static_cast<float>(z) / static_cast<float>(resolution - 1);
            v.texcoord = Vec2{u, v_coord};

            // Normal pointing up
            v.normal = Vec3{0.0f, 1.0f, 0.0f};

            // Tangent for normal mapping
            v.tangent = Vec4{1.0f, 0.0f, 0.0f, 1.0f};

            vertices.push_back(v);
        }
    }

    // Generate indices (two triangles per grid cell)
    for (int z = 0; z < resolution - 1; ++z) {
        for (int x = 0; x < resolution - 1; ++x) {
            uint16_t top_left = static_cast<uint16_t>(z * resolution + x);
            uint16_t top_right = top_left + 1;
            uint16_t bottom_left = static_cast<uint16_t>((z + 1) * resolution + x);
            uint16_t bottom_right = bottom_left + 1;

            // First triangle
            indices.push_back(top_left);
            indices.push_back(bottom_left);
            indices.push_back(top_right);

            // Second triangle
            indices.push_back(top_right);
            indices.push_back(bottom_left);
            indices.push_back(bottom_right);
        }
    }

    core::log(core::LogLevel::Debug, "WaterRenderer: Created water grid {}x{} ({} verts, {} indices)",
              resolution, resolution, vertices.size(), indices.size());

    // In a full implementation, this would upload to GPU via renderer
    // For now, return an empty handle that would be filled by the mesh system
    return MeshHandle{};
}

void WaterRenderer::set_global_quality(WaterQuality quality) {
    m_global_quality = quality;

    core::log(core::LogLevel::Info, "WaterRenderer: Quality set to {}",
              quality == WaterQuality::Low ? "Low" :
              quality == WaterQuality::Medium ? "Medium" :
              quality == WaterQuality::High ? "High" : "Ultra");
}

void WaterRenderer::update_reflection_texture(WaterSurfaceComponent& water) {
    // Update or create reflection render target based on quality settings

    int target_res = water.settings.reflection_resolution;

    // Adjust based on global quality
    switch (m_global_quality) {
        case WaterQuality::Low:
            target_res = std::min(target_res, 256);
            break;
        case WaterQuality::Medium:
            target_res = std::min(target_res, 512);
            break;
        case WaterQuality::High:
            target_res = std::min(target_res, 1024);
            break;
        case WaterQuality::Ultra:
            // Use requested resolution
            break;
    }

    (void)target_res;
    // Would create/update texture here
}

} // namespace engine::render
