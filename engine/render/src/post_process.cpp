#include <engine/render/post_process.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <cmath>

namespace engine::render {

using namespace engine::core;

// ============================================================================
// PostProcessSystem implementation
// ============================================================================

PostProcessSystem::~PostProcessSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void PostProcessSystem::init(IRenderer* renderer, const PostProcessConfig& config) {
    m_renderer = renderer;
    m_config = config;
    m_width = renderer->get_width();
    m_height = renderer->get_height();

    // Create bloom mip chain
    if (m_config.bloom.enabled) {
        create_bloom_chain();
    }

    m_initialized = true;
    log(LogLevel::Info, "Post-processing system initialized");
}

void PostProcessSystem::shutdown() {
    if (!m_initialized) return;

    destroy_bloom_chain();

    if (m_luminance_target.valid()) {
        m_renderer->destroy_render_target(m_luminance_target);
    }
    if (m_avg_luminance.valid()) {
        m_renderer->destroy_render_target(m_avg_luminance);
    }

    m_initialized = false;
    m_renderer = nullptr;

    log(LogLevel::Info, "Post-processing system shutdown");
}

void PostProcessSystem::set_config(const PostProcessConfig& config) {
    bool bloom_changed = (config.bloom.enabled != m_config.bloom.enabled ||
                          config.bloom.mip_count != m_config.bloom.mip_count);

    m_config = config;

    if (bloom_changed && m_initialized) {
        destroy_bloom_chain();
        if (m_config.bloom.enabled) {
            create_bloom_chain();
        }
    }
}

void PostProcessSystem::create_bloom_chain() {
    m_bloom_mip_count = std::min(m_config.bloom.mip_count, MAX_BLOOM_MIPS);

    uint32_t w = m_width;
    uint32_t h = m_height;

    for (int i = 0; i < m_bloom_mip_count; ++i) {
        w = std::max(w / 2, 1u);
        h = std::max(h / 2, 1u);

        // Downsample target
        RenderTargetDesc desc;
        desc.width = w;
        desc.height = h;
        desc.color_attachment_count = 1;
        desc.color_format = TextureFormat::RGBA16F;
        desc.has_depth = false;
        desc.samplable = true;
        desc.debug_name = "Bloom_Down";

        m_bloom_downsample[i] = m_renderer->create_render_target(desc);

        // Configure view for this mip
        ViewConfig view_config;
        view_config.render_target = m_bloom_downsample[i];
        view_config.clear_color_enabled = false;
        view_config.clear_depth_enabled = false;

        m_renderer->configure_view(
            static_cast<RenderView>(static_cast<uint16_t>(RenderView::BloomDownsample0) + i),
            view_config
        );

        // Upsample target (same size)
        desc.debug_name = "Bloom_Up";
        m_bloom_upsample[i] = m_renderer->create_render_target(desc);

        view_config.render_target = m_bloom_upsample[i];
        m_renderer->configure_view(
            static_cast<RenderView>(static_cast<uint16_t>(RenderView::BloomUpsample0) + i),
            view_config
        );
    }
}

void PostProcessSystem::destroy_bloom_chain() {
    for (int i = 0; i < MAX_BLOOM_MIPS; ++i) {
        if (m_bloom_downsample[i].valid()) {
            m_renderer->destroy_render_target(m_bloom_downsample[i]);
            m_bloom_downsample[i] = RenderTargetHandle{};
        }
        if (m_bloom_upsample[i].valid()) {
            m_renderer->destroy_render_target(m_bloom_upsample[i]);
            m_bloom_upsample[i] = RenderTargetHandle{};
        }
    }
    m_bloom_mip_count = 0;
}

void PostProcessSystem::render_bloom_downsample(TextureHandle /*input*/, int /*mip*/) {
    // TODO: Bind input texture
    // TODO: Set bloom threshold/scatter uniforms
    // TODO: Queue fullscreen quad draw to appropriate view
}

void PostProcessSystem::render_bloom_upsample(int /*mip*/) {
    // TODO: Bind lower mip and current mip
    // TODO: Set bloom scatter uniform
    // TODO: Queue fullscreen quad draw with additive blend
}

void PostProcessSystem::render_tonemapping(TextureHandle /*scene*/, TextureHandle /*bloom*/) {
    // TODO: Bind scene and bloom textures
    // TODO: Set exposure, tonemap operator, gamma uniforms
    // TODO: Queue fullscreen quad draw to final output
}

void PostProcessSystem::process(
    TextureHandle hdr_scene,
    RenderTargetHandle /*output_target*/
) {
    if (!m_initialized) return;

    // Bloom pass
    TextureHandle bloom_texture;
    if (m_config.bloom.enabled && m_bloom_mip_count > 0) {
        // Downsample chain
        TextureHandle input = hdr_scene;
        for (int i = 0; i < m_bloom_mip_count; ++i) {
            render_bloom_downsample(input, i);
            input = m_renderer->get_render_target_texture(m_bloom_downsample[i], 0);
        }

        // Upsample chain (from smallest to largest)
        for (int i = m_bloom_mip_count - 1; i >= 0; --i) {
            render_bloom_upsample(i);
        }

        bloom_texture = m_renderer->get_render_target_texture(m_bloom_upsample[0], 0);
    }

    // Tone mapping pass
    render_tonemapping(hdr_scene, bloom_texture);
}

void PostProcessSystem::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    if (m_initialized) {
        destroy_bloom_chain();
        if (m_config.bloom.enabled) {
            create_bloom_chain();
        }
    }
}

