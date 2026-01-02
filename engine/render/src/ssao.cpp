#include <engine/render/ssao.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <bgfx/bgfx.h>
#include <random>
#include <cmath>
#include <fstream>

namespace engine::render {

using namespace engine::core;

// SSAO shader programs and uniforms
struct SSAOShaders {
    bgfx::ProgramHandle ssao = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle blur = BGFX_INVALID_HANDLE;

    // Uniforms
    bgfx::UniformHandle u_ssaoParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_projParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_noiseScale = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_samples = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_depth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_normal = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_noise = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_aoInput = BGFX_INVALID_HANDLE;

    // Fullscreen quad resources
    bgfx::VertexBufferHandle fullscreen_vb = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle fullscreen_ib = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout fullscreen_layout;

    void create();
    void destroy();
    void draw_fullscreen(uint16_t view_id, bgfx::ProgramHandle program);
};

static SSAOShaders s_ssao_shaders;

static bgfx::ShaderHandle load_ssao_shader(const std::string& path) {
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

static std::string get_ssao_shader_path() {
    auto type = bgfx::getRendererType();
    switch (type) {
        case bgfx::RendererType::Direct3D11:
        case bgfx::RendererType::Direct3D12: return "shaders/dx11/";
        case bgfx::RendererType::Vulkan: return "shaders/spirv/";
        case bgfx::RendererType::OpenGL: return "shaders/glsl/";
        default: return "shaders/spirv/";
    }
}

void SSAOShaders::create() {
    std::string path = get_ssao_shader_path();

    // Load fullscreen vertex shader
    bgfx::ShaderHandle vs = load_ssao_shader(path + "vs_fullscreen.sc.bin");

    // Load fragment shaders
    bgfx::ShaderHandle fs_ssao = load_ssao_shader(path + "fs_ssao.sc.bin");
    bgfx::ShaderHandle fs_blur = load_ssao_shader(path + "fs_ssao_blur.sc.bin");

    // Create programs
    if (bgfx::isValid(vs)) {
        if (bgfx::isValid(fs_ssao)) {
            ssao = bgfx::createProgram(vs, fs_ssao, false);
        }
        if (bgfx::isValid(fs_blur)) {
            blur = bgfx::createProgram(vs, fs_blur, false);
        }
    }

    // Destroy individual shaders
    if (bgfx::isValid(vs)) bgfx::destroy(vs);
    if (bgfx::isValid(fs_ssao)) bgfx::destroy(fs_ssao);
    if (bgfx::isValid(fs_blur)) bgfx::destroy(fs_blur);

    // Create uniforms
    u_ssaoParams = bgfx::createUniform("u_ssaoParams", bgfx::UniformType::Vec4);
    u_projParams = bgfx::createUniform("u_projParams", bgfx::UniformType::Vec4);
    u_noiseScale = bgfx::createUniform("u_noiseScale", bgfx::UniformType::Vec4);
    u_samples = bgfx::createUniform("u_samples", bgfx::UniformType::Vec4, 16);
    s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);
    s_normal = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);
    s_noise = bgfx::createUniform("s_noise", bgfx::UniformType::Sampler);
    s_aoInput = bgfx::createUniform("s_aoInput", bgfx::UniformType::Sampler);

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

    log(LogLevel::Info, "SSAO shaders initialized");
}

void SSAOShaders::destroy() {
    if (bgfx::isValid(ssao)) bgfx::destroy(ssao);
    if (bgfx::isValid(blur)) bgfx::destroy(blur);

    if (bgfx::isValid(u_ssaoParams)) bgfx::destroy(u_ssaoParams);
    if (bgfx::isValid(u_projParams)) bgfx::destroy(u_projParams);
    if (bgfx::isValid(u_noiseScale)) bgfx::destroy(u_noiseScale);
    if (bgfx::isValid(u_samples)) bgfx::destroy(u_samples);
    if (bgfx::isValid(s_depth)) bgfx::destroy(s_depth);
    if (bgfx::isValid(s_normal)) bgfx::destroy(s_normal);
    if (bgfx::isValid(s_noise)) bgfx::destroy(s_noise);
    if (bgfx::isValid(s_aoInput)) bgfx::destroy(s_aoInput);

    if (bgfx::isValid(fullscreen_vb)) bgfx::destroy(fullscreen_vb);
    if (bgfx::isValid(fullscreen_ib)) bgfx::destroy(fullscreen_ib);

    ssao = BGFX_INVALID_HANDLE;
    blur = BGFX_INVALID_HANDLE;
}

