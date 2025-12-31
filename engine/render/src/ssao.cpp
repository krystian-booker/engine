#include <engine/render/ssao.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <random>
#include <cmath>

namespace engine::render {

using namespace engine::core;

SSAOSystem::~SSAOSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void SSAOSystem::init(IRenderer* renderer, const SSAOConfig& config) {
    m_renderer = renderer;
    m_config = config;
    m_width = renderer->get_width();
    m_height = renderer->get_height();

    // Generate hemisphere sample kernel
    generate_kernel();

    // Create noise texture
    create_noise_texture();

    // Create render targets
    create_render_targets();

    m_initialized = true;
    log(LogLevel::Info, "SSAO system initialized");
}

void SSAOSystem::shutdown() {
    if (!m_initialized) return;

    destroy_render_targets();

    if (m_noise_texture.valid()) {
        m_renderer->destroy_texture(m_noise_texture);
    }

    m_initialized = false;
    m_renderer = nullptr;

    log(LogLevel::Info, "SSAO system shutdown");
}

void SSAOSystem::set_config(const SSAOConfig& config) {
    bool needs_resize = (config.half_resolution != m_config.half_resolution);
    bool needs_kernel = (config.sample_count != m_config.sample_count);

    m_config = config;

    if (needs_kernel) {
        generate_kernel();
    }

    if (needs_resize && m_initialized) {
        destroy_render_targets();
        create_render_targets();
    }
}

void SSAOSystem::generate_kernel() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> rand_float(0.0f, 1.0f);

    for (uint32_t i = 0; i < 64; ++i) {
        // Generate random point in hemisphere
        Vec3 sample(
            rand_float(gen) * 2.0f - 1.0f,
            rand_float(gen) * 2.0f - 1.0f,
            rand_float(gen)  // Hemisphere - positive Z only
        );
        sample = glm::normalize(sample);

        // Scale to be within hemisphere (random length)
        sample *= rand_float(gen);

        // Accelerating interpolation - more samples close to origin
        float scale = static_cast<float>(i) / 64.0f;
        scale = 0.1f + scale * scale * 0.9f;  // lerp(0.1, 1.0, scale^2)
        sample *= scale;

        m_kernel[i] = Vec4(sample, 0.0f);
    }
}

void SSAOSystem::create_noise_texture() {
    // Create 4x4 noise texture with random rotation vectors
    const uint32_t noise_size = 4;
    std::vector<uint8_t> noise_data(noise_size * noise_size * 4);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> rand_float(0.0f, 1.0f);

    for (uint32_t i = 0; i < noise_size * noise_size; ++i) {
        // Random rotation around Z axis (tangent-space)
        Vec3 noise(
            rand_float(gen) * 2.0f - 1.0f,
            rand_float(gen) * 2.0f - 1.0f,
            0.0f
        );
        noise = glm::normalize(noise);

        // Pack to 0-255 range
        noise_data[i * 4 + 0] = static_cast<uint8_t>((noise.x * 0.5f + 0.5f) * 255.0f);
        noise_data[i * 4 + 1] = static_cast<uint8_t>((noise.y * 0.5f + 0.5f) * 255.0f);
        noise_data[i * 4 + 2] = 0;
        noise_data[i * 4 + 3] = 255;
    }

    TextureData tex_data;
    tex_data.width = noise_size;
    tex_data.height = noise_size;
    tex_data.format = TextureFormat::RGBA8;
    tex_data.pixels = std::move(noise_data);

    m_noise_texture = m_renderer->create_texture(tex_data);
}

