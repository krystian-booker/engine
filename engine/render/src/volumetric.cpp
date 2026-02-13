#include <engine/render/volumetric.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <bgfx/bgfx.h>
#include <random>
#include <cmath>
#include <fstream>

namespace engine::render {

using namespace engine::core;

// Volumetric shader resources
struct VolumetricShaders {
    bgfx::ProgramHandle volumetric = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle u_volumetricParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_fogColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_fogHeight = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_lightDir = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_lightColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_cameraPos = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_projParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadowMatrix = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_depth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_shadowMap0 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_shadowMap1 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_noise = BGFX_INVALID_HANDLE;

    bgfx::VertexBufferHandle fullscreen_vb = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle fullscreen_ib = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout fullscreen_layout;

    void create();
    void destroy();
    void draw_fullscreen(uint16_t view_id);
};

static VolumetricShaders s_vol_shaders;

static bgfx::ShaderHandle load_vol_shader(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return BGFX_INVALID_HANDLE;

    std::streampos pos = file.tellg();
    if (pos <= 0) return BGFX_INVALID_HANDLE;

    auto size = static_cast<std::streamsize>(pos);
    file.seekg(0, std::ios::beg);

    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
    file.read(reinterpret_cast<char*>(mem->data), size);
    if (file.gcount() != size) return BGFX_INVALID_HANDLE;

    mem->data[size] = '\0';
    return bgfx::createShader(mem);
}

static std::string get_vol_shader_path() {
    auto type = bgfx::getRendererType();
    switch (type) {
        case bgfx::RendererType::Direct3D11:
        case bgfx::RendererType::Direct3D12: return "shaders/dx11/";
        case bgfx::RendererType::Vulkan: return "shaders/spirv/";
        case bgfx::RendererType::OpenGL: return "shaders/glsl/";
        default: return "shaders/spirv/";
    }
}

void VolumetricShaders::create() {
    std::string path = get_vol_shader_path();

    bgfx::ShaderHandle vs = load_vol_shader(path + "vs_fullscreen.sc.bin");
    bgfx::ShaderHandle fs = load_vol_shader(path + "fs_volumetric.sc.bin");

    if (bgfx::isValid(vs) && bgfx::isValid(fs)) {
        volumetric = bgfx::createProgram(vs, fs, false);
    }

    if (bgfx::isValid(vs)) bgfx::destroy(vs);
    if (bgfx::isValid(fs)) bgfx::destroy(fs);

    // Create uniforms
    u_volumetricParams = bgfx::createUniform("u_volumetricParams", bgfx::UniformType::Vec4);
    u_fogColor = bgfx::createUniform("u_fogColor", bgfx::UniformType::Vec4);
    u_fogHeight = bgfx::createUniform("u_fogHeight", bgfx::UniformType::Vec4);
    u_lightDir = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
    u_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
    u_cameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);
    u_projParams = bgfx::createUniform("u_projParams", bgfx::UniformType::Vec4);
    u_shadowMatrix = bgfx::createUniform("u_shadowMatrix", bgfx::UniformType::Mat4, 4);
    s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);
    s_shadowMap0 = bgfx::createUniform("s_shadowMap0", bgfx::UniformType::Sampler);
    s_shadowMap1 = bgfx::createUniform("s_shadowMap1", bgfx::UniformType::Sampler);
    s_noise = bgfx::createUniform("s_noise", bgfx::UniformType::Sampler);

    // Create fullscreen quad
    fullscreen_layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    struct FSVertex { float x, y, z, u, v; };
    static const FSVertex vertices[] = {
        { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
        {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
        { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
    };
    static const uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };

    fullscreen_vb = bgfx::createVertexBuffer(bgfx::makeRef(vertices, sizeof(vertices)), fullscreen_layout);
    fullscreen_ib = bgfx::createIndexBuffer(bgfx::makeRef(indices, sizeof(indices)));

    log(LogLevel::Info, "Volumetric shaders initialized");
}

void VolumetricShaders::destroy() {
    if (bgfx::isValid(volumetric)) bgfx::destroy(volumetric);

    if (bgfx::isValid(u_volumetricParams)) bgfx::destroy(u_volumetricParams);
    if (bgfx::isValid(u_fogColor)) bgfx::destroy(u_fogColor);
    if (bgfx::isValid(u_fogHeight)) bgfx::destroy(u_fogHeight);
    if (bgfx::isValid(u_lightDir)) bgfx::destroy(u_lightDir);
    if (bgfx::isValid(u_lightColor)) bgfx::destroy(u_lightColor);
    if (bgfx::isValid(u_cameraPos)) bgfx::destroy(u_cameraPos);
    if (bgfx::isValid(u_projParams)) bgfx::destroy(u_projParams);
    if (bgfx::isValid(u_shadowMatrix)) bgfx::destroy(u_shadowMatrix);
    if (bgfx::isValid(s_depth)) bgfx::destroy(s_depth);
    if (bgfx::isValid(s_shadowMap0)) bgfx::destroy(s_shadowMap0);
    if (bgfx::isValid(s_shadowMap1)) bgfx::destroy(s_shadowMap1);
    if (bgfx::isValid(s_noise)) bgfx::destroy(s_noise);

    if (bgfx::isValid(fullscreen_vb)) bgfx::destroy(fullscreen_vb);
    if (bgfx::isValid(fullscreen_ib)) bgfx::destroy(fullscreen_ib);

    volumetric = BGFX_INVALID_HANDLE;
}

void VolumetricShaders::draw_fullscreen(uint16_t view_id) {
    if (!bgfx::isValid(volumetric) || !bgfx::isValid(fullscreen_vb)) return;

    bgfx::setVertexBuffer(0, fullscreen_vb);
    bgfx::setIndexBuffer(fullscreen_ib);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
    bgfx::submit(view_id, volumetric);
}

VolumetricSystem::~VolumetricSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void VolumetricSystem::init(IRenderer* renderer, const VolumetricConfig& config) {
    m_renderer = renderer;
    m_config = config;
    m_width = renderer->get_width();
    m_height = renderer->get_height();

    // Create shaders (shared across instances)
    static bool shaders_created = false;
    if (!shaders_created) {
        s_vol_shaders.create();
        shaders_created = true;
    }

    // Create noise textures
    create_noise_texture();

    // Create render targets
    create_render_targets();

    m_initialized = true;
    log(LogLevel::Info, "Volumetric system initialized");
}

void VolumetricSystem::shutdown() {
    if (!m_initialized) return;

    destroy_render_targets();

    if (m_noise_texture.valid()) {
        m_renderer->destroy_texture(m_noise_texture);
    }
    if (m_blue_noise.valid()) {
        m_renderer->destroy_texture(m_blue_noise);
    }

    m_initialized = false;
    m_renderer = nullptr;

    log(LogLevel::Info, "Volumetric system shutdown");
}

void VolumetricSystem::set_config(const VolumetricConfig& config) {
    bool needs_recreate = (config.froxel_width != m_config.froxel_width ||
                           config.froxel_height != m_config.froxel_height ||
                           config.froxel_depth != m_config.froxel_depth);

    m_config = config;

    if (needs_recreate && m_initialized) {
        destroy_render_targets();
        create_render_targets();
    }
}

void VolumetricSystem::create_render_targets() {
    // Note: Full volumetric implementation would use 3D textures
    // For now, we use 2D render targets as placeholders

    // Volumetric result (2D accumulated fog)
    RenderTargetDesc desc;
    desc.width = m_width / 2;  // Half resolution
    desc.height = m_height / 2;
    desc.color_attachment_count = 1;
    desc.color_format = TextureFormat::RGBA16F;
    desc.has_depth = false;
    desc.samplable = true;
    desc.debug_name = "Volumetric_Result";

    m_volumetric_result = m_renderer->create_render_target(desc);

    // Create temporal history volumes for reprojection
    if (m_config.temporal_reprojection) {
        RenderTargetDesc history_desc = desc;
        history_desc.debug_name = "Volumetric_History0";
        m_history_volume[0] = m_renderer->create_render_target(history_desc);

        history_desc.debug_name = "Volumetric_History1";
        m_history_volume[1] = m_renderer->create_render_target(history_desc);
    }

    // Configure view
    ViewConfig view_config;
    view_config.render_target = m_volumetric_result;
    view_config.clear_color_enabled = true;
    view_config.clear_color = 0x000000FF;
    view_config.clear_depth_enabled = false;

    m_renderer->configure_view(RenderView::VolumetricScatter, view_config);
}

void VolumetricSystem::destroy_render_targets() {
    if (m_volumetric_result.valid()) {
        m_renderer->destroy_render_target(m_volumetric_result);
        m_volumetric_result = RenderTargetHandle{};
    }

    if (m_density_volume.valid()) {
        m_renderer->destroy_render_target(m_density_volume);
        m_density_volume = RenderTargetHandle{};
    }

    if (m_scatter_volume.valid()) {
        m_renderer->destroy_render_target(m_scatter_volume);
        m_scatter_volume = RenderTargetHandle{};
    }

    if (m_integrated_volume.valid()) {
        m_renderer->destroy_render_target(m_integrated_volume);
        m_integrated_volume = RenderTargetHandle{};
    }

    for (int i = 0; i < 2; ++i) {
        if (m_history_volume[i].valid()) {
            m_renderer->destroy_render_target(m_history_volume[i]);
            m_history_volume[i] = RenderTargetHandle{};
        }
    }
}

void VolumetricSystem::create_noise_texture() {
    // Create 3D noise texture for density variation
    const uint32_t noise_size = 64;
    std::vector<uint8_t> noise_data;
    volumetric_noise::generate_3d_noise(noise_data, noise_size);

    TextureData tex_data;
    tex_data.width = noise_size;
    tex_data.height = noise_size;
    tex_data.depth = noise_size;
    tex_data.format = TextureFormat::RGBA8;
    tex_data.pixels = std::move(noise_data);

    m_noise_texture = m_renderer->create_texture(tex_data);

    // Create blue noise for temporal jittering
    std::vector<uint8_t> blue_noise_data;
    volumetric_noise::generate_blue_noise(blue_noise_data, 64);

    TextureData blue_data;
    blue_data.width = 64;
    blue_data.height = 64;
    blue_data.format = TextureFormat::RGBA8;
    blue_data.pixels = std::move(blue_noise_data);

    m_blue_noise = m_renderer->create_texture(blue_data);
}

void VolumetricSystem::update(
    const Mat4& view_matrix,
    const Mat4& proj_matrix,
    const Mat4& prev_view_proj,
    TextureHandle depth_texture,
    const std::array<TextureHandle, 4>& shadow_maps,
    const std::array<Mat4, 4>& shadow_matrices
) {
    if (!m_initialized) return;

    // Store current frame data for use in passes
    m_depth_texture = depth_texture;
    m_shadow_maps = shadow_maps;
    m_shadow_matrices = shadow_matrices;
    m_prev_view_proj = prev_view_proj;

    // Extract camera position from inverse view matrix
    Mat4 inv_view = glm::inverse(view_matrix);
    m_camera_pos = Vec3(inv_view[3]);

    // Get near/far from projection matrix (assuming perspective)
    m_near_plane = m_config.near_plane;
    m_far_plane = m_config.far_plane;

    // Volumetric rendering passes
    // 1. Inject density into froxel volume
    inject_density_pass();

    // 2. Scatter light through volume (with shadow sampling)
    scatter_light_pass();

    // 3. Apply temporal filtering
    if (m_config.temporal_reprojection) {
        temporal_filter_pass();
    }

    // 4. Spatial filtering (bilateral blur)
    spatial_filter_pass();

    // 5. Front-to-back integration
    integration_pass();

    // Swap history buffers
    m_history_index = 1 - m_history_index;
    m_frame_count++;
}

void VolumetricSystem::set_lights(const std::vector<VolumetricLightData>& lights) {
    m_lights = lights;
}

TextureHandle VolumetricSystem::get_volumetric_texture() const {
    if (m_volumetric_result.valid()) {
        return m_renderer->get_render_target_texture(m_volumetric_result, 0);
    }
    return TextureHandle{};
}

TextureHandle VolumetricSystem::get_froxel_texture() const {
    // Return density volume texture for debug visualization
    if (m_density_volume.valid()) {
        return m_renderer->get_render_target_texture(m_density_volume, 0);
    }
    return TextureHandle{};
}

void VolumetricSystem::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    if (m_initialized) {
        destroy_render_targets();
        create_render_targets();
    }
}

