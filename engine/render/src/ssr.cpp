#include <engine/render/ssr.hpp>
#include <engine/core/log.hpp>
#include <cmath>
#include <algorithm>
#include <fstream>

namespace engine::render {

using namespace engine::core;

// Shader loading helpers
static bgfx::ShaderHandle load_ssr_shader(const std::string& path) {
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

static std::string get_ssr_shader_path() {
    auto type = bgfx::getRendererType();
    switch (type) {
        case bgfx::RendererType::Direct3D11:
        case bgfx::RendererType::Direct3D12: return "shaders/dx11/";
        case bgfx::RendererType::Vulkan: return "shaders/spirv/";
        case bgfx::RendererType::OpenGL: return "shaders/glsl/";
        default: return "shaders/spirv/";
    }
}

// Global instance
static SSRSystem* s_ssr_system = nullptr;

SSRSystem& get_ssr_system() {
    if (!s_ssr_system) {
        static SSRSystem instance;
        s_ssr_system = &instance;
    }
    return *s_ssr_system;
}

SSRSystem::~SSRSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void SSRSystem::init(uint32_t width, uint32_t height, const SSRConfig& config) {
    if (m_initialized) return;

    m_config = config;
    m_width = width;
    m_height = height;

    // Calculate trace resolution
    m_trace_width = static_cast<uint32_t>(width * m_config.resolution_scale);
    m_trace_height = static_cast<uint32_t>(height * m_config.resolution_scale);

    // Create textures
    create_textures(width, height);

    // Create programs (would load compiled shaders)
    create_programs();

    // Create uniforms
    u_ssr_params = bgfx::createUniform("u_ssrParams", bgfx::UniformType::Vec4);
    u_ssr_params2 = bgfx::createUniform("u_ssrParams2", bgfx::UniformType::Vec4);
    u_view_matrix = bgfx::createUniform("u_viewMatrix", bgfx::UniformType::Mat4);
    u_proj_matrix = bgfx::createUniform("u_projMatrix", bgfx::UniformType::Mat4);
    u_inv_proj_matrix = bgfx::createUniform("u_invProjMatrix", bgfx::UniformType::Mat4);
    u_inv_view_matrix = bgfx::createUniform("u_invViewMatrix", bgfx::UniformType::Mat4);
    u_prev_view_proj = bgfx::createUniform("u_prevViewProj", bgfx::UniformType::Mat4);
    u_texel_size = bgfx::createUniform("u_texelSize", bgfx::UniformType::Vec4);
    u_hiz_level = bgfx::createUniform("u_hizLevel", bgfx::UniformType::Vec4);

    s_color = bgfx::createUniform("s_color", bgfx::UniformType::Sampler);
    s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);
    s_normal = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);
    s_roughness = bgfx::createUniform("s_roughness", bgfx::UniformType::Sampler);
    s_hiz = bgfx::createUniform("s_hiz", bgfx::UniformType::Sampler);
    s_reflection = bgfx::createUniform("s_reflection", bgfx::UniformType::Sampler);
    s_history = bgfx::createUniform("s_history", bgfx::UniformType::Sampler);
    s_velocity = bgfx::createUniform("s_velocity", bgfx::UniformType::Sampler);
    s_hit = bgfx::createUniform("s_hit", bgfx::UniformType::Sampler);

    m_stats.trace_width = m_trace_width;
    m_stats.trace_height = m_trace_height;
    m_stats.hiz_levels = m_config.hiz_levels;

    m_initialized = true;
}