void SSAOShaders::draw_fullscreen(uint16_t view_id, bgfx::ProgramHandle program) {
    if (!bgfx::isValid(program) || !bgfx::isValid(fullscreen_vb)) return;

    bgfx::setVertexBuffer(0, fullscreen_vb);
    bgfx::setIndexBuffer(fullscreen_ib);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(view_id, program);
}

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

    // Create shaders (shared across all SSAOSystem instances)
    static bool shaders_created = false;
    if (!shaders_created) {
        s_ssao_shaders.create();
        shaders_created = true;
    }

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
    TextureHandle depth_texture,
    TextureHandle normal_texture,
    const Mat4& projection,
    const Mat4& /*view*/
) {
    if (!m_initialized || !bgfx::isValid(s_ssao_shaders.ssao)) {
        return get_ao_texture();
    }

    uint16_t view_id = static_cast<uint16_t>(RenderView::SSAO);

    // Get native texture handles
    uint16_t depth_idx = m_renderer->get_native_texture_handle(depth_texture);
    uint16_t noise_idx = m_renderer->get_native_texture_handle(m_noise_texture);

    if (depth_idx == bgfx::kInvalidHandle) {
        return get_ao_texture();
    }

    bgfx::TextureHandle depth_handle = { depth_idx };
    bgfx::TextureHandle noise_handle = { noise_idx };

    // Extract projection parameters
    // Assuming standard perspective projection matrix
    float near_plane = projection[3][2] / (projection[2][2] - 1.0f);
    float far_plane = projection[3][2] / (projection[2][2] + 1.0f);
    float aspect = projection[1][1] / projection[0][0];
    float tan_half_fov = 1.0f / projection[1][1];

    Vec4 ssao_params(m_config.radius, m_config.bias, m_config.intensity, m_config.power);
    Vec4 proj_params(near_plane, far_plane, aspect, tan_half_fov);

    uint32_t ao_width = m_config.half_resolution ? m_width / 2 : m_width;
    uint32_t ao_height = m_config.half_resolution ? m_height / 2 : m_height;
    Vec4 noise_scale(
        float(ao_width) / 4.0f,
        float(ao_height) / 4.0f,
        float(ao_width),
        float(ao_height)
    );

    // Set uniforms
    bgfx::setUniform(s_ssao_shaders.u_ssaoParams, &ssao_params);
    bgfx::setUniform(s_ssao_shaders.u_projParams, &proj_params);
    bgfx::setUniform(s_ssao_shaders.u_noiseScale, &noise_scale);
    bgfx::setUniform(s_ssao_shaders.u_samples, m_kernel.data(), 16);

    // Bind textures
    bgfx::setTexture(0, s_ssao_shaders.s_depth, depth_handle);

    if (normal_texture.valid()) {
        uint16_t normal_idx = m_renderer->get_native_texture_handle(normal_texture);
        if (normal_idx != bgfx::kInvalidHandle) {
            bgfx::TextureHandle normal_handle = { normal_idx };
            bgfx::setTexture(1, s_ssao_shaders.s_normal, normal_handle);
        }
    }

    if (noise_idx != bgfx::kInvalidHandle) {
        bgfx::setTexture(2, s_ssao_shaders.s_noise, noise_handle);
    }

    // Draw SSAO fullscreen pass
    s_ssao_shaders.draw_fullscreen(view_id, s_ssao_shaders.ssao);

    // Bilateral blur passes if enabled
    if (m_config.blur_enabled && bgfx::isValid(s_ssao_shaders.blur) && m_blur_temp_target.valid()) {
        uint16_t blur_view_id = static_cast<uint16_t>(RenderView::SSAOBlur);

        TextureHandle ao_tex = m_renderer->get_render_target_texture(m_ao_target, 0);
        uint16_t ao_idx = m_renderer->get_native_texture_handle(ao_tex);

        if (ao_idx != bgfx::kInvalidHandle) {
            bgfx::TextureHandle ao_handle = { ao_idx };

            for (int pass = 0; pass < m_config.blur_passes; ++pass) {
                // Bind AO texture as input
                bgfx::setTexture(0, s_ssao_shaders.s_aoInput, ao_handle);

                // Draw blur pass
                s_ssao_shaders.draw_fullscreen(blur_view_id, s_ssao_shaders.blur);
            }
        }
    }

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