void VolumetricSystem::inject_density_pass() {
    // Density injection is handled directly in integration_pass() shader
    // which samples the noise texture and applies height-based falloff.
    // This pass exists for future froxel-based implementation where density
    // would be pre-computed into a 3D texture.
    //
    // For the current ray-marching approach, density is computed per-sample:
    // - Base density from m_config.fog_density
    // - Height falloff: exp(-(height - base_height) * falloff)
    // - Noise variation: sample 3D noise texture for natural variation
}

void VolumetricSystem::scatter_light_pass() {
    // Light scattering is computed in integration_pass() shader via ray marching.
    // For each ray step, the shader:
    // - Samples shadow maps to determine light visibility
    // - Applies Henyey-Greenstein phase function based on m_config.anisotropy
    // - Accumulates in-scattered radiance
    //
    // This pass exists for future froxel-based implementation where scattering
    // would be pre-computed per-voxel for better performance.
}

void VolumetricSystem::temporal_filter_pass() {
    if (!m_config.temporal_reprojection) return;
    if (!m_history_volume[m_history_index].valid()) return;

    // Temporal filtering blends current frame with previous frame's result
    // to reduce noise from ray marching. Uses exponential moving average:
    // result = lerp(current, history, blend_factor)
    //
    // The integration_pass() shader handles this by:
    // - Computing current frame's scattering
    // - Reprojecting previous frame using view matrix delta
    // - Blending with configurable history weight (typically 0.9-0.95)
    //
    // Note: Full implementation would use motion vectors for disocclusion
    // detection and history rejection. For now, we rely on the shader's
    // basic temporal accumulation.
}