void SSRSystem::shutdown() {
    if (!m_initialized) return;

    destroy_textures();
    destroy_programs();

    // Destroy uniforms
    if (bgfx::isValid(u_ssr_params)) bgfx::destroy(u_ssr_params);
    if (bgfx::isValid(u_ssr_params2)) bgfx::destroy(u_ssr_params2);
    if (bgfx::isValid(u_view_matrix)) bgfx::destroy(u_view_matrix);
    if (bgfx::isValid(u_proj_matrix)) bgfx::destroy(u_proj_matrix);
    if (bgfx::isValid(u_inv_proj_matrix)) bgfx::destroy(u_inv_proj_matrix);
    if (bgfx::isValid(u_inv_view_matrix)) bgfx::destroy(u_inv_view_matrix);
    if (bgfx::isValid(u_prev_view_proj)) bgfx::destroy(u_prev_view_proj);
    if (bgfx::isValid(u_texel_size)) bgfx::destroy(u_texel_size);
    if (bgfx::isValid(u_hiz_level)) bgfx::destroy(u_hiz_level);

    if (bgfx::isValid(s_color)) bgfx::destroy(s_color);
    if (bgfx::isValid(s_depth)) bgfx::destroy(s_depth);
    if (bgfx::isValid(s_normal)) bgfx::destroy(s_normal);
    if (bgfx::isValid(s_roughness)) bgfx::destroy(s_roughness);
    if (bgfx::isValid(s_hiz)) bgfx::destroy(s_hiz);
    if (bgfx::isValid(s_reflection)) bgfx::destroy(s_reflection);
    if (bgfx::isValid(s_history)) bgfx::destroy(s_history);
    if (bgfx::isValid(s_velocity)) bgfx::destroy(s_velocity);
    if (bgfx::isValid(s_hit)) bgfx::destroy(s_hit);

    m_initialized = false;
}

void SSRSystem::resize(uint32_t width, uint32_t height) {
    if (m_width == width && m_height == height) return;

    m_width = width;
    m_height = height;
    m_trace_width = static_cast<uint32_t>(width * m_config.resolution_scale);
    m_trace_height = static_cast<uint32_t>(height * m_config.resolution_scale);

    destroy_textures();
    create_textures(width, height);

    m_stats.trace_width = m_trace_width;
    m_stats.trace_height = m_trace_height;
}

void SSRSystem::create_textures(uint32_t width, uint32_t height) {
    // Reflection textures — ping-pong pair (RGBA16F for HDR)
    for (int i = 0; i < 2; ++i) {
        m_reflection_textures[i] = bgfx::createTexture2D(
            m_trace_width, m_trace_height, false, 1,
            bgfx::TextureFormat::RGBA16F,
            BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
        );
    }
    m_history_index = 0;

    // Hi-Z texture with mip chain
    if (m_config.use_hiz) {
        m_hiz_texture = bgfx::createTexture2D(
            width, height, true, 1,
            bgfx::TextureFormat::R32F,
            BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIP_POINT
        );

        // Create framebuffers for each hi-z level
        m_hiz_fbs.clear();
        uint32_t mip_width = width;
        uint32_t mip_height = height;

        for (uint32_t i = 0; i < m_config.hiz_levels; ++i) {
            bgfx::Attachment att;
            att.init(m_hiz_texture, bgfx::Access::Write, i);
            m_hiz_fbs.push_back(bgfx::createFrameBuffer(1, &att));

            mip_width = std::max(1u, mip_width / 2);
            mip_height = std::max(1u, mip_height / 2);
        }
    }

    // Hit texture (stores UV of hit point + PDF)
    m_hit_texture = bgfx::createTexture2D(
        m_trace_width, m_trace_height, false, 1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );

    // Pre-create framebuffers for both ping-pong configurations
    for (int i = 0; i < 2; ++i) {
        bgfx::TextureHandle trace_attachments[] = { m_reflection_textures[i], m_hit_texture };
        m_trace_fbs[i] = bgfx::createFrameBuffer(2, trace_attachments);
    }

    if (m_config.temporal_enabled) {
        for (int i = 0; i < 2; ++i) {
            m_resolve_fbs[i] = bgfx::createFrameBuffer(1, &m_reflection_textures[1 - i]);
        }
    }
}

