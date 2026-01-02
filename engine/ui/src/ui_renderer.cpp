#include <engine/ui/ui_renderer.hpp>
#include <engine/core/log.hpp>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <cmath>
#include <fstream>
#include <string>

namespace engine::ui {

namespace {

// Helper function to load shader binary from file
bgfx::ShaderHandle load_shader_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        core::log(core::LogLevel::Error, "UIRenderer: Failed to open shader file: {}", path);
        return BGFX_INVALID_HANDLE;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
    file.read(reinterpret_cast<char*>(mem->data), size);
    mem->data[size] = '\0';

    return bgfx::createShader(mem);
}

// Get shader path based on renderer type
std::string get_shader_path() {
    bgfx::RendererType::Enum renderer = bgfx::getRendererType();
    std::string shader_dir = "shaders/";

    switch (renderer) {
        case bgfx::RendererType::Direct3D11:
        case bgfx::RendererType::Direct3D12:
            shader_dir += "dx11/";
            break;
        case bgfx::RendererType::OpenGL:
            shader_dir += "glsl/";
            break;
        case bgfx::RendererType::Vulkan:
            shader_dir += "spirv/";
            break;
        case bgfx::RendererType::Metal:
            shader_dir += "metal/";
            break;
        default:
            shader_dir += "dx11/";
            break;
    }

    return shader_dir;
}

} // anonymous namespace

// UIRenderContext implementation

UIRenderContext::UIRenderContext() = default;
UIRenderContext::~UIRenderContext() = default;

void UIRenderContext::begin(uint32_t screen_width, uint32_t screen_height) {
    m_vertices.clear();
    m_indices.clear();
    m_commands.clear();
    m_clip_stack.clear();

    m_screen_width = screen_width;
    m_screen_height = screen_height;
    m_current_texture = 0;
    m_current_is_text = false;

    // Start with full screen clip rect
    m_clip_stack.push_back(Rect(0, 0,
        static_cast<float>(screen_width),
        static_cast<float>(screen_height)));
}

void UIRenderContext::end() {
    // Finalize any pending batch
    flush_batch();
}

void UIRenderContext::push_clip_rect(const Rect& rect) {
    Rect clipped = rect;
    if (!m_clip_stack.empty()) {
        clipped = m_clip_stack.back().intersect(rect);
    }
    m_clip_stack.push_back(clipped);
    flush_batch();  // Need new command for new clip rect
}

void UIRenderContext::pop_clip_rect() {
    if (m_clip_stack.size() > 1) {
        m_clip_stack.pop_back();
        flush_batch();
    }
}

const Rect& UIRenderContext::get_clip_rect() const {
    static Rect default_rect;
    return m_clip_stack.empty() ? default_rect : m_clip_stack.back();
}

void UIRenderContext::draw_rect(const Rect& rect, const Vec4& color) {
    new_command(0);

    uint32_t packed_color = pack_color(color);
    uint32_t base = static_cast<uint32_t>(m_vertices.size());

    add_rect_vertices(rect, Vec2(0, 0), Vec2(1, 1), packed_color);

    m_indices.push_back(base + 0);
    m_indices.push_back(base + 1);
    m_indices.push_back(base + 2);
    m_indices.push_back(base + 2);
    m_indices.push_back(base + 3);
    m_indices.push_back(base + 0);

    m_commands.back().vertex_count += 4;
    m_commands.back().index_count += 6;
}

void UIRenderContext::draw_rect_rounded(const Rect& rect, const Vec4& color, float radius) {
    if (radius <= 0.0f) {
        draw_rect(rect, color);
        return;
    }

    new_command(0);

    uint32_t packed_color = pack_color(color);
    add_rounded_rect_vertices(rect, radius, packed_color);
}

void UIRenderContext::draw_rect_outline(const Rect& rect, const Vec4& color, float thickness) {
    // Draw as 4 thin rectangles
    // Top
    draw_rect(Rect(rect.x, rect.y, rect.width, thickness), color);
    // Bottom
    draw_rect(Rect(rect.x, rect.bottom() - thickness, rect.width, thickness), color);
    // Left
    draw_rect(Rect(rect.x, rect.y + thickness, thickness, rect.height - thickness * 2), color);
    // Right
    draw_rect(Rect(rect.right() - thickness, rect.y + thickness, thickness, rect.height - thickness * 2), color);
}

void UIRenderContext::draw_rect_outline_rounded(const Rect& rect, const Vec4& color, float thickness, float radius) {
    // Simplified: just draw outline without rounded corners for now
    // A proper implementation would generate arc vertices
    draw_rect_outline(rect, color, thickness);
}