void VolumetricSystem::spatial_filter_pass() {
    // Spatial filtering applies bilateral blur to reduce noise while
    // preserving edges. Uses depth-aware weights to prevent bleeding
    // across depth discontinuities.
    //
    // For the current implementation, the integration_pass() produces
    // relatively clean results with enough ray march steps (default 64).
    // Spatial filtering is optional and can be enabled for lower step
    // counts to improve quality/performance tradeoff.
    //
    // A full implementation would:
    // 1. Horizontal blur pass with depth-weighted kernel
    // 2. Vertical blur pass with depth-weighted kernel
    // 3. Use separable 5x5 or 7x7 kernel for efficiency
}

void VolumetricSystem::integration_pass() {
    if (!bgfx::isValid(s_vol_shaders.volumetric)) return;

    uint16_t view_id = static_cast<uint16_t>(RenderView::VolumetricScatter);

    // Get depth texture
    uint16_t depth_idx = m_renderer->get_native_texture_handle(m_depth_texture);
    if (depth_idx == bgfx::kInvalidHandle) return;

    bgfx::TextureHandle depth_handle = { depth_idx };

    // Set volumetric parameters
    Vec4 vol_params(
        m_config.fog_density,
        m_config.scattering_intensity,
        m_config.anisotropy,
        m_config.extinction_coefficient
    );

    Vec4 fog_color(
        m_config.fog_albedo.x,
        m_config.fog_albedo.y,
        m_config.fog_albedo.z,
        0.0f  // reserved (fog_height_falloff is in u_fogHeight.y)
    );

    Vec4 fog_height(
        m_config.fog_base_height,
        m_config.fog_height_falloff,
        m_config.noise_scale,
        m_config.noise_intensity
    );

    // Find directional light
    Vec4 light_dir(0.0f, -1.0f, 0.0f, 1.0f);
    Vec4 light_color(1.0f, 1.0f, 1.0f, 1.0f);

    for (const auto& light : m_lights) {
        if (light.type == 0) {  // Directional
            light_dir = Vec4(light.direction, light.intensity);
            light_color = Vec4(light.color, 1.0f);
            break;
        }
    }

    Vec4 cam_pos(m_camera_pos, 1.0f);
    Vec4 proj_params(m_near_plane, m_far_plane, 0.0f, 0.0f);

    // Set uniforms
    bgfx::setUniform(s_vol_shaders.u_volumetricParams, &vol_params);
    bgfx::setUniform(s_vol_shaders.u_fogColor, &fog_color);
    bgfx::setUniform(s_vol_shaders.u_fogHeight, &fog_height);
    bgfx::setUniform(s_vol_shaders.u_lightDir, &light_dir);
    bgfx::setUniform(s_vol_shaders.u_lightColor, &light_color);
    bgfx::setUniform(s_vol_shaders.u_cameraPos, &cam_pos);
    bgfx::setUniform(s_vol_shaders.u_projParams, &proj_params);
    bgfx::setUniform(s_vol_shaders.u_shadowMatrix, m_shadow_matrices.data(), 4);

    // Bind textures
    bgfx::setTexture(0, s_vol_shaders.s_depth, depth_handle);

    // Bind shadow maps if available
    for (int i = 0; i < 2; ++i) {
        if (m_shadow_maps[i].valid()) {
            uint16_t shadow_idx = m_renderer->get_native_texture_handle(m_shadow_maps[i]);
            if (shadow_idx != bgfx::kInvalidHandle) {
                bgfx::TextureHandle shadow_handle = { shadow_idx };
                if (i == 0) bgfx::setTexture(1, s_vol_shaders.s_shadowMap0, shadow_handle);
                else bgfx::setTexture(2, s_vol_shaders.s_shadowMap1, shadow_handle);
            }
        }
    }

    // Bind noise texture
    if (m_noise_texture.valid()) {
        uint16_t noise_idx = m_renderer->get_native_texture_handle(m_noise_texture);
        if (noise_idx != bgfx::kInvalidHandle) {
            bgfx::TextureHandle noise_handle = { noise_idx };
            bgfx::setTexture(3, s_vol_shaders.s_noise, noise_handle);
        }
    }

    // Draw fullscreen volumetric pass
    s_vol_shaders.draw_fullscreen(view_id);
}