void SSRSystem::destroy_textures() {
    for (int i = 0; i < 2; ++i) {
        if (bgfx::isValid(m_reflection_textures[i])) {
            bgfx::destroy(m_reflection_textures[i]);
            m_reflection_textures[i] = BGFX_INVALID_HANDLE;
        }
    }
    if (bgfx::isValid(m_hiz_texture)) {
        bgfx::destroy(m_hiz_texture);
        m_hiz_texture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_hit_texture)) {
        bgfx::destroy(m_hit_texture);
        m_hit_texture = BGFX_INVALID_HANDLE;
    }
    for (int i = 0; i < 2; ++i) {
        if (bgfx::isValid(m_trace_fbs[i])) {
            bgfx::destroy(m_trace_fbs[i]);
            m_trace_fbs[i] = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_resolve_fbs[i])) {
            bgfx::destroy(m_resolve_fbs[i]);
            m_resolve_fbs[i] = BGFX_INVALID_HANDLE;
        }
    }

    for (auto& fb : m_hiz_fbs) {
        if (bgfx::isValid(fb)) {
            bgfx::destroy(fb);
        }
    }
    m_hiz_fbs.clear();
}

void SSRSystem::create_programs() {
    std::string path = get_ssr_shader_path();

    // Load fullscreen vertex shader (shared by all SSR passes)
    bgfx::ShaderHandle vs = load_ssr_shader(path + "vs_fullscreen.sc.bin");
    if (!bgfx::isValid(vs)) {
        log(LogLevel::Warn, "Failed to load SSR vertex shader");
        return;
    }

    // Hi-Z program
    {
        bgfx::ShaderHandle fs = load_ssr_shader(path + "fs_ssr_hiz.sc.bin");
        if (bgfx::isValid(fs)) {
            m_hiz_program = bgfx::createProgram(vs, fs, false);
            bgfx::destroy(fs);
            log(LogLevel::Debug, "SSR Hi-Z shader loaded");
        } else {
            log(LogLevel::Warn, "Failed to load SSR Hi-Z fragment shader");
        }
    }

    // Trace program
    {
        bgfx::ShaderHandle fs = load_ssr_shader(path + "fs_ssr_trace.sc.bin");
        if (bgfx::isValid(fs)) {
            m_trace_program = bgfx::createProgram(vs, fs, false);
            bgfx::destroy(fs);
            log(LogLevel::Debug, "SSR trace shader loaded");
        } else {
            log(LogLevel::Warn, "Failed to load SSR trace fragment shader");
        }
    }

    // Resolve program
    {
        bgfx::ShaderHandle fs = load_ssr_shader(path + "fs_ssr_resolve.sc.bin");
        if (bgfx::isValid(fs)) {
            m_resolve_program = bgfx::createProgram(vs, fs, false);
            bgfx::destroy(fs);
            log(LogLevel::Debug, "SSR resolve shader loaded");
        } else {
            log(LogLevel::Warn, "Failed to load SSR resolve fragment shader");
        }
    }

    // Composite program
    {
        bgfx::ShaderHandle fs = load_ssr_shader(path + "fs_ssr_composite.sc.bin");
        if (bgfx::isValid(fs)) {
            m_composite_program = bgfx::createProgram(vs, fs, false);
            bgfx::destroy(fs);
            log(LogLevel::Debug, "SSR composite shader loaded");
        } else {
            log(LogLevel::Warn, "Failed to load SSR composite fragment shader");
        }
    }

    // Destroy shared vertex shader
    bgfx::destroy(vs);

    log(LogLevel::Info, "SSR shaders initialized");
}

void SSRSystem::destroy_programs() {
    if (bgfx::isValid(m_hiz_program)) {
        bgfx::destroy(m_hiz_program);
        m_hiz_program = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_trace_program)) {
        bgfx::destroy(m_trace_program);
        m_trace_program = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_resolve_program)) {
        bgfx::destroy(m_resolve_program);
        m_resolve_program = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_composite_program)) {
        bgfx::destroy(m_composite_program);
        m_composite_program = BGFX_INVALID_HANDLE;
    }
}

