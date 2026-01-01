#include <engine/ui/ui_elements.hpp>
#include <engine/ui/ui_renderer.hpp>

namespace engine::ui {

UIButton::UIButton() {
    m_style = UIStyle::button();
    m_interactive = true;
}

UIButton::UIButton(const std::string& text) : UIButton() {
    m_text = text;
}

void UIButton::on_render(UIRenderContext& ctx) {
    render_background(ctx, m_bounds);

    // Draw text
    if (!m_text.empty()) {
        StyleState state = get_current_state();
        const Vec4& text_color = m_style.text_color.get(state);

        Vec2 text_pos = m_content_bounds.center();
        ctx.draw_text(m_text, text_pos, m_style.font,
                      m_style.font_size, text_color, HAlign::Center);
    }
}

Vec2 UIButton::on_measure(Vec2 available_size) {
    Vec2 size = UIElement::on_measure(available_size);

    // Measure text if we need to fit content
    if (!m_text.empty() && m_style.width_mode == SizeMode::FitContent) {
        // Estimate text size (proper implementation would use FontManager)
        float text_width = m_text.length() * m_style.font_size * 0.6f;
        size.x = text_width + m_style.padding.horizontal();
    }

    return size;
}

void UIButton::on_click_internal() {
    // Button-specific click handling can go here
}

} // namespace engine::ui
