#include <engine/render/post_process.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <bgfx/bgfx.h>
#include <cmath>
#include <fstream>

namespace engine::render {

using namespace engine::core;

// Post-process shader programs and uniforms
struct PostProcessShaders {
    bgfx::ProgramHandle bloom_downsample = BGFX_INVALID_HANDLE;
    // TODO: Implement specialized first-pass bloom downsample with Karis average
    // to prevent firefly artifacts from bright sub-pixel features.
    bgfx::ProgramHandle bloom_downsample_first = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle bloom_upsample = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle tonemap = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle taa = BGFX_INVALID_HANDLE;

    // Uniforms
    bgfx::UniformHandle u_bloomParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texelSize = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_tonemapParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_vignetteParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_taaParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_source = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_highRes = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_scene = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_bloom = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_current = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_history = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_depth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_velocity = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_volumetric = BGFX_INVALID_HANDLE;

    // Fullscreen quad vertex buffer
    bgfx::VertexBufferHandle fullscreen_vb = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle fullscreen_ib = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout fullscreen_layout;

    void create();
    void destroy();
    void draw_fullscreen(uint16_t view_id, bgfx::ProgramHandle program);
};

static PostProcessShaders s_pp_shaders;

static bgfx::ShaderHandle load_shader(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return BGFX_INVALID_HANDLE;
    }
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

static std::string get_shader_path() {
    auto type = bgfx::getRendererType();
    switch (type) {
        case bgfx::RendererType::Direct3D11:
        case bgfx::RendererType::Direct3D12: return "shaders/dx11/";
        case bgfx::RendererType::Vulkan: return "shaders/spirv/";
        case bgfx::RendererType::OpenGL: return "shaders/glsl/";
        default: return "shaders/spirv/";
    }
}

void PostProcessShaders::create() {
    std::string path = get_shader_path();

    // Load fullscreen vertex shader
    bgfx::ShaderHandle vs = load_shader(path + "vs_fullscreen.sc.bin");

    // Load fragment shaders
    bgfx::ShaderHandle fs_bloom_down = load_shader(path + "fs_bloom_downsample.sc.bin");
    bgfx::ShaderHandle fs_bloom_up = load_shader(path + "fs_bloom_upsample.sc.bin");
    bgfx::ShaderHandle fs_tonemap = load_shader(path + "fs_tonemap.sc.bin");
    bgfx::ShaderHandle fs_taa = load_shader(path + "fs_taa.sc.bin");

    // Create programs
    if (bgfx::isValid(vs)) {
        if (bgfx::isValid(fs_bloom_down)) {
            bloom_downsample = bgfx::createProgram(vs, fs_bloom_down, false);
        }
        if (bgfx::isValid(fs_bloom_up)) {
            bloom_upsample = bgfx::createProgram(vs, fs_bloom_up, false);
        }
        if (bgfx::isValid(fs_tonemap)) {
            tonemap = bgfx::createProgram(vs, fs_tonemap, false);
        }
        if (bgfx::isValid(fs_taa)) {
            taa = bgfx::createProgram(vs, fs_taa, false);
        }
    }

    // Destroy individual shaders (programs keep copies)
    if (bgfx::isValid(vs)) bgfx::destroy(vs);
    if (bgfx::isValid(fs_bloom_down)) bgfx::destroy(fs_bloom_down);
    if (bgfx::isValid(fs_bloom_up)) bgfx::destroy(fs_bloom_up);
    if (bgfx::isValid(fs_tonemap)) bgfx::destroy(fs_tonemap);
    if (bgfx::isValid(fs_taa)) bgfx::destroy(fs_taa);

    // Create uniforms
    u_bloomParams = bgfx::createUniform("u_bloomParams", bgfx::UniformType::Vec4);
    u_texelSize = bgfx::createUniform("u_texelSize", bgfx::UniformType::Vec4);
    u_tonemapParams = bgfx::createUniform("u_tonemapParams", bgfx::UniformType::Vec4);
    u_vignetteParams = bgfx::createUniform("u_vignetteParams", bgfx::UniformType::Vec4);
    u_taaParams = bgfx::createUniform("u_taaParams", bgfx::UniformType::Vec4);
    s_source = bgfx::createUniform("s_source", bgfx::UniformType::Sampler);
    s_highRes = bgfx::createUniform("s_highRes", bgfx::UniformType::Sampler);
    s_scene = bgfx::createUniform("s_scene", bgfx::UniformType::Sampler);
    s_bloom = bgfx::createUniform("s_bloom", bgfx::UniformType::Sampler);
    s_current = bgfx::createUniform("s_current", bgfx::UniformType::Sampler);
    s_history = bgfx::createUniform("s_history", bgfx::UniformType::Sampler);
    s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);
    s_velocity = bgfx::createUniform("s_velocity", bgfx::UniformType::Sampler);
    s_volumetric = bgfx::createUniform("s_volumetric", bgfx::UniformType::Sampler);