TextureHandle PostProcessSystem::get_bloom_texture() const {
    if (m_bloom_mip_count > 0 && m_bloom_upsample[0].valid()) {
        return m_renderer->get_render_target_texture(m_bloom_upsample[0], 0);
    }
    return TextureHandle{};
}

// ============================================================================
// TAASystem implementation
// ============================================================================

TAASystem::~TAASystem() {
    if (m_initialized) {
        shutdown();
    }
}

void TAASystem::init(IRenderer* renderer, const TAAConfig& config) {
    m_renderer = renderer;
    m_config = config;
    m_width = renderer->get_width();
    m_height = renderer->get_height();

    // Generate Halton jitter sequence
    for (int i = 0; i < JITTER_SAMPLES; ++i) {
        m_jitter_sequence[i] = Vec2(
            halton(i + 1, 2) - 0.5f,
            halton(i + 1, 3) - 0.5f
        );
    }

    create_history_buffers();

    m_initialized = true;
    log(LogLevel::Info, "TAA system initialized");
}

void TAASystem::shutdown() {
    if (!m_initialized) return;

    destroy_history_buffers();

    m_initialized = false;
    m_renderer = nullptr;

    log(LogLevel::Info, "TAA system shutdown");
}

void TAASystem::set_config(const TAAConfig& config) {
    m_config = config;
}

Vec2 TAASystem::get_jitter(uint32_t frame_index) const {
    if (!m_config.enabled) {
        return Vec2(0.0f);
    }

    int idx = frame_index % JITTER_SAMPLES;
    Vec2 jitter = m_jitter_sequence[idx] * m_config.jitter_scale;

    // Scale jitter to pixel size
    jitter.x /= static_cast<float>(m_width);
    jitter.y /= static_cast<float>(m_height);

    return jitter;
}

TextureHandle TAASystem::resolve(
    TextureHandle /*current_frame*/,
    TextureHandle /*depth_texture*/,
    TextureHandle /*motion_vectors*/
) {
    // TODO: Implement TAA resolve
    // 1. Reproject history using motion vectors
    // 2. Sample and clamp history (neighborhood clamping in YCoCg)
    // 3. Blend current and history based on velocity
    // 4. Output to history buffer

    // Swap history buffers
    m_history_index = 1 - m_history_index;
    m_frame_count++;

    return m_renderer->get_render_target_texture(m_history[m_history_index], 0);
}

void TAASystem::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    if (m_initialized) {
        destroy_history_buffers();
        create_history_buffers();
    }
}

void TAASystem::create_history_buffers() {
    RenderTargetDesc desc;
    desc.width = m_width;
    desc.height = m_height;
    desc.color_attachment_count = 1;
    desc.color_format = TextureFormat::RGBA16F;
    desc.has_depth = false;
    desc.samplable = true;
    desc.debug_name = "TAA_History";

    m_history[0] = m_renderer->create_render_target(desc);
    m_history[1] = m_renderer->create_render_target(desc);

    // Configure TAA view
    ViewConfig view_config;
    view_config.render_target = m_history[0];
    view_config.clear_color_enabled = true;
    view_config.clear_color = 0x00000000;
    view_config.clear_depth_enabled = false;

    m_renderer->configure_view(RenderView::TAA, view_config);
}

void TAASystem::destroy_history_buffers() {
    for (int i = 0; i < 2; ++i) {
        if (m_history[i].valid()) {
            m_renderer->destroy_render_target(m_history[i]);
            m_history[i] = RenderTargetHandle{};
        }
    }
}

float TAASystem::halton(int index, int base) {
    float f = 1.0f;
    float r = 0.0f;

    while (index > 0) {
        f = f / static_cast<float>(base);
        r = r + f * static_cast<float>(index % base);
        index = index / base;
    }

    return r;
}

} // namespace engine::render
