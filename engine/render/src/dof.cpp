#include <engine/render/dof.hpp>
#include <algorithm>
#include <cmath>

namespace engine::render {

// Global instance
static DOFSystem* s_dof_system = nullptr;

DOFSystem& get_dof_system() {
    if (!s_dof_system) {
        static DOFSystem instance;
        s_dof_system = &instance;
    }
    return *s_dof_system;
}

DOFSystem::~DOFSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void DOFSystem::init(uint32_t width, uint32_t height, const DOFConfig& config) {
    if (m_initialized) return;

    m_config = config;
    m_width = width;
    m_height = height;
    m_half_width = width / 2;
    m_half_height = height / 2;

    m_current_focus_distance = m_config.focus_distance;
    m_target_focus_distance = m_config.focus_distance;

    create_textures(width, height);
    create_programs();

    // Create uniforms
    u_dof_params = bgfx::createUniform("u_dofParams", bgfx::UniformType::Vec4);
    u_dof_params2 = bgfx::createUniform("u_dofParams2", bgfx::UniformType::Vec4);
    u_dof_focus = bgfx::createUniform("u_dofFocus", bgfx::UniformType::Vec4);
    u_texel_size = bgfx::createUniform("u_texelSize", bgfx::UniformType::Vec4);
    u_proj_params = bgfx::createUniform("u_projParams", bgfx::UniformType::Vec4);

    s_color = bgfx::createUniform("s_color", bgfx::UniformType::Sampler);
    s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);
    s_coc = bgfx::createUniform("s_coc", bgfx::UniformType::Sampler);
    s_near = bgfx::createUniform("s_near", bgfx::UniformType::Sampler);
    s_far = bgfx::createUniform("s_far", bgfx::UniformType::Sampler);
    s_near_blur = bgfx::createUniform("s_nearBlur", bgfx::UniformType::Sampler);
    s_far_blur = bgfx::createUniform("s_farBlur", bgfx::UniformType::Sampler);
    s_bokeh = bgfx::createUniform("s_bokeh", bgfx::UniformType::Sampler);

    m_initialized = true;
}

void DOFSystem::shutdown() {
    if (!m_initialized) return;

    destroy_textures();
    destroy_programs();

    // Destroy uniforms
    if (bgfx::isValid(u_dof_params)) bgfx::destroy(u_dof_params);
    if (bgfx::isValid(u_dof_params2)) bgfx::destroy(u_dof_params2);
    if (bgfx::isValid(u_dof_focus)) bgfx::destroy(u_dof_focus);
    if (bgfx::isValid(u_texel_size)) bgfx::destroy(u_texel_size);
    if (bgfx::isValid(u_proj_params)) bgfx::destroy(u_proj_params);

    if (bgfx::isValid(s_color)) bgfx::destroy(s_color);
    if (bgfx::isValid(s_depth)) bgfx::destroy(s_depth);
    if (bgfx::isValid(s_coc)) bgfx::destroy(s_coc);
    if (bgfx::isValid(s_near)) bgfx::destroy(s_near);
    if (bgfx::isValid(s_far)) bgfx::destroy(s_far);
    if (bgfx::isValid(s_near_blur)) bgfx::destroy(s_near_blur);
    if (bgfx::isValid(s_far_blur)) bgfx::destroy(s_far_blur);
    if (bgfx::isValid(s_bokeh)) bgfx::destroy(s_bokeh);

    m_initialized = false;
}

void DOFSystem::resize(uint32_t width, uint32_t height) {
    if (m_width == width && m_height == height) return;

    m_width = width;
    m_height = height;
    m_half_width = width / 2;
    m_half_height = height / 2;

    destroy_textures();
    create_textures(width, height);
}

