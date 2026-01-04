#include <engine/ui/ui_rich_label.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <engine/ui/ui_context.hpp>

namespace engine::ui {

UIRichLabel::UIRichLabel() {
    m_style = UIStyle::label();
    m_base_style.color = m_style.text_color.normal;
    m_base_style.font_size = m_style.font_size;
}

UIRichLabel::UIRichLabel(const std::string& markup) : UIRichLabel() {
    set_markup(markup);
}

void UIRichLabel::set_markup(const std::string& markup) {
    m_markup = markup;
    invalidate_layout();
}

void UIRichLabel::set_text(const std::string& text) {
    // For plain text, we don't need to escape - just set directly
    // and let the parser treat it as plain text
    m_markup = RichTextParser::escape(text);
    invalidate_layout();
}

void UIRichLabel::invalidate_layout() {
    m_layout_dirty = true;
    mark_dirty();
    mark_layout_dirty();
}

void UIRichLabel::rebuild_layout(float max_width) {
    UIContext* ctx = get_ui_context();
    if (!ctx) {
        m_layout = RichTextLayout();
        m_layout_dirty = false;
        return;
    }

    // Update base style from current style state
    RichTextStyle base = m_base_style;
    if (base.font == INVALID_FONT_HANDLE) {
        base.font = m_style.font;
        if (base.font == INVALID_FONT_HANDLE) {
            base.font = ctx->get_default_font();
        }
    }

    // Use style's font size if not set
    if (m_base_style.font_size <= 0.0f) {
        base.font_size = m_style.font_size;
    }

    // Apply current state color as base
    base.color = m_base_style.color;

    float layout_width = m_word_wrap ? max_width : 0.0f;
    m_layout = RichTextParser::parse(m_markup, base, &ctx->font_manager(),
                                      layout_width, m_word_wrap);

    m_layout_dirty = false;
    m_last_layout_width = max_width;
}

void UIRichLabel::on_layout(const Rect& bounds) {
    // Rebuild layout if dirty or width changed (for word wrap)
    float available_width = bounds.width - m_style.padding.horizontal();
    if (m_layout_dirty || (m_word_wrap && available_width != m_last_layout_width)) {
        rebuild_layout(available_width);
    }
}

void UIRichLabel::on_render(UIRenderContext& ctx) {
    render_background(ctx, m_bounds);

    if (m_layout.runs.empty()) {
        return;
    }

    UIContext* ui_ctx = get_ui_context();
    if (!ui_ctx) return;

    // Calculate text offset for alignment
    Vec2 text_offset = Vec2(m_content_bounds.x, m_content_bounds.y);

    // Horizontal alignment
    switch (m_text_align) {
        case HAlign::Center:
            text_offset.x += (m_content_bounds.width - m_layout.total_width) * 0.5f;
            break;
        case HAlign::Right:
            text_offset.x += m_content_bounds.width - m_layout.total_width;
            break;
        default:
            break;
    }

    // Vertical alignment
    switch (m_vertical_align) {
        case VAlign::Center:
            text_offset.y += (m_content_bounds.height - m_layout.total_height) * 0.5f;
            break;
        case VAlign::Bottom:
            text_offset.y += m_content_bounds.height - m_layout.total_height;
            break;
        default:
            break;
    }

    // Render each run
    for (const auto& run : m_layout.runs) {
        if (run.text.empty()) continue;

        Vec2 pos(text_offset.x + run.x, text_offset.y + run.y);

        FontHandle font = run.style.font;
        if (font == INVALID_FONT_HANDLE) {
            font = ui_ctx->get_default_font();
        }

        // Draw text
        ctx.draw_text(run.text, pos, font, run.style.font_size, run.style.color, HAlign::Left);

        // Draw underline if needed
        if (run.style.underline) {
            float underline_y = pos.y + run.height + 2.0f;
            ctx.draw_rect(Rect(pos.x, underline_y, run.width, 1.0f), run.style.color);
        }

        // Draw strikethrough if needed
        if (run.style.strikethrough) {
            float strike_y = pos.y + run.height * 0.5f;
            ctx.draw_rect(Rect(pos.x, strike_y, run.width, 1.0f), run.style.color);
        }
    }
}

Vec2 UIRichLabel::on_measure(Vec2 available_size) {
    Vec2 size = UIElement::on_measure(available_size);

    // Calculate layout if needed for content-based sizing
    if (m_style.width_mode == SizeMode::FitContent ||
        m_style.height_mode == SizeMode::FitContent) {

        float max_width = m_word_wrap ? available_size.x - m_style.padding.horizontal() : 0.0f;

        if (m_layout_dirty || (m_word_wrap && max_width != m_last_layout_width)) {
            rebuild_layout(max_width);
        }

        if (m_style.width_mode == SizeMode::FitContent) {
            size.x = m_layout.total_width + m_style.padding.horizontal();
        }
        if (m_style.height_mode == SizeMode::FitContent) {
            size.y = m_layout.total_height + m_style.padding.vertical();
        }
    }

    return size;
}

} // namespace engine::ui
