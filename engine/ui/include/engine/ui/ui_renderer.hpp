#pragma once

#include <engine/ui/ui_types.hpp>
#include <engine/ui/ui_font.hpp>
#include <engine/render/types.hpp>
#include <vector>

namespace engine::ui {

// Render context for drawing UI elements
class UIRenderContext {
public:
    UIRenderContext();
    ~UIRenderContext();

    void begin(uint32_t screen_width, uint32_t screen_height);
    void end();

    // Clipping
    void push_clip_rect(const Rect& rect);
    void pop_clip_rect();
    const Rect& get_clip_rect() const;

    // Drawing primitives
    void draw_rect(const Rect& rect, const Vec4& color);
    void draw_rect_rounded(const Rect& rect, const Vec4& color, float radius);
    void draw_rect_outline(const Rect& rect, const Vec4& color, float thickness);
    void draw_rect_outline_rounded(const Rect& rect, const Vec4& color, float thickness, float radius);

    void draw_image(const Rect& rect, render::TextureHandle texture, const Vec4& tint = Vec4(1.0f));
    void draw_image_uv(const Rect& rect, render::TextureHandle texture,
                       Vec2 uv_min, Vec2 uv_max, const Vec4& tint = Vec4(1.0f));

    void draw_text(const std::string& text, Vec2 position, FontHandle font,
                   float size, const Vec4& color, HAlign halign = HAlign::Left);

    void draw_text_layout(const TextLayout& layout, Vec2 position,
                          FontHandle font, const Vec4& color);

    // Get draw lists for rendering
    const std::vector<UIVertex>& get_vertices() const { return m_vertices; }
    const std::vector<uint16_t>& get_indices() const { return m_indices; }
    const std::vector<UIDrawCommand>& get_commands() const { return m_commands; }

    // Font manager access
    void set_font_manager(FontManager* manager) { m_font_manager = manager; }
    FontManager* get_font_manager() { return m_font_manager; }

private:
    void add_vertex(Vec2 pos, Vec2 uv, uint32_t color);
    void add_rect_vertices(const Rect& rect, Vec2 uv_min, Vec2 uv_max, uint32_t color);
    void add_rounded_rect_vertices(const Rect& rect, float radius, uint32_t color);
    void flush_batch();
    void new_command(uint32_t texture_id, bool is_text = false);

    std::vector<UIVertex> m_vertices;
    std::vector<uint16_t> m_indices;
    std::vector<UIDrawCommand> m_commands;

    std::vector<Rect> m_clip_stack;
    uint32_t m_current_texture = 0;
    bool m_current_is_text = false;
    uint32_t m_screen_width = 0;
    uint32_t m_screen_height = 0;

    FontManager* m_font_manager = nullptr;
};

// UI renderer - submits draw commands to bgfx
class UIRenderer {
public:
    UIRenderer();
    ~UIRenderer();

    bool init();
    void shutdown();

    // Render a context to a view
    void render(const UIRenderContext& ctx, render::RenderView view);

    // White texture for solid colors
    render::TextureHandle get_white_texture() const { return m_white_texture; }

private:
    render::ShaderHandle m_shader;
    render::TextureHandle m_white_texture;

    bgfx::VertexLayout m_vertex_layout;
    bgfx::DynamicVertexBufferHandle m_vertex_buffer;
    bgfx::DynamicIndexBufferHandle m_index_buffer;

    bgfx::UniformHandle m_u_texture;
    bgfx::UniformHandle m_u_params;

    bool m_initialized = false;
};

} // namespace engine::ui