void SSRSystem::generate_hiz(bgfx::ViewId view_id, bgfx::TextureHandle depth_texture) {
    if (!m_config.use_hiz || !bgfx::isValid(m_hiz_program)) return;

    uint32_t mip_width = m_width;
    uint32_t mip_height = m_height;

    for (uint32_t i = 0; i < m_config.hiz_levels && i < m_hiz_fbs.size(); ++i) {
        bgfx::ViewId vid = static_cast<bgfx::ViewId>(view_id + i);
        bgfx::setViewFrameBuffer(vid, m_hiz_fbs[i]);
        bgfx::setViewRect(vid, 0, 0, static_cast<uint16_t>(mip_width), static_cast<uint16_t>(mip_height));

        // Texel size
        float texel_size[4] = {
            1.0f / mip_width,
            1.0f / mip_height,
            static_cast<float>(mip_width),
            static_cast<float>(mip_height)
        };
        bgfx::setUniform(u_texel_size, texel_size);

        // Hi-Z level
        float level[4] = { static_cast<float>(i), 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(u_hiz_level, level);

        // Source texture (depth for level 0, previous hi-z level otherwise)
        if (i == 0) {
            bgfx::setTexture(0, s_depth, depth_texture);
        } else {
            bgfx::setTexture(0, s_hiz, m_hiz_texture, i - 1);
        }

        bgfx::setState(BGFX_STATE_WRITE_R);
        bgfx::submit(vid, m_hiz_program);

        mip_width = std::max(1u, mip_width / 2);
        mip_height = std::max(1u, mip_height / 2);
    }
}

void SSRSystem::trace(bgfx::ViewId view_id,
                      bgfx::TextureHandle color_texture,
                      bgfx::TextureHandle depth_texture,
                      bgfx::TextureHandle normal_texture,
                      bgfx::TextureHandle roughness_texture,
                      const Mat4& view_matrix,
                      const Mat4& proj_matrix,
                      const Mat4& inv_proj_matrix,
                      const Mat4& inv_view_matrix) {
    if (!bgfx::isValid(m_trace_program)) return;

    bgfx::setViewFrameBuffer(view_id, m_trace_fbs[m_history_index]);
    bgfx::setViewRect(view_id, 0, 0, static_cast<uint16_t>(m_trace_width), static_cast<uint16_t>(m_trace_height));

    // Set matrices
    bgfx::setUniform(u_view_matrix, glm::value_ptr(view_matrix));
    bgfx::setUniform(u_proj_matrix, glm::value_ptr(proj_matrix));
    bgfx::setUniform(u_inv_proj_matrix, glm::value_ptr(inv_proj_matrix));
    bgfx::setUniform(u_inv_view_matrix, glm::value_ptr(inv_view_matrix));

    // SSR params: max_steps, max_distance, thickness, stride
    float params[4] = {
        static_cast<float>(m_config.max_steps),
        m_config.max_distance,
        m_config.thickness,
        m_config.stride
    };
    bgfx::setUniform(u_ssr_params, params);

    // SSR params2: stride_cutoff, roughness_threshold, jitter, frame
    float jitter = m_config.jitter_enabled ? 1.0f : 0.0f;
    float params2[4] = {
        m_config.stride_cutoff,
        m_config.roughness_threshold,
        jitter,
        static_cast<float>(m_frame_count)
    };
    bgfx::setUniform(u_ssr_params2, params2);

    // Texel size
    float texel_size[4] = {
        1.0f / m_trace_width,
        1.0f / m_trace_height,
        static_cast<float>(m_trace_width),
        static_cast<float>(m_trace_height)
    };
    bgfx::setUniform(u_texel_size, texel_size);

    // Bind textures
    bgfx::setTexture(0, s_color, color_texture);
    bgfx::setTexture(1, s_depth, depth_texture);
    bgfx::setTexture(2, s_normal, normal_texture);
    bgfx::setTexture(3, s_roughness, roughness_texture);

    if (m_config.use_hiz && bgfx::isValid(m_hiz_texture)) {
        bgfx::setTexture(4, s_hiz, m_hiz_texture);
    }

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(view_id, m_trace_program);

    m_frame_count++;
}

void SSRSystem::temporal_resolve(bgfx::ViewId view_id,
                                  bgfx::TextureHandle velocity_texture,
                                  const Mat4& prev_view_proj) {
    if (!m_config.temporal_enabled || !bgfx::isValid(m_resolve_program)) return;

    bgfx::setViewFrameBuffer(view_id, m_resolve_fbs[m_history_index]);
    bgfx::setViewRect(view_id, 0, 0, static_cast<uint16_t>(m_trace_width), static_cast<uint16_t>(m_trace_height));

    // Previous view-proj for reprojection
    bgfx::setUniform(u_prev_view_proj, glm::value_ptr(prev_view_proj));

    // Temporal weight
    float params[4] = {
        m_config.temporal_weight,
        0.0f, 0.0f, 0.0f
    };
    bgfx::setUniform(u_ssr_params, params);

    // Texel size
    float texel_size[4] = {
        1.0f / m_trace_width,
        1.0f / m_trace_height,
        static_cast<float>(m_trace_width),
        static_cast<float>(m_trace_height)
    };
    bgfx::setUniform(u_texel_size, texel_size);

    // Bind textures — current is [m_history_index], history is [1-m_history_index]
    bgfx::setTexture(0, s_reflection, m_reflection_textures[m_history_index]);
    bgfx::setTexture(1, s_history, m_reflection_textures[1 - m_history_index]);
    bgfx::setTexture(2, s_velocity, velocity_texture);
    bgfx::setTexture(3, s_hit, m_hit_texture);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(view_id, m_resolve_program);

    // Flip ping-pong index (pre-created framebuffers already reference the correct textures)
    m_history_index = 1 - m_history_index;
}

void SSRSystem::composite(bgfx::ViewId view_id,
                          bgfx::TextureHandle scene_color,
                          bgfx::TextureHandle roughness_texture) {
    if (!bgfx::isValid(m_composite_program)) return;

    // Composite params: intensity, edge_fade_start, edge_fade_end, fresnel_bias
    float params[4] = {
        m_config.intensity,
        m_config.edge_fade_start,
        m_config.edge_fade_end,
        m_config.fresnel_bias
    };
    bgfx::setUniform(u_ssr_params, params);

    // Debug mode
    float params2[4] = {
        m_config.debug_mode ? 1.0f : 0.0f,
        m_config.roughness_threshold,
        0.0f, 0.0f
    };
    bgfx::setUniform(u_ssr_params2, params2);

    // Bind textures
    bgfx::setTexture(0, s_color, scene_color);
    bgfx::setTexture(1, s_reflection, m_reflection_textures[m_history_index]);
    bgfx::setTexture(2, s_roughness, roughness_texture);
    bgfx::setTexture(3, s_hit, m_hit_texture);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(view_id, m_composite_program);
}

void SSRSystem::render(bgfx::ViewId trace_view,
                       bgfx::ViewId resolve_view,
                       bgfx::ViewId composite_view,
                       bgfx::TextureHandle color_texture,
                       bgfx::TextureHandle depth_texture,
                       bgfx::TextureHandle normal_texture,
                       bgfx::TextureHandle roughness_texture,
                       bgfx::TextureHandle velocity_texture,
                       const Mat4& view_matrix,
                       const Mat4& proj_matrix,
                       const Mat4& inv_proj_matrix,
                       const Mat4& inv_view_matrix,
                       const Mat4& prev_view_proj) {
    if (!m_initialized) return;

    // Generate hi-z if enabled — uses view IDs starting at trace_view
    if (m_config.use_hiz) {
        generate_hiz(trace_view, depth_texture);
    }

    // Trace reflections — offset by hiz_levels to avoid view ID collision
    bgfx::ViewId actual_trace_view = m_config.use_hiz
        ? static_cast<bgfx::ViewId>(trace_view + m_config.hiz_levels)
        : trace_view;
    trace(actual_trace_view, color_texture, depth_texture, normal_texture, roughness_texture,
          view_matrix, proj_matrix, inv_proj_matrix, inv_view_matrix);

    // Temporal resolve if enabled
    if (m_config.temporal_enabled && bgfx::isValid(velocity_texture)) {
        temporal_resolve(resolve_view, velocity_texture, prev_view_proj);
    }

    // Composite is typically done by the render pipeline
    // composite(composite_view, color_texture, roughness_texture);
}

} // namespace engine::render