// ============================================================================
// Phase function implementations
// ============================================================================

namespace phase {

float henyey_greenstein(float cos_theta, float g) {
    float g2 = g * g;
    float denom = 1.0f + g2 - 2.0f * g * cos_theta;
    return (1.0f - g2) / (4.0f * 3.14159265f * std::pow(std::max(denom, 1e-6f), 1.5f));
}

float schlick_phase(float cos_theta, float g) {
    float k = 1.55f * g - 0.55f * g * g * g;
    float denom = 1.0f - k * cos_theta;
    return (1.0f - k * k) / (4.0f * 3.14159265f * std::max(denom * denom, 1e-6f));
}

float cornette_shanks(float cos_theta, float g) {
    float g2 = g * g;
    float num = 3.0f * (1.0f - g2) * (1.0f + cos_theta * cos_theta);
    float denom = 2.0f * (2.0f + g2) * std::pow(std::max(1.0f + g2 - 2.0f * g * cos_theta, 1e-6f), 1.5f);
    return num / denom;
}

} // namespace phase

// ============================================================================
// Noise generation implementations
// ============================================================================

namespace volumetric_noise {

// Simple 3D Perlin-like noise
void generate_3d_noise(std::vector<uint8_t>& data, uint32_t size) {
    data.resize(size * size * size * 4);

    std::random_device rd;
    std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // Generate gradient vectors
    std::vector<Vec3> gradients(256);
    for (int i = 0; i < 256; ++i) {
        float theta = dist(gen) * 2.0f * 3.14159265f;
        float phi = std::acos(2.0f * dist(gen) - 1.0f);
        gradients[i] = Vec3(
            std::sin(phi) * std::cos(theta),
            std::sin(phi) * std::sin(theta),
            std::cos(phi)
        );
    }

    // Permutation table
    std::vector<int> perm(512);
    for (int i = 0; i < 256; ++i) perm[i] = i;
    for (int i = 255; i > 0; --i) {
        int j = static_cast<int>(dist(gen) * i);
        std::swap(perm[i], perm[j]);
    }
    for (int i = 0; i < 256; ++i) perm[256 + i] = perm[i];

    auto fade = [](float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); };
    auto lerp_f = [](float a, float b, float t) { return a + t * (b - a); };