void DOFSystem::create_textures(uint32_t width, uint32_t height) {
    // CoC texture (R16F for signed CoC)
    m_coc_texture = bgfx::createTexture2D(
        width, height, false, 1,
        bgfx::TextureFormat::R16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );
    m_coc_fb = bgfx::createFrameBuffer(1, &m_coc_texture);

    // Half-resolution near/far textures
    m_near_texture = bgfx::createTexture2D(
        m_half_width, m_half_height, false, 1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );

    m_far_texture = bgfx::createTexture2D(
        m_half_width, m_half_height, false, 1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );

    bgfx::TextureHandle downsample_attachments[] = { m_near_texture, m_far_texture };
    m_downsample_fb = bgfx::createFrameBuffer(2, downsample_attachments);

    // Blurred textures
    m_near_blur_texture = bgfx::createTexture2D(
        m_half_width, m_half_height, false, 1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );
    m_near_blur_fb = bgfx::createFrameBuffer(1, &m_near_blur_texture);

    m_far_blur_texture = bgfx::createTexture2D(
        m_half_width, m_half_height, false, 1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );
    m_far_blur_fb = bgfx::createFrameBuffer(1, &m_far_blur_texture);

    // Result texture
    m_result_texture = bgfx::createTexture2D(
        width, height, false, 1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );
    m_result_fb = bgfx::createFrameBuffer(1, &m_result_texture);
}

void DOFSystem::destroy_textures() {
    if (bgfx::isValid(m_coc_texture)) bgfx::destroy(m_coc_texture);
    if (bgfx::isValid(m_near_texture)) bgfx::destroy(m_near_texture);
    if (bgfx::isValid(m_far_texture)) bgfx::destroy(m_far_texture);
    if (bgfx::isValid(m_near_blur_texture)) bgfx::destroy(m_near_blur_texture);
    if (bgfx::isValid(m_far_blur_texture)) bgfx::destroy(m_far_blur_texture);
    if (bgfx::isValid(m_result_texture)) bgfx::destroy(m_result_texture);
    if (bgfx::isValid(m_bokeh_texture)) bgfx::destroy(m_bokeh_texture);

    if (bgfx::isValid(m_coc_fb)) bgfx::destroy(m_coc_fb);
    if (bgfx::isValid(m_downsample_fb)) bgfx::destroy(m_downsample_fb);
    if (bgfx::isValid(m_near_blur_fb)) bgfx::destroy(m_near_blur_fb);
    if (bgfx::isValid(m_far_blur_fb)) bgfx::destroy(m_far_blur_fb);
    if (bgfx::isValid(m_result_fb)) bgfx::destroy(m_result_fb);

    m_coc_texture = BGFX_INVALID_HANDLE;
    m_near_texture = BGFX_INVALID_HANDLE;
    m_far_texture = BGFX_INVALID_HANDLE;
    m_near_blur_texture = BGFX_INVALID_HANDLE;
    m_far_blur_texture = BGFX_INVALID_HANDLE;
    m_result_texture = BGFX_INVALID_HANDLE;
    m_bokeh_texture = BGFX_INVALID_HANDLE;

    m_coc_fb = BGFX_INVALID_HANDLE;
    m_downsample_fb = BGFX_INVALID_HANDLE;
    m_near_blur_fb = BGFX_INVALID_HANDLE;
    m_far_blur_fb = BGFX_INVALID_HANDLE;
    m_result_fb = BGFX_INVALID_HANDLE;
}

void DOFSystem::create_programs() {
    // Programs would be loaded from compiled shaders
    m_coc_program = BGFX_INVALID_HANDLE;
    m_downsample_program = BGFX_INVALID_HANDLE;
    m_blur_program = BGFX_INVALID_HANDLE;
    m_bokeh_program = BGFX_INVALID_HANDLE;
    m_composite_program = BGFX_INVALID_HANDLE;
}

void DOFSystem::destroy_programs() {
    if (bgfx::isValid(m_coc_program)) bgfx::destroy(m_coc_program);
    if (bgfx::isValid(m_downsample_program)) bgfx::destroy(m_downsample_program);
    if (bgfx::isValid(m_blur_program)) bgfx::destroy(m_blur_program);
    if (bgfx::isValid(m_bokeh_program)) bgfx::destroy(m_bokeh_program);
    if (bgfx::isValid(m_composite_program)) bgfx::destroy(m_composite_program);

    m_coc_program = BGFX_INVALID_HANDLE;
    m_downsample_program = BGFX_INVALID_HANDLE;
    m_blur_program = BGFX_INVALID_HANDLE;
    m_bokeh_program = BGFX_INVALID_HANDLE;
    m_composite_program = BGFX_INVALID_HANDLE;
}