void SSAOSystem::create_render_targets() {
    uint32_t ao_width = m_config.half_resolution ? m_width / 2 : m_width;
    uint32_t ao_height = m_config.half_resolution ? m_height / 2 : m_height;

    // AO output target (single channel, but we use R8 for compatibility)
    RenderTargetDesc ao_desc;
    ao_desc.width = ao_width;
    ao_desc.height = ao_height;
    ao_desc.color_attachment_count = 1;
    ao_desc.color_format = TextureFormat::R8;
    ao_desc.has_depth = false;
    ao_desc.samplable = true;
    ao_desc.debug_name = "SSAO_AO";

    m_ao_target = m_renderer->create_render_target(ao_desc);

    // Configure SSAO view
    ViewConfig ao_view_config;
    ao_view_config.render_target = m_ao_target;
    ao_view_config.clear_color_enabled = true;
    ao_view_config.clear_color = 0xFFFFFFFF;  // White = no occlusion
    ao_view_config.clear_depth_enabled = false;

    m_renderer->configure_view(RenderView::SSAO, ao_view_config);

    // Blur temp target (same format as AO)
    if (m_config.blur_enabled) {
        RenderTargetDesc blur_desc = ao_desc;
        blur_desc.debug_name = "SSAO_BlurTemp";

        m_blur_temp_target = m_renderer->create_render_target(blur_desc);

        ViewConfig blur_view_config;
        blur_view_config.render_target = m_blur_temp_target;
        blur_view_config.clear_color_enabled = false;
        blur_view_config.clear_depth_enabled = false;

        m_renderer->configure_view(RenderView::SSAOBlur, blur_view_config);
    }
}

void SSAOSystem::destroy_render_targets() {
    if (m_ao_target.valid()) {
        m_renderer->destroy_render_target(m_ao_target);
        m_ao_target = RenderTargetHandle{};
    }

    if (m_blur_temp_target.valid()) {
        m_renderer->destroy_render_target(m_blur_temp_target);
        m_blur_temp_target = RenderTargetHandle{};
    }
}

TextureHandle SSAOSystem::render(
    TextureHandle /*depth_texture*/,
    TextureHandle /*normal_texture*/,
    const Mat4& /*projection*/,
    const Mat4& /*view*/
) {
    // Note: This method sets up uniforms and queues the SSAO fullscreen pass
    // The actual shader execution happens in the renderer's flush() call

    // TODO: Set SSAO uniforms (kernel samples, noise scale, projection params)
    // TODO: Bind depth and normal textures
    // TODO: Queue fullscreen quad draw to SSAO view
    // TODO: If blur enabled, queue blur passes

    return get_ao_texture();
}

TextureHandle SSAOSystem::get_ao_texture() const {
    if (m_config.blur_enabled && m_blur_temp_target.valid()) {
        return m_renderer->get_render_target_texture(m_blur_temp_target, 0);
    }
    return m_renderer->get_render_target_texture(m_ao_target, 0);
}

void SSAOSystem::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    if (m_initialized) {
        destroy_render_targets();
        create_render_targets();
    }
}

// GTAO helper implementations
namespace gtao {

void generate_hilbert_noise(std::vector<uint8_t>& pixels, uint32_t size) {
    // Generate blue noise pattern using Hilbert curve
    // This provides better temporal stability than pure random noise
    pixels.resize(size * size * 4);

    std::random_device rd;
    std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<float> rand_float(0.0f, 1.0f);

    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            uint32_t idx = (y * size + x) * 4;

            // Spatial-temporal noise pattern
            float angle = rand_float(gen) * 2.0f * 3.14159265f;
            float offset = rand_float(gen);

            pixels[idx + 0] = static_cast<uint8_t>(std::cos(angle) * 127.5f + 127.5f);
            pixels[idx + 1] = static_cast<uint8_t>(std::sin(angle) * 127.5f + 127.5f);
            pixels[idx + 2] = static_cast<uint8_t>(offset * 255.0f);
            pixels[idx + 3] = 255;
        }
    }
}

float integrate_arc(float h1, float h2, float n) {
    // Integrate visibility function over arc defined by horizon angles
    float cosN = std::cos(n);
    float sinN = std::sin(n);

    return 0.25f * (-std::cos(2.0f * h1 - n) + cosN + 2.0f * h1 * sinN)
         + 0.25f * (-std::cos(2.0f * h2 - n) + cosN + 2.0f * h2 * sinN);
}

} // namespace gtao

} // namespace engine::render