void UIRenderContext::draw_image(const Rect& rect, render::TextureHandle texture, const Vec4& tint) {
    draw_image_uv(rect, texture, Vec2(0, 0), Vec2(1, 1), tint);
}

void UIRenderContext::draw_image_uv(const Rect& rect, render::TextureHandle texture,
                                     Vec2 uv_min, Vec2 uv_max, const Vec4& tint) {
    new_command(texture.id, false);

    uint32_t packed_color = pack_color(tint);
    uint32_t base = static_cast<uint32_t>(m_vertices.size());

    // Add vertices with texture coordinates
    m_vertices.push_back({Vec2(rect.x, rect.y), uv_min, packed_color});
    m_vertices.push_back({Vec2(rect.right(), rect.y), Vec2(uv_max.x, uv_min.y), packed_color});
    m_vertices.push_back({Vec2(rect.right(), rect.bottom()), uv_max, packed_color});
    m_vertices.push_back({Vec2(rect.x, rect.bottom()), Vec2(uv_min.x, uv_max.y), packed_color});

    m_indices.push_back(base + 0);
    m_indices.push_back(base + 1);
    m_indices.push_back(base + 2);
    m_indices.push_back(base + 2);
    m_indices.push_back(base + 3);
    m_indices.push_back(base + 0);

    m_commands.back().vertex_count += 4;
    m_commands.back().index_count += 6;
}

void UIRenderContext::draw_text(const std::string& text, Vec2 position, FontHandle font,
                                 float size, const Vec4& color, HAlign halign) {
    if (!m_font_manager || text.empty()) return;

    FontAtlas* atlas = m_font_manager->get_font(font);
    if (!atlas) {
        atlas = m_font_manager->get_font(m_font_manager->get_default_font());
        if (!atlas) return;
    }

    // Layout text
    TextLayout layout = m_font_manager->layout_text(font, text);

    // Apply alignment
    Vec2 offset = position;
    switch (halign) {
        case HAlign::Center:
            offset.x -= layout.width * 0.5f;
            break;
        case HAlign::Right:
            offset.x -= layout.width;
            break;
        default:
            break;
    }

    // Adjust for vertical centering (roughly)
    offset.y -= atlas->get_metrics().ascent * 0.5f;

    draw_text_layout(layout, offset, font, color);
}

void UIRenderContext::draw_text_layout(const TextLayout& layout, Vec2 position,
                                         FontHandle font, const Vec4& color) {
    if (!m_font_manager) return;

    FontAtlas* atlas = m_font_manager->get_font(font);
    if (!atlas) return;

    new_command(atlas->get_texture().id, true);

    uint32_t packed_color = pack_color(color);

    for (const auto& glyph : layout.glyphs) {
        if (!glyph.glyph) continue;

        float x = position.x + glyph.x;
        float y = position.y + glyph.y;

        uint32_t base = static_cast<uint32_t>(m_vertices.size());

        // Glyph quad
        m_vertices.push_back({Vec2(x, y), Vec2(glyph.glyph->x0, glyph.glyph->y0), packed_color});
        m_vertices.push_back({Vec2(x + glyph.width, y), Vec2(glyph.glyph->x1, glyph.glyph->y0), packed_color});
        m_vertices.push_back({Vec2(x + glyph.width, y + glyph.height), Vec2(glyph.glyph->x1, glyph.glyph->y1), packed_color});
        m_vertices.push_back({Vec2(x, y + glyph.height), Vec2(glyph.glyph->x0, glyph.glyph->y1), packed_color});

        m_indices.push_back(base + 0);
        m_indices.push_back(base + 1);
        m_indices.push_back(base + 2);
        m_indices.push_back(base + 2);
        m_indices.push_back(base + 3);
        m_indices.push_back(base + 0);

        m_commands.back().vertex_count += 4;
        m_commands.back().index_count += 6;
    }
}

void UIRenderContext::add_vertex(Vec2 pos, Vec2 uv, uint32_t color) {
    m_vertices.push_back({pos, uv, color});
}

void UIRenderContext::add_rect_vertices(const Rect& rect, Vec2 uv_min, Vec2 uv_max, uint32_t color) {
    m_vertices.push_back({Vec2(rect.x, rect.y), uv_min, color});
    m_vertices.push_back({Vec2(rect.right(), rect.y), Vec2(uv_max.x, uv_min.y), color});
    m_vertices.push_back({Vec2(rect.right(), rect.bottom()), uv_max, color});
    m_vertices.push_back({Vec2(rect.x, rect.bottom()), Vec2(uv_min.x, uv_max.y), color});
}

