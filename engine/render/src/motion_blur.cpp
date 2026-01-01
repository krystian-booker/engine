#include <engine/render/motion_blur.hpp>
#include <algorithm>

namespace engine::render {

// Global instance
static MotionBlurSystem* s_motion_blur_system = nullptr;

MotionBlurSystem& get_motion_blur_system() {
    if (!s_motion_blur_system) {
        static MotionBlurSystem instance;
        s_motion_blur_system = &instance;
    }
    return *s_motion_blur_system;
}

MotionBlurSystem::~MotionBlurSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void MotionBlurSystem::init(uint32_t width, uint32_t height, const MotionBlurConfig& config) {
    if (m_initialized) return;

    m_config = config;
    m_width = width;
    m_height = height;

    create_textures(width, height);
    create_programs();

    // Create uniforms
    u_motion_params = bgfx::createUniform("u_motionParams", bgfx::UniformType::Vec4);
    u_motion_params2 = bgfx::createUniform("u_motionParams2", bgfx::UniformType::Vec4);
    u_view_proj = bgfx::createUniform("u_viewProj", bgfx::UniformType::Mat4);
    u_prev_view_proj = bgfx::createUniform("u_prevViewProj", bgfx::UniformType::Mat4);
    u_inv_view_proj = bgfx::createUniform("u_invViewProj", bgfx::UniformType::Mat4);
    u_texel_size = bgfx::createUniform("u_texelSize", bgfx::UniformType::Vec4);

    s_color = bgfx::createUniform("s_color", bgfx::UniformType::Sampler);
    s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);
    s_velocity = bgfx::createUniform("s_velocity", bgfx::UniformType::Sampler);
    s_tile_max = bgfx::createUniform("s_tileMax", bgfx::UniformType::Sampler);
    s_neighbor_max = bgfx::createUniform("s_neighborMax", bgfx::UniformType::Sampler);

    // Calculate tile counts
    m_stats.tile_count_x = (width + m_config.tile_size - 1) / m_config.tile_size;
    m_stats.tile_count_y = (height + m_config.tile_size - 1) / m_config.tile_size;

    m_initialized = true;
}

void MotionBlurSystem::shutdown() {
    if (!m_initialized) return;

    destroy_textures();
    destroy_programs();

    // Destroy uniforms
    if (bgfx::isValid(u_motion_params)) bgfx::destroy(u_motion_params);
    if (bgfx::isValid(u_motion_params2)) bgfx::destroy(u_motion_params2);
    if (bgfx::isValid(u_view_proj)) bgfx::destroy(u_view_proj);
    if (bgfx::isValid(u_prev_view_proj)) bgfx::destroy(u_prev_view_proj);
    if (bgfx::isValid(u_inv_view_proj)) bgfx::destroy(u_inv_view_proj);
    if (bgfx::isValid(u_texel_size)) bgfx::destroy(u_texel_size);

    if (bgfx::isValid(s_color)) bgfx::destroy(s_color);
    if (bgfx::isValid(s_depth)) bgfx::destroy(s_depth);
    if (bgfx::isValid(s_velocity)) bgfx::destroy(s_velocity);
    if (bgfx::isValid(s_tile_max)) bgfx::destroy(s_tile_max);
    if (bgfx::isValid(s_neighbor_max)) bgfx::destroy(s_neighbor_max);

    m_initialized = false;
}

void MotionBlurSystem::resize(uint32_t width, uint32_t height) {
    if (m_width == width && m_height == height) return;

    m_width = width;
    m_height = height;

    destroy_textures();
    create_textures(width, height);

    m_stats.tile_count_x = (width + m_config.tile_size - 1) / m_config.tile_size;
    m_stats.tile_count_y = (height + m_config.tile_size - 1) / m_config.tile_size;
}

