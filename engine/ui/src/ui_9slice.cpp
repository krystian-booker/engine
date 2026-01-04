#include <engine/ui/ui_9slice.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <algorithm>

namespace engine::ui {

UI9SliceImage::UI9SliceImage() {
    // 9-slice images are not interactive by default
}

UI9SliceImage::UI9SliceImage(render::TextureHandle texture, const NineSliceBorder& border)
    : UI9SliceImage() {
    m_texture = texture;
    m_border = border;
}

void UI9SliceImage::set_border_pixels(float left, float top, float right, float bottom) {
    m_border = NineSliceBorder(left, top, right, bottom);
    mark_dirty();
}

void UI9SliceImage::set_texture_size(uint32_t width, uint32_t height) {
    m_texture_width = width;
    m_texture_height = height;
    mark_dirty();
}

void UI9SliceImage::on_render(UIRenderContext& ctx) {
    render_background(ctx, m_bounds);

    if (!m_texture.valid() || m_texture_width == 0 || m_texture_height == 0) {
        return;
    }

    const Rect& rect = m_content_bounds;

    // Border sizes in pixels (clamped to half of available space)
    float bl = std::min(m_border.left, rect.width * 0.5f);
    float br = std::min(m_border.right, rect.width * 0.5f);
    float bt = std::min(m_border.top, rect.height * 0.5f);
    float bb = std::min(m_border.bottom, rect.height * 0.5f);

    // Ensure we don't exceed total width/height
    if (bl + br > rect.width) {
        float scale = rect.width / (bl + br);
        bl *= scale;
        br *= scale;
    }
    if (bt + bb > rect.height) {
        float scale = rect.height / (bt + bb);
        bt *= scale;
        bb *= scale;
    }

    // Screen-space X positions
    float x0 = rect.x;
    float x1 = rect.x + bl;
    float x2 = rect.right() - br;
    float x3 = rect.right();

    // Screen-space Y positions
    float y0 = rect.y;
    float y1 = rect.y + bt;
    float y2 = rect.bottom() - bb;
    float y3 = rect.bottom();

    // UV coordinates (normalized 0-1 based on texture dimensions)
    float tex_w = static_cast<float>(m_texture_width);
    float tex_h = static_cast<float>(m_texture_height);

    float u0 = 0.0f;
    float u1 = m_border.left / tex_w;
    float u2 = 1.0f - m_border.right / tex_w;
    float u3 = 1.0f;

    float v0 = 0.0f;
    float v1 = m_border.top / tex_h;
    float v2 = 1.0f - m_border.bottom / tex_h;
    float v3 = 1.0f;

    // Draw 9 regions:
    // [TL][T ][TR]
    // [L ][C ][R ]
    // [BL][B ][BR]

    // Top row
    if (bt > 0.0f) {
        // Top-left corner
        if (bl > 0.0f) {
            ctx.draw_image_uv(Rect(x0, y0, bl, bt), m_texture,
                              Vec2(u0, v0), Vec2(u1, v1), m_tint);
        }
        // Top edge (stretches horizontally)
        if (x2 > x1) {
            ctx.draw_image_uv(Rect(x1, y0, x2 - x1, bt), m_texture,
                              Vec2(u1, v0), Vec2(u2, v1), m_tint);
        }
        // Top-right corner
        if (br > 0.0f) {
            ctx.draw_image_uv(Rect(x2, y0, br, bt), m_texture,
                              Vec2(u2, v0), Vec2(u3, v1), m_tint);
        }
    }

    // Middle row
    if (y2 > y1) {
        // Left edge (stretches vertically)
        if (bl > 0.0f) {
            ctx.draw_image_uv(Rect(x0, y1, bl, y2 - y1), m_texture,
                              Vec2(u0, v1), Vec2(u1, v2), m_tint);
        }
        // Center (stretches both directions)
        if (x2 > x1) {
            ctx.draw_image_uv(Rect(x1, y1, x2 - x1, y2 - y1), m_texture,
                              Vec2(u1, v1), Vec2(u2, v2), m_tint);
        }
        // Right edge (stretches vertically)
        if (br > 0.0f) {
            ctx.draw_image_uv(Rect(x2, y1, br, y2 - y1), m_texture,
                              Vec2(u2, v1), Vec2(u3, v2), m_tint);
        }
    }

    // Bottom row
    if (bb > 0.0f) {
        // Bottom-left corner
        if (bl > 0.0f) {
            ctx.draw_image_uv(Rect(x0, y2, bl, bb), m_texture,
                              Vec2(u0, v2), Vec2(u1, v3), m_tint);
        }
        // Bottom edge (stretches horizontally)
        if (x2 > x1) {
            ctx.draw_image_uv(Rect(x1, y2, x2 - x1, bb), m_texture,
                              Vec2(u1, v2), Vec2(u2, v3), m_tint);
        }
        // Bottom-right corner
        if (br > 0.0f) {
            ctx.draw_image_uv(Rect(x2, y2, br, bb), m_texture,
                              Vec2(u2, v2), Vec2(u3, v3), m_tint);
        }
    }
}

Vec2 UI9SliceImage::on_measure(Vec2 available_size) {
    Vec2 size = UIElement::on_measure(available_size);

    // Minimum size is the sum of border regions
    if (m_style.width_mode == SizeMode::FitContent) {
        size.x = std::max(size.x, m_border.horizontal());
    }
    if (m_style.height_mode == SizeMode::FitContent) {
        size.y = std::max(size.y, m_border.vertical());
    }

    return size;
}

} // namespace engine::ui
