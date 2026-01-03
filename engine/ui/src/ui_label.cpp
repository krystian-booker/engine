#include <engine/ui/ui_elements.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <engine/ui/ui_context.hpp>

namespace engine::ui {

UILabel::UILabel() {
    m_style = UIStyle::label();
}

UILabel::UILabel(const std::string& text) : UILabel() {
    m_text = text;
}

void UILabel::on_render(UIRenderContext& ctx) {
    render_background(ctx, m_bounds);

    if (!m_text.empty()) {
        StyleState state = get_current_state();
        const Vec4& text_color = m_style.text_color.get(state);

        Vec2 text_pos;

        // Calculate text position based on alignment
        switch (m_style.text_align) {
            case HAlign::Left:
                text_pos.x = m_content_bounds.x;
                break;
            case HAlign::Center:
                text_pos.x = m_content_bounds.center().x;
                break;
            case HAlign::Right:
                text_pos.x = m_content_bounds.right();
                break;
        }

        switch (m_style.text_valign) {
            case VAlign::Top:
                text_pos.y = m_content_bounds.y;
                break;
            case VAlign::Center:
                text_pos.y = m_content_bounds.center().y;
                break;
            case VAlign::Bottom:
                text_pos.y = m_content_bounds.bottom();
                break;
        }

        ctx.draw_text(m_text, text_pos, m_style.font,
                      m_style.font_size, text_color, m_style.text_align);
    }
}

Vec2 UILabel::on_measure(Vec2 available_size) {
    Vec2 size = UIElement::on_measure(available_size);

    if (!m_text.empty() && (m_style.width_mode == SizeMode::FitContent ||
                            m_style.height_mode == SizeMode::FitContent)) {
        UIContext* ctx = get_ui_context();
        if (ctx) {
            Vec2 text_size = ctx->font_manager().measure_text(m_style.font, m_text);

            if (m_style.width_mode == SizeMode::FitContent) {
                size.x = text_size.x + m_style.padding.horizontal();
            }
            if (m_style.height_mode == SizeMode::FitContent) {
                size.y = text_size.y + m_style.padding.vertical();
            }
        }
    }

    return size;
}

} // namespace engine::ui
