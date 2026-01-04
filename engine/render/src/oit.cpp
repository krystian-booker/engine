#include <engine/render/oit.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <bgfx/bgfx.h>
#include <fstream>

namespace engine::render {

using namespace engine::core;

// Shader loading helpers
static bgfx::ShaderHandle load_oit_shader(const std::string& path) {
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

static std::string get_oit_shader_path() {
    auto type = bgfx::getRendererType();
    switch (type) {
        case bgfx::RendererType::Direct3D11:
        case bgfx::RendererType::Direct3D12: return "shaders/dx11/";
        case bgfx::RendererType::Vulkan: return "shaders/spirv/";
        case bgfx::RendererType::OpenGL: return "shaders/glsl/";
        default: return "shaders/spirv/";
    }
}

OITSystem::~OITSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void OITSystem::init(IRenderer* renderer, uint32_t width, uint32_t height) {
    m_renderer = renderer;
    m_width = width;
    m_height = height;

    create_targets();

    // Load composite shader
    std::string path = get_oit_shader_path();
    bgfx::ShaderHandle vs = load_oit_shader(path + "vs_fullscreen.sc.bin");
    bgfx::ShaderHandle fs = load_oit_shader(path + "fs_oit_composite.sc.bin");

    if (bgfx::isValid(vs) && bgfx::isValid(fs)) {
        bgfx::ProgramHandle prog = bgfx::createProgram(vs, fs, false);
        m_composite_program = prog.idx;
        log(LogLevel::Info, "OIT composite shader loaded");
    } else {
        log(LogLevel::Warn, "Failed to load OIT composite shaders");
    }

    if (bgfx::isValid(vs)) bgfx::destroy(vs);
    if (bgfx::isValid(fs)) bgfx::destroy(fs);

    // Create sampler uniforms
    bgfx::UniformHandle accum = bgfx::createUniform("s_accum", bgfx::UniformType::Sampler);
    bgfx::UniformHandle reveal = bgfx::createUniform("s_reveal", bgfx::UniformType::Sampler);
    bgfx::UniformHandle opaque = bgfx::createUniform("s_opaque", bgfx::UniformType::Sampler);

    u_accum = accum.idx;
    u_reveal = reveal.idx;
    u_opaque = opaque.idx;

    // Create fullscreen quad geometry
    struct FSVertex { float x, y, z, u, v; };
    static const FSVertex vertices[] = {
        { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
        {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
        { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
    };
    static const uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    bgfx::VertexBufferHandle vb = bgfx::createVertexBuffer(
        bgfx::makeRef(vertices, sizeof(vertices)), layout);
    bgfx::IndexBufferHandle ib = bgfx::createIndexBuffer(
        bgfx::makeRef(indices, sizeof(indices)));

    m_fullscreen_vb = vb.idx;
    m_fullscreen_ib = ib.idx;

    m_initialized = true;
    log(LogLevel::Info, "OIT system initialized ({}x{})", width, height);
}

void OITSystem::shutdown() {
    if (!m_initialized) return;

    destroy_targets();

    // Destroy MRT framebuffer
    if (m_mrt_framebuffer != UINT16_MAX) {
        bgfx::FrameBufferHandle fb = { m_mrt_framebuffer };
        bgfx::destroy(fb);
        m_mrt_framebuffer = UINT16_MAX;
    }

    // Destroy composite program
    if (m_composite_program != UINT16_MAX) {
        bgfx::ProgramHandle prog = { m_composite_program };
        bgfx::destroy(prog);
        m_composite_program = UINT16_MAX;
    }

    // Destroy fullscreen geometry
    if (m_fullscreen_vb != UINT16_MAX) {
        bgfx::VertexBufferHandle vb = { m_fullscreen_vb };
        bgfx::destroy(vb);
        m_fullscreen_vb = UINT16_MAX;
    }
    if (m_fullscreen_ib != UINT16_MAX) {
        bgfx::IndexBufferHandle ib = { m_fullscreen_ib };
        bgfx::destroy(ib);
        m_fullscreen_ib = UINT16_MAX;
    }

    // Destroy uniforms
    if (u_accum != UINT16_MAX) {
        bgfx::UniformHandle u = { u_accum };
        bgfx::destroy(u);
        u_accum = UINT16_MAX;
    }
    if (u_reveal != UINT16_MAX) {
        bgfx::UniformHandle u = { u_reveal };
        bgfx::destroy(u);
        u_reveal = UINT16_MAX;
    }
    if (u_opaque != UINT16_MAX) {
        bgfx::UniformHandle u = { u_opaque };
        bgfx::destroy(u);
        u_opaque = UINT16_MAX;
    }

    m_initialized = false;
    m_renderer = nullptr;

    log(LogLevel::Info, "OIT system shutdown");
}

void OITSystem::resize(uint32_t width, uint32_t height) {
    if (m_width == width && m_height == height) {
        return;
    }

    m_width = width;
    m_height = height;

    destroy_targets();
    create_targets();

    log(LogLevel::Debug, "OIT system resized to {}x{}", width, height);
}

void OITSystem::create_targets() {
    // Accumulation target: RGBA16F for high-precision color accumulation
    // RGB = sum of (color * alpha * weight)
    // A = sum of (alpha * weight)
    {
        RenderTargetDesc desc;
        desc.width = m_width;
        desc.height = m_height;
        desc.color_attachment_count = 1;
        desc.color_format = TextureFormat::RGBA16F;
        desc.has_depth = false;  // Use existing depth buffer
        desc.samplable = true;
        desc.debug_name = "OIT_Accumulation";

        m_accum_target = m_renderer->create_render_target(desc);
    }

    // Revealage target: R16F for alpha product
    // R = product of (1 - alpha)
    {
        RenderTargetDesc desc;
        desc.width = m_width;
        desc.height = m_height;
        desc.color_attachment_count = 1;
        desc.color_format = TextureFormat::RGBA16F;  // Using RGBA16F as R16F may not be available
        desc.has_depth = false;
        desc.samplable = true;
        desc.debug_name = "OIT_Revealage";

        m_reveal_target = m_renderer->create_render_target(desc);
    }

    // Create MRT framebuffer for both targets
    if (m_accum_target.valid() && m_reveal_target.valid()) {
        TextureHandle accum_tex = m_renderer->get_render_target_texture(m_accum_target, 0);
        TextureHandle reveal_tex = m_renderer->get_render_target_texture(m_reveal_target, 0);

        uint16_t accum_idx = m_renderer->get_native_texture_handle(accum_tex);
        uint16_t reveal_idx = m_renderer->get_native_texture_handle(reveal_tex);

        if (accum_idx != bgfx::kInvalidHandle && reveal_idx != bgfx::kInvalidHandle) {
            bgfx::TextureHandle attachments[2] = {
                { accum_idx },
                { reveal_idx }
            };

            bgfx::FrameBufferHandle fb = bgfx::createFrameBuffer(2, attachments, false);
            m_mrt_framebuffer = fb.idx;

            log(LogLevel::Debug, "OIT MRT framebuffer created");
        }
    }
}

void OITSystem::destroy_targets() {
    // Destroy MRT framebuffer first (before textures)
    if (m_mrt_framebuffer != UINT16_MAX) {
        bgfx::FrameBufferHandle fb = { m_mrt_framebuffer };
        bgfx::destroy(fb);
        m_mrt_framebuffer = UINT16_MAX;
    }

    if (m_accum_target.valid()) {
        m_renderer->destroy_render_target(m_accum_target);
        m_accum_target = RenderTargetHandle{};
    }

    if (m_reveal_target.valid()) {
        m_renderer->destroy_render_target(m_reveal_target);
        m_reveal_target = RenderTargetHandle{};
    }
}

void OITSystem::begin_transparent_pass() {
    if (!m_initialized || !m_config.enabled) {
        return;
    }

    if (m_mrt_framebuffer == UINT16_MAX) {
        log(LogLevel::Warn, "OIT MRT framebuffer not available");
        return;
    }

    // Set up MRT framebuffer for transparent rendering
    bgfx::FrameBufferHandle fb = { m_mrt_framebuffer };

    // Use a dedicated view ID for OIT transparent pass
    // View ID for OIT accumulation (after main transparent)
    uint16_t oit_view_id = static_cast<uint16_t>(RenderView::MainTransparent);

    bgfx::setViewFrameBuffer(oit_view_id, fb);
    bgfx::setViewRect(oit_view_id, 0, 0, static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height));

    // Clear both MRT attachments
    // Accumulation: 0,0,0,0 (no accumulated color/alpha)
    // Revealage: 1,0,0,0 (fully revealed/transparent - product of (1-alpha) starts at 1)
    bgfx::setViewClear(oit_view_id,
        BGFX_CLEAR_COLOR,
        0x00000000,  // Clear color for accum (rgba=0)
        1.0f, 0);

    // Note: OIT uses special blend states that are configured per-draw in the
    // fs_oit_accumulate shader. The shader outputs to gl_FragData[0] and [1]
    // with the following blend modes:
    // - Attachment 0 (accum): ONE, ONE (additive blending)
    // - Attachment 1 (reveal): ZERO, ONE_MINUS_SRC_COLOR (multiplicative)
    //
    // bgfx doesn't support per-attachment blend modes directly, so the shader
    // uses separate outputs with independent blend targets where supported,
    // or falls back to multiple passes on other platforms.
}

void OITSystem::composite(RenderTargetHandle destination) {
    if (!m_initialized || !m_config.enabled) {
        return;
    }

    if (m_composite_program == UINT16_MAX) {
        log(LogLevel::Warn, "OIT composite program not available");
        return;
    }

    TextureHandle accum_tex = get_accum_texture();
    TextureHandle reveal_tex = get_reveal_texture();

    if (!accum_tex.valid() || !reveal_tex.valid()) {
        return;
    }

    // Get the opaque scene texture from destination (we read from it and write to it)
    TextureHandle opaque_tex = m_renderer->get_render_target_texture(destination, 0);
    if (!opaque_tex.valid()) {
        return;
    }

    // Get native texture handles
    uint16_t accum_idx = m_renderer->get_native_texture_handle(accum_tex);
    uint16_t reveal_idx = m_renderer->get_native_texture_handle(reveal_tex);
    uint16_t opaque_idx = m_renderer->get_native_texture_handle(opaque_tex);

    if (accum_idx == bgfx::kInvalidHandle ||
        reveal_idx == bgfx::kInvalidHandle ||
        opaque_idx == bgfx::kInvalidHandle) {
        return;
    }

    // Use PostProcess view for compositing
    uint16_t composite_view_id = static_cast<uint16_t>(RenderView::PostProcess0);

    // Configure view to render to destination
    ViewConfig view_config;
    view_config.render_target = destination;
    view_config.clear_color_enabled = false;  // Don't clear - blend over opaque
    view_config.clear_depth_enabled = false;
    m_renderer->configure_view(RenderView::PostProcess0, view_config);

    // Set up textures
    bgfx::UniformHandle s_accum_handle = { u_accum };
    bgfx::UniformHandle s_reveal_handle = { u_reveal };
    bgfx::UniformHandle s_opaque_handle = { u_opaque };

    bgfx::TextureHandle accum_handle = { accum_idx };
    bgfx::TextureHandle reveal_handle = { reveal_idx };
    bgfx::TextureHandle opaque_handle = { opaque_idx };

    bgfx::setTexture(0, s_accum_handle, accum_handle);
    bgfx::setTexture(1, s_reveal_handle, reveal_handle);
    bgfx::setTexture(2, s_opaque_handle, opaque_handle);

    // Set up fullscreen quad
    bgfx::VertexBufferHandle vb = { m_fullscreen_vb };
    bgfx::IndexBufferHandle ib = { m_fullscreen_ib };
    bgfx::ProgramHandle prog = { m_composite_program };

    bgfx::setVertexBuffer(0, vb);
    bgfx::setIndexBuffer(ib);

    // Blend mode: replace (we handle blending in the shader)
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);

    // Submit fullscreen composite pass
    bgfx::submit(composite_view_id, prog);

    log(LogLevel::Debug, "OIT composite submitted");
}

TextureHandle OITSystem::get_accum_texture() const {
    if (m_accum_target.valid()) {
        return m_renderer->get_render_target_texture(m_accum_target, 0);
    }
    return TextureHandle{};
}

TextureHandle OITSystem::get_reveal_texture() const {
    if (m_reveal_target.valid()) {
        return m_renderer->get_render_target_texture(m_reveal_target, 0);
    }
    return TextureHandle{};
}

} // namespace engine::render