void DOFSystem::update(float dt, bgfx::TextureHandle depth_texture,
                        const Mat4& inv_proj_matrix) {
    if (m_config.auto_focus) {
        // Sample depth at focus point
        m_target_focus_distance = sample_depth_at_focus_point(depth_texture, inv_proj_matrix);
        m_target_focus_distance = std::clamp(m_target_focus_distance, 0.1f, m_config.auto_focus_range);
    } else {
        m_target_focus_distance = m_config.focus_distance;
    }

    // Smooth focus transition
    float focus_speed = m_config.auto_focus_speed * dt;
    m_current_focus_distance = m_current_focus_distance +
        (m_target_focus_distance - m_current_focus_distance) * std::min(focus_speed, 1.0f);

    m_stats.current_focus = m_current_focus_distance;
    m_stats.target_focus = m_target_focus_distance;
}

float DOFSystem::sample_depth_at_focus_point(bgfx::TextureHandle /*depth_texture*/,
                                              const Mat4& /*inv_proj_matrix*/) {
    // In practice, this would read back from the depth buffer
    // For now, return current focus distance
    return m_current_focus_distance;
}

void DOFSystem::set_focus_distance(float distance) {
    m_config.focus_distance = distance;
    if (!m_config.auto_focus) {
        m_target_focus_distance = distance;
    }
}

void DOFSystem::focus_on_world_point(const Vec3& world_pos, const Mat4& view_matrix) {
    float distance = DOFUtils::focus_distance_from_world(world_pos, view_matrix);
    set_focus_distance(distance);
}

void DOFSystem::calculate_coc(bgfx::ViewId view_id,
                               bgfx::TextureHandle depth_texture,
                               const Mat4& proj_matrix) {
    if (!bgfx::isValid(m_coc_program)) return;

    bgfx::setViewFrameBuffer(view_id, m_coc_fb);
    bgfx::setViewRect(view_id, 0, 0, m_width, m_height);

    // DOF params: focus_distance, focus_range, max_blur, aperture
    float params[4] = {
        m_current_focus_distance,
        m_config.focus_range,
        m_config.max_blur_radius,
        m_config.aperture
    };
    bgfx::setUniform(u_dof_params, params);

    // Focus params: near_start, near_end, far_start, far_end
    float focus[4] = {
        m_config.near_blur_start,
        m_current_focus_distance - m_config.focus_range * 0.5f,
        m_current_focus_distance + m_config.focus_range * 0.5f,
        m_config.far_blur_start
    };
    bgfx::setUniform(u_dof_focus, focus);

    // Projection params for depth linearization
    float proj_params[4] = {
        proj_matrix.m[2][2],
        proj_matrix.m[3][2],
        proj_matrix.m[2][3],
        proj_matrix.m[3][3]
    };
    bgfx::setUniform(u_proj_params, proj_params);

    bgfx::setTexture(0, s_depth, depth_texture);

    bgfx::setState(BGFX_STATE_WRITE_R);
    bgfx::submit(view_id, m_coc_program);
}

void DOFSystem::downsample(bgfx::ViewId view_id, bgfx::TextureHandle color_texture) {
    if (!bgfx::isValid(m_downsample_program)) return;

    bgfx::setViewFrameBuffer(view_id, m_downsample_fb);
    bgfx::setViewRect(view_id, 0, 0, m_half_width, m_half_height);

    float texel_size[4] = {
        1.0f / m_width,
        1.0f / m_height,
        static_cast<float>(m_width),
        static_cast<float>(m_height)
    };
    bgfx::setUniform(u_texel_size, texel_size);

    bgfx::setTexture(0, s_color, color_texture);
    bgfx::setTexture(1, s_coc, m_coc_texture);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(view_id, m_downsample_program);
}

void DOFSystem::blur_near(bgfx::ViewId view_id) {
    if (!bgfx::isValid(m_blur_program)) return;

    bgfx::setViewFrameBuffer(view_id, m_near_blur_fb);
    bgfx::setViewRect(view_id, 0, 0, m_half_width, m_half_height);

    float params[4] = {
        m_config.max_blur_radius * 0.5f,  // Half res
        static_cast<float>(m_config.bokeh_samples),
        m_config.near_blur_intensity,
        1.0f  // Near field flag
    };
    bgfx::setUniform(u_dof_params, params);

    float texel_size[4] = {
        1.0f / m_half_width,
        1.0f / m_half_height,
        static_cast<float>(m_half_width),
        static_cast<float>(m_half_height)
    };
    bgfx::setUniform(u_texel_size, texel_size);

    bgfx::setTexture(0, s_near, m_near_texture);
    bgfx::setTexture(1, s_coc, m_coc_texture);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(view_id, m_blur_program);
}