    // Create fullscreen quad vertex layout
    fullscreen_layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    // Create fullscreen quad vertices (-1 to 1 clip space, 0 to 1 UV)
    struct FSVertex { float x, y, z, u, v; };
    static const FSVertex vertices[] = {
        { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
        {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
        { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
    };

    static const uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };

    fullscreen_vb = bgfx::createVertexBuffer(
        bgfx::makeRef(vertices, sizeof(vertices)),
        fullscreen_layout
    );
    fullscreen_ib = bgfx::createIndexBuffer(
        bgfx::makeRef(indices, sizeof(indices))
    );

    log(LogLevel::Info, "Post-process shaders initialized");
}

void PostProcessShaders::destroy() {
    if (bgfx::isValid(bloom_downsample)) bgfx::destroy(bloom_downsample);
    if (bgfx::isValid(bloom_downsample_first)) bgfx::destroy(bloom_downsample_first);
    if (bgfx::isValid(bloom_upsample)) bgfx::destroy(bloom_upsample);
    if (bgfx::isValid(tonemap)) bgfx::destroy(tonemap);
    if (bgfx::isValid(taa)) bgfx::destroy(taa);

    if (bgfx::isValid(u_bloomParams)) bgfx::destroy(u_bloomParams);
    if (bgfx::isValid(u_texelSize)) bgfx::destroy(u_texelSize);
    if (bgfx::isValid(u_tonemapParams)) bgfx::destroy(u_tonemapParams);
    if (bgfx::isValid(u_vignetteParams)) bgfx::destroy(u_vignetteParams);
    if (bgfx::isValid(u_taaParams)) bgfx::destroy(u_taaParams);
    if (bgfx::isValid(s_source)) bgfx::destroy(s_source);
    if (bgfx::isValid(s_highRes)) bgfx::destroy(s_highRes);
    if (bgfx::isValid(s_scene)) bgfx::destroy(s_scene);
    if (bgfx::isValid(s_bloom)) bgfx::destroy(s_bloom);
    if (bgfx::isValid(s_current)) bgfx::destroy(s_current);
    if (bgfx::isValid(s_history)) bgfx::destroy(s_history);
    if (bgfx::isValid(s_depth)) bgfx::destroy(s_depth);
    if (bgfx::isValid(s_velocity)) bgfx::destroy(s_velocity);
    if (bgfx::isValid(s_volumetric)) bgfx::destroy(s_volumetric);

    if (bgfx::isValid(fullscreen_vb)) bgfx::destroy(fullscreen_vb);
    if (bgfx::isValid(fullscreen_ib)) bgfx::destroy(fullscreen_ib);

    bloom_downsample = BGFX_INVALID_HANDLE;
    bloom_downsample_first = BGFX_INVALID_HANDLE;
    bloom_upsample = BGFX_INVALID_HANDLE;
    tonemap = BGFX_INVALID_HANDLE;
    taa = BGFX_INVALID_HANDLE;
}

void PostProcessShaders::draw_fullscreen(uint16_t view_id, bgfx::ProgramHandle program) {
    if (!bgfx::isValid(program) || !bgfx::isValid(fullscreen_vb)) {
        return;
    }

    bgfx::setVertexBuffer(0, fullscreen_vb);
    bgfx::setIndexBuffer(fullscreen_ib);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(view_id, program);
}

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

    // Create shaders (shared across all PostProcessSystem instances)
    static bool shaders_created = false;
    if (!shaders_created) {
        s_pp_shaders.create();
        shaders_created = true;
    }

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

    // Destroy static shader resources before bgfx shutdown to avoid
    // dangling handle destruction in static destructors.
    s_pp_shaders.destroy();

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

void PostProcessSystem::render_bloom_downsample(TextureHandle input, int mip) {
    if (!bgfx::isValid(s_pp_shaders.bloom_downsample)) return;

    // Get the view for this mip level
    uint16_t view_id = static_cast<uint16_t>(RenderView::BloomDownsample0) + mip;

    // Get native texture handle
    uint16_t tex_idx = m_renderer->get_native_texture_handle(input);
    if (tex_idx == bgfx::kInvalidHandle) return;

    bgfx::TextureHandle tex_handle = { tex_idx };

    // Input texture for downsample pass N is the output of pass N-1
    // For pass 0, input is full-res HDR; pass N reads from mip N (size = m_width >> mip)
    uint32_t input_width = m_width >> mip;
    uint32_t input_height = m_height >> mip;

    Vec4 texel_size(1.0f / float(input_width), 1.0f / float(input_height), 0.0f, 0.0f);
    Vec4 bloom_params(m_config.bloom.threshold, m_config.bloom.threshold * 0.5f, 0.0f, 0.0f);

    // Set uniforms
    bgfx::setUniform(s_pp_shaders.u_texelSize, &texel_size);
    bgfx::setUniform(s_pp_shaders.u_bloomParams, &bloom_params);

    // Bind input texture
    bgfx::setTexture(0, s_pp_shaders.s_source, tex_handle);

    // Draw fullscreen quad
    s_pp_shaders.draw_fullscreen(view_id, s_pp_shaders.bloom_downsample);
}

void PostProcessSystem::render_bloom_upsample(int mip) {
    if (!bgfx::isValid(s_pp_shaders.bloom_upsample)) return;

    // Get the view for this mip level
    uint16_t view_id = static_cast<uint16_t>(RenderView::BloomUpsample0) + mip;

    // Get source (lower resolution mip from downsample chain or previous upsample)
    TextureHandle source_tex;
    if (mip == m_bloom_mip_count - 1) {
        // Smallest mip - source is the last downsample
        source_tex = m_renderer->get_render_target_texture(m_bloom_downsample[mip], 0);
    } else {
        // Source is the previous (smaller) upsample result
        source_tex = m_renderer->get_render_target_texture(m_bloom_upsample[mip + 1], 0);
    }

    // Get high-res source from downsample chain
    TextureHandle highres_tex = m_renderer->get_render_target_texture(m_bloom_downsample[mip], 0);

    uint16_t source_idx = m_renderer->get_native_texture_handle(source_tex);
    uint16_t highres_idx = m_renderer->get_native_texture_handle(highres_tex);

    if (source_idx == bgfx::kInvalidHandle) return;

    bgfx::TextureHandle source_handle = { source_idx };
    bgfx::TextureHandle highres_handle = { highres_idx };

    // Calculate texel size
    uint32_t target_width = m_width >> (mip + 1);
    uint32_t target_height = m_height >> (mip + 1);
    Vec4 texel_size(1.0f / float(target_width), 1.0f / float(target_height), 0.0f, 0.0f);
    Vec4 bloom_params(m_config.bloom.scatter, m_config.bloom.intensity, 0.0f, 0.0f);

    // Set uniforms
    bgfx::setUniform(s_pp_shaders.u_texelSize, &texel_size);
    bgfx::setUniform(s_pp_shaders.u_bloomParams, &bloom_params);

    // Bind textures
    bgfx::setTexture(0, s_pp_shaders.s_source, source_handle);
    if (highres_idx != bgfx::kInvalidHandle) {
        bgfx::setTexture(1, s_pp_shaders.s_highRes, highres_handle);
    }

    // Draw fullscreen quad
    s_pp_shaders.draw_fullscreen(view_id, s_pp_shaders.bloom_upsample);
}

void PostProcessSystem::render_tonemapping(TextureHandle scene, TextureHandle bloom) {
    if (!bgfx::isValid(s_pp_shaders.tonemap)) return;

    // Use the final view for tonemapping
    uint16_t view_id = static_cast<uint16_t>(RenderView::Tonemapping);

    uint16_t scene_idx = m_renderer->get_native_texture_handle(scene);
    if (scene_idx == bgfx::kInvalidHandle) return;

    bgfx::TextureHandle scene_handle = { scene_idx };

    // Get tonemap operator as float
    float tonemap_op = 3.0f;  // ACES default
    switch (m_config.tonemapping.op) {
        case ToneMappingOperator::None: tonemap_op = 0.0f; break;
        case ToneMappingOperator::Reinhard: tonemap_op = 1.0f; break;
        case ToneMappingOperator::ReinhardExtended: tonemap_op = 2.0f; break;
        case ToneMappingOperator::ACES: tonemap_op = 3.0f; break;
        case ToneMappingOperator::Uncharted2: tonemap_op = 4.0f; break;
        case ToneMappingOperator::AgX: tonemap_op = 5.0f; break;
    }

    Vec4 tonemap_params(
        m_config.tonemapping.exposure,
        m_config.tonemapping.gamma,
        m_config.tonemapping.white_point,
        tonemap_op
    );

    Vec4 bloom_params(m_config.bloom.intensity, 0.0f, 0.0f, 0.0f);

    Vec4 vignette_params(
        m_config.vignette_enabled ? 1.0f : 0.0f,
        m_config.vignette_intensity,
        m_config.vignette_smoothness,
        0.0f
    );

    // Set uniforms
    bgfx::setUniform(s_pp_shaders.u_tonemapParams, &tonemap_params);
    bgfx::setUniform(s_pp_shaders.u_bloomParams, &bloom_params);
    bgfx::setUniform(s_pp_shaders.u_vignetteParams, &vignette_params);

    // Bind scene texture
    bgfx::setTexture(0, s_pp_shaders.s_scene, scene_handle);

    // Bind bloom texture if available
    if (bloom.valid()) {
        uint16_t bloom_idx = m_renderer->get_native_texture_handle(bloom);
        if (bloom_idx != bgfx::kInvalidHandle) {
            bgfx::TextureHandle bloom_handle = { bloom_idx };
            bgfx::setTexture(1, s_pp_shaders.s_bloom, bloom_handle);
        }
    }

    // Bind volumetric fog texture if available
    // The shader composites: final = scene * vol.a + vol.rgb
    if (m_volumetric_texture.valid()) {
        uint16_t vol_idx = m_renderer->get_native_texture_handle(m_volumetric_texture);
        if (vol_idx != bgfx::kInvalidHandle) {
            bgfx::TextureHandle vol_handle = { vol_idx };
            bgfx::setTexture(2, s_pp_shaders.s_volumetric, vol_handle);
        }
    }

    // Draw fullscreen quad
    s_pp_shaders.draw_fullscreen(view_id, s_pp_shaders.tonemap);

    // Clear the volumetric texture for next frame
    m_volumetric_texture = TextureHandle{};
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

    // Return jitter in pixel units — the pipeline converts to clip space
    // via: jitter * 2.0f / resolution
    return jitter;
}

TextureHandle TAASystem::resolve(
    TextureHandle current_frame,
    TextureHandle depth_texture,
    TextureHandle motion_vectors
) {
    if (!m_initialized || !bgfx::isValid(s_pp_shaders.taa)) {
        return current_frame;  // Fallback to current frame if not initialized
    }

    // Get the output history buffer (the one we'll write to)
    int write_index = 1 - m_history_index;
    RenderTargetHandle output_target = m_history[write_index];

    if (!output_target.valid()) {
        return current_frame;
    }

    // Configure view to render into the output history buffer
    ViewConfig view_config;
    view_config.render_target = output_target;
    view_config.clear_color_enabled = false;
    view_config.clear_depth_enabled = false;
    m_renderer->configure_view(RenderView::TAA, view_config);

    uint16_t view_id = static_cast<uint16_t>(RenderView::TAA);

    // Get native texture handles
    uint16_t current_idx = m_renderer->get_native_texture_handle(current_frame);
    if (current_idx == bgfx::kInvalidHandle) {
        return current_frame;
    }

    bgfx::TextureHandle current_handle = { current_idx };

    // Get history texture (the one we read from)
    TextureHandle history_tex = m_renderer->get_render_target_texture(m_history[m_history_index], 0);
    uint16_t history_idx = m_renderer->get_native_texture_handle(history_tex);
    bgfx::TextureHandle history_handle = { history_idx };

    // Get depth texture handle
    uint16_t depth_idx = m_renderer->get_native_texture_handle(depth_texture);
    bgfx::TextureHandle depth_handle = { depth_idx };

    // Check if we have motion vectors
    bool has_motion_vectors = motion_vectors.valid();
    bgfx::TextureHandle velocity_handle = BGFX_INVALID_HANDLE;
    if (has_motion_vectors) {
        uint16_t velocity_idx = m_renderer->get_native_texture_handle(motion_vectors);
        velocity_handle = { velocity_idx };
    }

    // Set TAA parameters
    // x=blend_factor (average of feedback min/max), y=motion_scale, z=use_velocity, w=use_catmull_rom
    float blend_factor = (m_config.feedback_min + m_config.feedback_max) * 0.5f;
    Vec4 taa_params(
        blend_factor,
        1.0f,  // motion_scale - default value
        has_motion_vectors ? 1.0f : 0.0f,
        1.0f   // Always use Catmull-Rom for sharper history sampling
    );

    // Texel size for sampling
    Vec4 texel_size(
        1.0f / static_cast<float>(m_width),
        1.0f / static_cast<float>(m_height),
        static_cast<float>(m_width),
        static_cast<float>(m_height)
    );

    // Set uniforms
    bgfx::setUniform(s_pp_shaders.u_taaParams, &taa_params);
    bgfx::setUniform(s_pp_shaders.u_texelSize, &texel_size);

    // Bind textures
    bgfx::setTexture(0, s_pp_shaders.s_current, current_handle);
    if (bgfx::isValid(history_handle)) {
        bgfx::setTexture(1, s_pp_shaders.s_history, history_handle);
    }
    if (bgfx::isValid(depth_handle)) {
        bgfx::setTexture(2, s_pp_shaders.s_depth, depth_handle);
    }
    if (bgfx::isValid(velocity_handle)) {
        bgfx::setTexture(3, s_pp_shaders.s_velocity, velocity_handle);
    }

    // Draw fullscreen quad
    s_pp_shaders.draw_fullscreen(view_id, s_pp_shaders.taa);

    // Swap history buffers for next frame
    m_history_index = write_index;
    m_frame_count++;

    // Return the texture we just wrote to
    return m_renderer->get_render_target_texture(output_target, 0);
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