void UIRenderContext::add_rounded_rect_vertices(const Rect& rect, float radius, uint32_t color) {
    // Simplified rounded rect - just add corner segments
    constexpr int CORNER_SEGMENTS = 4;

    radius = std::min(radius, std::min(rect.width, rect.height) * 0.5f);

    // Center vertex for fan
    uint32_t center_idx = static_cast<uint32_t>(m_vertices.size());
    m_vertices.push_back({rect.center(), Vec2(0.5f, 0.5f), color});

    // Generate corner vertices
    std::vector<Vec2> corners = {
        Vec2(rect.x + radius, rect.y + radius),           // Top-left
        Vec2(rect.right() - radius, rect.y + radius),     // Top-right
        Vec2(rect.right() - radius, rect.bottom() - radius), // Bottom-right
        Vec2(rect.x + radius, rect.bottom() - radius)     // Bottom-left
    };

    float start_angles[] = {
        3.14159f,           // Top-left: 180 to 270
        3.14159f * 1.5f,    // Top-right: 270 to 360
        0.0f,               // Bottom-right: 0 to 90
        3.14159f * 0.5f     // Bottom-left: 90 to 180
    };

    uint32_t first_vertex = static_cast<uint32_t>(m_vertices.size());

    for (int c = 0; c < 4; ++c) {
        Vec2 corner_center = corners[c];
        float start_angle = start_angles[c];

        for (int i = 0; i <= CORNER_SEGMENTS; ++i) {
            float angle = start_angle + (3.14159f * 0.5f) * (static_cast<float>(i) / CORNER_SEGMENTS);
            Vec2 offset(std::cos(angle) * radius, std::sin(angle) * radius);
            Vec2 pos = corner_center + offset;
            m_vertices.push_back({pos, Vec2(0.5f, 0.5f), color});
        }
    }

    // Generate triangles (fan from center)
    uint32_t vertex_count = static_cast<uint32_t>(m_vertices.size()) - first_vertex;
    for (uint32_t i = 0; i < vertex_count - 1; ++i) {
        m_indices.push_back(center_idx);
        m_indices.push_back(first_vertex + i);
        m_indices.push_back(first_vertex + i + 1);
    }
    // Close the fan
    m_indices.push_back(center_idx);
    m_indices.push_back(first_vertex + vertex_count - 1);
    m_indices.push_back(first_vertex);

    m_commands.back().vertex_count += vertex_count + 1;
    m_commands.back().index_count += vertex_count * 3;
}

void UIRenderContext::flush_batch() {
    // Nothing to flush if no current command
}

void UIRenderContext::new_command(uint32_t texture_id, bool is_text) {
    // Check if we can extend the current command
    if (!m_commands.empty() && !m_clip_stack.empty()) {
        auto& cmd = m_commands.back();
        const auto& clip = m_clip_stack.back();
        if (cmd.texture_id == texture_id &&
            cmd.is_text == is_text &&
            cmd.clip_rect.x == clip.x &&
            cmd.clip_rect.y == clip.y &&
            cmd.clip_rect.width == clip.width &&
            cmd.clip_rect.height == clip.height) {
            return;  // Can extend current command
        }
    }

    // Create new command
    UIDrawCommand cmd;
    cmd.texture_id = texture_id;
    cmd.is_text = is_text;
    cmd.vertex_offset = static_cast<uint32_t>(m_vertices.size());
    cmd.index_offset = static_cast<uint32_t>(m_indices.size());
    cmd.vertex_count = 0;
    cmd.index_count = 0;
    cmd.clip_rect = m_clip_stack.empty() ?
        Rect(0, 0, static_cast<float>(m_screen_width), static_cast<float>(m_screen_height)) :
        m_clip_stack.back();

    m_commands.push_back(cmd);
    m_current_texture = texture_id;
    m_current_is_text = is_text;
}

// UIRenderer implementation

UIRenderer::UIRenderer()
    : m_vertex_buffer(BGFX_INVALID_HANDLE)
    , m_index_buffer(BGFX_INVALID_HANDLE)
    , m_u_texture(BGFX_INVALID_HANDLE)
    , m_u_params(BGFX_INVALID_HANDLE) {
}

UIRenderer::~UIRenderer() {
    shutdown();
}