void DOFSystem::blur_far(bgfx::ViewId view_id) {
    if (!bgfx::isValid(m_blur_program)) return;

    bgfx::setViewFrameBuffer(view_id, m_far_blur_fb);
    bgfx::setViewRect(view_id, 0, 0, m_half_width, m_half_height);

    float params[4] = {
        m_config.max_blur_radius * 0.5f,
        static_cast<float>(m_config.bokeh_samples),
        m_config.far_blur_intensity,
        0.0f  // Far field flag
    };
    bgfx::setUniform(u_dof_params, params);

    float texel_size[4] = {
        1.0f / m_half_width,
        1.0f / m_half_height,
        static_cast<float>(m_half_width),
        static_cast<float>(m_half_height)
    };
    bgfx::setUniform(u_texel_size, texel_size);

    bgfx::setTexture(0, s_far, m_far_texture);
    bgfx::setTexture(1, s_coc, m_coc_texture);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(view_id, m_blur_program);
}

void DOFSystem::apply_bokeh(bgfx::ViewId view_id, bgfx::TextureHandle color_texture) {
    if (m_config.mode != DOFMode::Bokeh && m_config.mode != DOFMode::BokehSprites) return;
    if (!bgfx::isValid(m_bokeh_program)) return;

    // Bokeh params
    float params[4] = {
        m_config.bokeh_brightness,
        m_config.bokeh_threshold,
        m_config.bokeh_size,
        m_config.bokeh_rotation
    };
    bgfx::setUniform(u_dof_params2, params);

    bgfx::setTexture(0, s_color, color_texture);
    bgfx::setTexture(1, s_coc, m_coc_texture);

    if (bgfx::isValid(m_bokeh_texture)) {
        bgfx::setTexture(2, s_bokeh, m_bokeh_texture);
    }

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ADD);
    bgfx::submit(view_id, m_bokeh_program);
}

void DOFSystem::composite(bgfx::ViewId view_id, bgfx::TextureHandle color_texture) {
    if (!bgfx::isValid(m_composite_program)) return;

    bgfx::setViewFrameBuffer(view_id, m_result_fb);
    bgfx::setViewRect(view_id, 0, 0, m_width, m_height);

    float params[4] = {
        m_config.high_quality_near ? 1.0f : 0.0f,
        m_config.debug_coc ? 1.0f : 0.0f,
        m_config.debug_focus ? 1.0f : 0.0f,
        0.0f
    };
    bgfx::setUniform(u_dof_params, params);

    float texel_size[4] = {
        1.0f / m_width,
        1.0f / m_height,
        static_cast<float>(m_width),
        static_cast<float>(m_height)
    };
    bgfx::setUniform(u_texel_size, texel_size);

    bgfx::setTexture(0, s_color, color_texture);
    bgfx::setTexture(1, s_coc, m_coc_texture);
    bgfx::setTexture(2, s_near_blur, m_near_blur_texture);
    bgfx::setTexture(3, s_far_blur, m_far_blur_texture);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(view_id, m_composite_program);
}

void DOFSystem::render(bgfx::ViewId coc_view,
                        bgfx::ViewId downsample_view,
                        bgfx::ViewId blur_view,
                        bgfx::ViewId composite_view,
                        bgfx::TextureHandle color_texture,
                        bgfx::TextureHandle depth_texture,
                        const Mat4& proj_matrix) {
    if (!m_initialized) return;

    // Calculate CoC
    calculate_coc(coc_view, depth_texture, proj_matrix);

    // Downsample and separate near/far
    downsample(downsample_view, color_texture);

    // Blur near and far (can run in parallel with different views)
    blur_near(blur_view);
    blur_far(blur_view + 1);

    // Apply bokeh if enabled
    if (m_config.mode == DOFMode::Bokeh) {
        apply_bokeh(blur_view + 2, color_texture);
    }

    // Composite
    composite(composite_view, color_texture);
}

} // namespace engine::render