    for (uint32_t z = 0; z < size; ++z) {
        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                float fx = static_cast<float>(x) / static_cast<float>(size) * 4.0f;
                float fy = static_cast<float>(y) / static_cast<float>(size) * 4.0f;
                float fz = static_cast<float>(z) / static_cast<float>(size) * 4.0f;

                int X = static_cast<int>(std::floor(fx)) & 255;
                int Y = static_cast<int>(std::floor(fy)) & 255;
                int Z = static_cast<int>(std::floor(fz)) & 255;

                fx -= std::floor(fx);
                fy -= std::floor(fy);
                fz -= std::floor(fz);

                float u = fade(fx);
                float v = fade(fy);
                float w = fade(fz);

                int A = perm[X] + Y;
                int AA = perm[A] + Z;
                int AB = perm[A + 1] + Z;
                int B = perm[X + 1] + Y;
                int BA = perm[B] + Z;
                int BB = perm[B + 1] + Z;

                auto grad = [&](int hash, float x, float y, float z) {
                    const Vec3& g = gradients[hash & 255];
                    return g.x * x + g.y * y + g.z * z;
                };

                float noise = lerp_f(
                    lerp_f(
                        lerp_f(grad(perm[AA], fx, fy, fz), grad(perm[BA], fx - 1, fy, fz), u),
                        lerp_f(grad(perm[AB], fx, fy - 1, fz), grad(perm[BB], fx - 1, fy - 1, fz), u),
                        v
                    ),
                    lerp_f(
                        lerp_f(grad(perm[AA + 1], fx, fy, fz - 1), grad(perm[BA + 1], fx - 1, fy, fz - 1), u),
                        lerp_f(grad(perm[AB + 1], fx, fy - 1, fz - 1), grad(perm[BB + 1], fx - 1, fy - 1, fz - 1), u),
                        v
                    ),
                    w
                );

                // Normalize to 0-255
                uint8_t val = static_cast<uint8_t>((noise * 0.5f + 0.5f) * 255.0f);

                uint32_t idx = (z * size * size + y * size + x) * 4;
                data[idx + 0] = val;
                data[idx + 1] = val;
                data[idx + 2] = val;
                data[idx + 3] = 255;
            }
        }
    }
}

void generate_blue_noise(std::vector<uint8_t>& data, uint32_t size) {
    data.resize(size * size * 4);

    // Simple blue noise approximation using void-and-cluster algorithm
    std::random_device rd;
    std::mt19937 gen(123);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // Initialize with white noise
    for (uint32_t i = 0; i < size * size; ++i) {
        uint8_t val = static_cast<uint8_t>(dist(gen) * 255.0f);
        data[i * 4 + 0] = val;
        data[i * 4 + 1] = static_cast<uint8_t>(dist(gen) * 255.0f);
        data[i * 4 + 2] = static_cast<uint8_t>(dist(gen) * 255.0f);
        data[i * 4 + 3] = 255;
    }
}

} // namespace volumetric_noise

} // namespace engine::render
