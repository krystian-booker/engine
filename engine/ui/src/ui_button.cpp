#include <engine/ui/ui_elements.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <engine/ui/ui_context.hpp>
#include <engine/localization/localization.hpp>

namespace engine::ui {

std::string UIButton::get_resolved_text() const {
    if (!m_text_key.empty()) {
        return localization::loc(m_text_key);
    }
    return m_text;
}

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
    std::string text = get_resolved_text();
    if (!text.empty()) {
        StyleState state = get_current_state();
        const Vec4& text_color = m_style.text_color.get(state);

        Vec2 text_pos = m_content_bounds.center();
        ctx.draw_text(text, text_pos, m_style.font,
                      m_style.font_size, text_color, HAlign::Center);
    }
}

Vec2 UIButton::on_measure(Vec2 available_size) {
    Vec2 size = UIElement::on_measure(available_size);

    // Measure text if we need to fit content
    std::string text = get_resolved_text();
    if (!text.empty() && m_style.width_mode == SizeMode::FitContent) {
        UIContext* ctx = get_ui_context();
        if (ctx) {
            Vec2 text_size = ctx->font_manager().measure_text(m_style.font, text);
            size.x = text_size.x + m_style.padding.horizontal();
        }
    }

    return size;
}

void UIButton::on_click_internal() {
    // Button-specific click handling can go here
}

} // namespace engine::ui