bool UIRenderer::init() {
    if (m_initialized) return true;

    // Create vertex layout
    m_vertex_layout
        .begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    // Create dynamic buffers
    m_vertex_buffer = bgfx::createDynamicVertexBuffer(65536, m_vertex_layout, BGFX_BUFFER_ALLOW_RESIZE);
    m_index_buffer = bgfx::createDynamicIndexBuffer(65536 * 3, BGFX_BUFFER_ALLOW_RESIZE);

    // Create uniforms
    m_u_texture = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    m_u_params = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);

    // Create 1x1 white texture for solid colors
    uint32_t white_pixel = 0xFFFFFFFF;
    m_white_texture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
                                            0, bgfx::copy(&white_pixel, 4));

    // Load UI shader
    std::string shader_path = get_shader_path();
    bgfx::ShaderHandle vsh = load_shader_from_file(shader_path + "vs_ui.sc.bin");
    bgfx::ShaderHandle fsh = load_shader_from_file(shader_path + "fs_ui.sc.bin");

    if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
        m_shader = bgfx::createProgram(vsh, fsh, true);
        core::log(core::LogLevel::Info, "UIRenderer: Shader program loaded");
    } else {
        core::log(core::LogLevel::Warn, "UIRenderer: Failed to load shaders - UI will not render");
        if (bgfx::isValid(vsh)) bgfx::destroy(vsh);
        if (bgfx::isValid(fsh)) bgfx::destroy(fsh);
    }

    m_initialized = true;
    core::log(core::LogLevel::Info, "UIRenderer initialized");

    return true;
}

void UIRenderer::shutdown() {
    if (!m_initialized) return;

    if (bgfx::isValid(m_shader)) {
        bgfx::destroy(m_shader);
        m_shader = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(m_vertex_buffer)) {
        bgfx::destroy(m_vertex_buffer);
        m_vertex_buffer = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(m_index_buffer)) {
        bgfx::destroy(m_index_buffer);
        m_index_buffer = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(m_u_texture)) {
        bgfx::destroy(m_u_texture);
        m_u_texture = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(m_u_params)) {
        bgfx::destroy(m_u_params);
        m_u_params = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(m_white_texture)) {
        bgfx::destroy(m_white_texture);
        m_white_texture = BGFX_INVALID_HANDLE;
    }

    m_initialized = false;
    core::log(core::LogLevel::Info, "UIRenderer shutdown");
}

void UIRenderer::render(const UIRenderContext& ctx, render::RenderView view) {
    if (!m_initialized) return;
    if (!bgfx::isValid(m_shader)) return;

    const auto& vertices = ctx.get_vertices();
    const auto& indices = ctx.get_indices();
    const auto& commands = ctx.get_commands();

    if (vertices.empty() || indices.empty() || commands.empty()) {
        return;
    }

    const bgfx::ViewId view_id = static_cast<bgfx::ViewId>(view);

    // Get screen dimensions from context
    uint32_t screen_width = ctx.get_screen_width();
    uint32_t screen_height = ctx.get_screen_height();

    if (screen_width == 0 || screen_height == 0) {
        return;
    }

    // Setup orthographic projection for 2D UI rendering
    float ortho[16];
    bx::mtxOrtho(ortho, 0.0f, static_cast<float>(screen_width),
                 static_cast<float>(screen_height), 0.0f,
                 0.0f, 1000.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);

    bgfx::setViewTransform(view_id, nullptr, ortho);
    bgfx::setViewRect(view_id, 0, 0, static_cast<uint16_t>(screen_width), static_cast<uint16_t>(screen_height));

    // Update buffers
    bgfx::update(m_vertex_buffer, 0, bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(UIVertex))));
    bgfx::update(m_index_buffer, 0, bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t))));

    // Submit draw commands
    for (const auto& cmd : commands) {
        if (cmd.index_count == 0) continue;

        // Set scissor rect for clipping
        const Rect& clip = cmd.clip_rect;
        if (clip.width > 0 && clip.height > 0) {
            bgfx::setScissor(
                static_cast<uint16_t>(clip.x),
                static_cast<uint16_t>(clip.y),
                static_cast<uint16_t>(clip.width),
                static_cast<uint16_t>(clip.height)
            );
        }

        // Bind texture (use white texture for solid colors if texture_id is 0)
        bgfx::TextureHandle tex;
        if (cmd.texture_id == 0) {
            tex = m_white_texture;
        } else {
            tex.idx = static_cast<uint16_t>(cmd.texture_id);
        }

        if (bgfx::isValid(tex)) {
            bgfx::setTexture(0, m_u_texture, tex);
        }

        // Set vertex and index buffers with offsets
        bgfx::setVertexBuffer(0, m_vertex_buffer, cmd.vertex_offset, cmd.vertex_count);
        bgfx::setIndexBuffer(m_index_buffer, cmd.index_offset, cmd.index_count);

        // Set render state for alpha blending
        uint64_t state = BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);

        bgfx::setState(state);

        // Submit
        bgfx::submit(view_id, m_shader);
    }
}

} // namespace engine::ui
