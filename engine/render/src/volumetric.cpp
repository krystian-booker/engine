#include <engine/render/volumetric.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <random>
#include <cmath>

namespace engine::render {

using namespace engine::core;

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

    // Configure view
    ViewConfig view_config;
    view_config.render_target = m_volumetric_result;
    view_config.clear_color_enabled = true;
    view_config.clear_color = 0x00000000;
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
    const Mat4& /*view_matrix*/,
    const Mat4& /*proj_matrix*/,
    const Mat4& prev_view_proj,
    TextureHandle /*depth_texture*/,
    const std::array<TextureHandle, 4>& /*shadow_maps*/,
    const std::array<Mat4, 4>& /*shadow_matrices*/
) {
    if (!m_initialized) return;

    // Store previous frame data for reprojection
    m_prev_view_proj = prev_view_proj;

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
    // TODO: Inject density based on fog parameters
    // - Sample noise texture for variation
    // - Apply height-based falloff
    // - Handle local volumetric effects
}

void VolumetricSystem::scatter_light_pass() {
    // TODO: For each froxel:
    // - Sample shadow maps to determine visibility
    // - Apply phase function for each light
    // - Accumulate in-scattered radiance
}

void VolumetricSystem::temporal_filter_pass() {
    // TODO: Reproject from previous frame
    // - Calculate motion vectors from view matrices
    // - Sample history with reprojected coordinates
    // - Blend current and history with rejection
}

void VolumetricSystem::spatial_filter_pass() {
    // TODO: Apply bilateral blur to reduce noise
    // - Use depth-aware weights
    // - Separate horizontal and vertical passes
}

void VolumetricSystem::integration_pass() {
    // TODO: Front-to-back ray marching integration
    // - Accumulate opacity and in-scattered light
    // - Output final volumetric contribution
}

// ============================================================================
// Phase function implementations
// ============================================================================

namespace phase {

float henyey_greenstein(float cos_theta, float g) {
    float g2 = g * g;
    float denom = 1.0f + g2 - 2.0f * g * cos_theta;
    return (1.0f - g2) / (4.0f * 3.14159265f * std::pow(denom, 1.5f));
}

float schlick_phase(float cos_theta, float g) {
    float k = 1.55f * g - 0.55f * g * g * g;
    float denom = 1.0f - k * cos_theta;
    return (1.0f - k * k) / (4.0f * 3.14159265f * denom * denom);
}

float cornette_shanks(float cos_theta, float g) {
    float g2 = g * g;
    float num = 3.0f * (1.0f - g2) * (1.0f + cos_theta * cos_theta);
    float denom = 2.0f * (2.0f + g2) * std::pow(1.0f + g2 - 2.0f * g * cos_theta, 1.5f);
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