void MotionBlurSystem::create_textures(uint32_t width, uint32_t height) {
    // Velocity buffer (RG16F for high precision velocity)
    m_velocity_texture = bgfx::createTexture2D(
        width, height, false, 1,
        bgfx::TextureFormat::RG16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );
    m_velocity_fb = bgfx::createFrameBuffer(1, &m_velocity_texture);

    // Tile max velocity texture
    if (m_config.tile_based) {
        uint32_t tile_width = (width + m_config.tile_size - 1) / m_config.tile_size;
        uint32_t tile_height = (height + m_config.tile_size - 1) / m_config.tile_size;

        m_tile_max_texture = bgfx::createTexture2D(
            tile_width, tile_height, false, 1,
            bgfx::TextureFormat::RG16F,
            BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
        );
        m_tile_max_fb = bgfx::createFrameBuffer(1, &m_tile_max_texture);

        // Neighbor max (3x3 max of tiles)
        m_neighbor_max_texture = bgfx::createTexture2D(
            tile_width, tile_height, false, 1,
            bgfx::TextureFormat::RG16F,
            BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
        );
        m_neighbor_max_fb = bgfx::createFrameBuffer(1, &m_neighbor_max_texture);
    }

    // Result texture
    m_result_texture = bgfx::createTexture2D(
        width, height, false, 1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );
    m_result_fb = bgfx::createFrameBuffer(1, &m_result_texture);
}

void MotionBlurSystem::destroy_textures() {
    if (bgfx::isValid(m_velocity_texture)) {
        bgfx::destroy(m_velocity_texture);
        m_velocity_texture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_tile_max_texture)) {
        bgfx::destroy(m_tile_max_texture);
        m_tile_max_texture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_neighbor_max_texture)) {
        bgfx::destroy(m_neighbor_max_texture);
        m_neighbor_max_texture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_result_texture)) {
        bgfx::destroy(m_result_texture);
        m_result_texture = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(m_velocity_fb)) {
        bgfx::destroy(m_velocity_fb);
        m_velocity_fb = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_tile_max_fb)) {
        bgfx::destroy(m_tile_max_fb);
        m_tile_max_fb = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_neighbor_max_fb)) {
        bgfx::destroy(m_neighbor_max_fb);
        m_neighbor_max_fb = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_result_fb)) {
        bgfx::destroy(m_result_fb);
        m_result_fb = BGFX_INVALID_HANDLE;
    }
}

void MotionBlurSystem::create_programs() {
    // Programs would be loaded from compiled shaders
    m_camera_velocity_program = BGFX_INVALID_HANDLE;
    m_tile_max_program = BGFX_INVALID_HANDLE;
    m_neighbor_max_program = BGFX_INVALID_HANDLE;
    m_blur_program = BGFX_INVALID_HANDLE;
}

void MotionBlurSystem::destroy_programs() {
    if (bgfx::isValid(m_camera_velocity_program)) {
        bgfx::destroy(m_camera_velocity_program);
        m_camera_velocity_program = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_tile_max_program)) {
        bgfx::destroy(m_tile_max_program);
        m_tile_max_program = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_neighbor_max_program)) {
        bgfx::destroy(m_neighbor_max_program);
        m_neighbor_max_program = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_blur_program)) {
        bgfx::destroy(m_blur_program);
        m_blur_program = BGFX_INVALID_HANDLE;
    }
}

void MotionBlurSystem::generate_camera_velocity(bgfx::ViewId view_id,
                                                 bgfx::TextureHandle depth_texture,
                                                 const Mat4& current_view_proj,
                                                 const Mat4& prev_view_proj,
                                                 const Mat4& inv_view_proj) {
    if (!m_config.camera_motion || !bgfx::isValid(m_camera_velocity_program)) return;

    bgfx::setViewFrameBuffer(view_id, m_velocity_fb);
    bgfx::setViewRect(view_id, 0, 0, m_width, m_height);

    // Set matrices
    bgfx::setUniform(u_view_proj, &current_view_proj.m[0][0]);
    bgfx::setUniform(u_prev_view_proj, &prev_view_proj.m[0][0]);
    bgfx::setUniform(u_inv_view_proj, &inv_view_proj.m[0][0]);

    // Motion params: shutter_fraction, max_velocity, 0, 0
    float params[4] = {
        m_config.get_shutter_fraction(),
        m_config.max_blur_radius,
        0.0f, 0.0f
    };
    bgfx::setUniform(u_motion_params, params);

    // Texel size
    float texel_size[4] = {
        1.0f / m_width,
        1.0f / m_height,
        static_cast<float>(m_width),
        static_cast<float>(m_height)
    };
    bgfx::setUniform(u_texel_size, texel_size);

    // Bind depth texture
    bgfx::setTexture(0, s_depth, depth_texture);

    bgfx::setState(BGFX_STATE_WRITE_RG);
    bgfx::submit(view_id, m_camera_velocity_program);
}

void MotionBlurSystem::generate_tile_max(bgfx::ViewId view_id) {
    if (!m_config.tile_based) return;

    // Tile max pass
    if (bgfx::isValid(m_tile_max_program)) {
        uint32_t tile_width = (m_width + m_config.tile_size - 1) / m_config.tile_size;
        uint32_t tile_height = (m_height + m_config.tile_size - 1) / m_config.tile_size;

        bgfx::setViewFrameBuffer(view_id, m_tile_max_fb);
        bgfx::setViewRect(view_id, 0, 0, tile_width, tile_height);

        // Tile params
        float params[4] = {
            static_cast<float>(m_config.tile_size),
            static_cast<float>(m_width),
            static_cast<float>(m_height),
            0.0f
        };
        bgfx::setUniform(u_motion_params, params);

        bgfx::setTexture(0, s_velocity, m_velocity_texture);

        bgfx::setState(BGFX_STATE_WRITE_RG);
        bgfx::submit(view_id, m_tile_max_program);
    }

    // Neighbor max pass
    if (bgfx::isValid(m_neighbor_max_program)) {
        uint32_t tile_width = (m_width + m_config.tile_size - 1) / m_config.tile_size;
        uint32_t tile_height = (m_height + m_config.tile_size - 1) / m_config.tile_size;

        bgfx::setViewFrameBuffer(view_id + 1, m_neighbor_max_fb);
        bgfx::setViewRect(view_id + 1, 0, 0, tile_width, tile_height);

        float texel_size[4] = {
            1.0f / tile_width,
            1.0f / tile_height,
            static_cast<float>(tile_width),
            static_cast<float>(tile_height)
        };
        bgfx::setUniform(u_texel_size, texel_size);

        bgfx::setTexture(0, s_tile_max, m_tile_max_texture);

        bgfx::setState(BGFX_STATE_WRITE_RG);
        bgfx::submit(view_id + 1, m_neighbor_max_program);
    }
}

void MotionBlurSystem::apply(bgfx::ViewId view_id,
                              bgfx::TextureHandle color_texture,
                              bgfx::TextureHandle depth_texture) {
    if (!bgfx::isValid(m_blur_program)) return;

    bgfx::setViewFrameBuffer(view_id, m_result_fb);
    bgfx::setViewRect(view_id, 0, 0, m_width, m_height);

    // Motion blur params
    float params[4] = {
        m_config.intensity,
        m_config.max_blur_radius,
        m_config.min_velocity_threshold,
        static_cast<float>(m_config.samples)
    };
    bgfx::setUniform(u_motion_params, params);

    // Additional params
    float jitter = m_config.jitter_samples ? 1.0f : 0.0f;
    float depth_aware = m_config.depth_aware ? m_config.depth_falloff : 0.0f;
    float params2[4] = {
        jitter,
        depth_aware,
        m_config.center_attenuation ? m_config.center_falloff_start : 0.0f,
        m_config.center_attenuation ? m_config.center_falloff_end : 0.0f
    };
    bgfx::setUniform(u_motion_params2, params2);

    // Texel size
    float texel_size[4] = {
        1.0f / m_width,
        1.0f / m_height,
        static_cast<float>(m_width),
        static_cast<float>(m_height)
    };
    bgfx::setUniform(u_texel_size, texel_size);

    // Bind textures
    bgfx::setTexture(0, s_color, color_texture);
    bgfx::setTexture(1, s_depth, depth_texture);
    bgfx::setTexture(2, s_velocity, m_velocity_texture);

    if (m_config.tile_based) {
        bgfx::setTexture(3, s_neighbor_max, m_neighbor_max_texture);
    }

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(view_id, m_blur_program);

    m_frame_count++;
}

void MotionBlurSystem::render(bgfx::ViewId velocity_view,
                               bgfx::ViewId tile_view,
                               bgfx::ViewId blur_view,
                               bgfx::TextureHandle color_texture,
                               bgfx::TextureHandle depth_texture,
                               const Mat4& current_view_proj,
                               const Mat4& prev_view_proj,
                               const Mat4& inv_view_proj) {
    if (!m_initialized) return;

    // Generate camera velocity
    if (m_config.camera_motion) {
        generate_camera_velocity(velocity_view, depth_texture,
                                  current_view_proj, prev_view_proj, inv_view_proj);
    }

    // Generate tile max (if tile-based)
    if (m_config.tile_based) {
        generate_tile_max(tile_view);
    }

    // Apply motion blur
    apply(blur_view, color_texture, depth_texture);
}

} // namespace engine::render
