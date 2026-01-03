#include <engine/ui/ui_elements.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <engine/ui/ui_context.hpp>
#include <algorithm>

namespace engine::ui {

UIDialog::UIDialog() {
    m_style = UIStyle::panel();
    m_interactive = true;
    rebuild_buttons();
}

void UIDialog::rebuild_buttons() {
    m_dialog_buttons.clear();

    switch (m_buttons) {
        case DialogButtons::OK:
            m_dialog_buttons.push_back({"OK", DialogResult::OK, {}, false, false});
            break;
        case DialogButtons::OKCancel:
            m_dialog_buttons.push_back({"OK", DialogResult::OK, {}, false, false});
            m_dialog_buttons.push_back({"Cancel", DialogResult::Cancel, {}, false, false});
            break;
        case DialogButtons::YesNo:
            m_dialog_buttons.push_back({"Yes", DialogResult::Yes, {}, false, false});
            m_dialog_buttons.push_back({"No", DialogResult::No, {}, false, false});
            break;
        case DialogButtons::YesNoCancel:
            m_dialog_buttons.push_back({"Yes", DialogResult::Yes, {}, false, false});
            m_dialog_buttons.push_back({"No", DialogResult::No, {}, false, false});
            m_dialog_buttons.push_back({"Cancel", DialogResult::Cancel, {}, false, false});
            break;
    }

    mark_dirty();
}

void UIDialog::show() {
    if (!m_is_showing) {
        m_is_showing = true;
        m_result = DialogResult::None;
        set_visible(true);
        mark_dirty();
    }
}

void UIDialog::hide() {
    if (m_is_showing) {
        m_is_showing = false;
        set_visible(false);
        mark_dirty();
    }
}

void UIDialog::handle_button_click(DialogResult result) {
    m_result = result;
    hide();
    if (on_result) {
        on_result(result);
    }
}

void UIDialog::on_update(float dt, const UIInputState& input) {
    if (!m_is_showing) return;

    // Update button hover/press states
    for (auto& btn : m_dialog_buttons) {
        bool was_hovered = btn.hovered;
        btn.hovered = btn.bounds.contains(input.mouse_position);

        if (btn.hovered && input.was_mouse_pressed(0)) {
            btn.pressed = true;
        }
        if (btn.pressed && input.was_mouse_released(0)) {
            btn.pressed = false;
            if (btn.hovered) {
                handle_button_click(btn.result);
            }
        }
        if (!input.is_mouse_down(0)) {
            btn.pressed = false;
        }
    }
}

void UIDialog::render(UIRenderContext& ctx) {
    if (!is_visible() || !m_is_showing) return;

    UIContext* ui_ctx = get_ui_context();
    if (!ui_ctx) return;

    float screen_w = static_cast<float>(ui_ctx->get_screen_width());
    float screen_h = static_cast<float>(ui_ctx->get_screen_height());

    // Draw overlay
    ctx.draw_rect(Rect(0, 0, screen_w, screen_h), Vec4(0.0f, 0.0f, 0.0f, 0.5f));

    // Calculate dialog size
    float font_size = 14.0f;
    FontHandle font = ui_ctx->font_manager().get_default_font();

    Vec2 title_size = ui_ctx->font_manager().measure_text(font, m_title);
    Vec2 message_size = ui_ctx->font_manager().measure_text(font, m_message);

    float content_width = std::max({m_dialog_width, title_size.x + m_padding * 2, message_size.x + m_padding * 2});
    float button_area_width = m_dialog_buttons.size() * 80.0f + (m_dialog_buttons.size() - 1) * m_button_spacing;
    content_width = std::max(content_width, button_area_width + m_padding * 2);

    float title_height = m_title.empty() ? 0.0f : 30.0f;
    float message_height = m_message.empty() ? 0.0f : message_size.y + m_padding;
    float button_area_height = m_button_height + m_padding;

    float dialog_height = title_height + message_height + button_area_height + m_padding;

    // Center dialog on screen
    Rect dialog_rect(
        (screen_w - content_width) * 0.5f,
        (screen_h - dialog_height) * 0.5f,
        content_width,
        dialog_height
    );

    // Update bounds for the element
    m_bounds = dialog_rect;
    m_content_bounds = Rect(
        dialog_rect.x + m_padding,
        dialog_rect.y + m_padding,
        dialog_rect.width - m_padding * 2,
        dialog_rect.height - m_padding * 2
    );

    // Draw dialog background
    ctx.draw_rect_rounded(dialog_rect, Vec4(0.18f, 0.18f, 0.18f, 1.0f), 8.0f);
    ctx.draw_rect_outline_rounded(dialog_rect, Vec4(0.35f, 0.35f, 0.35f, 1.0f), 1.0f, 8.0f);

    float y = dialog_rect.y + m_padding;

    // Draw title
    if (!m_title.empty()) {
        Vec2 title_pos(dialog_rect.center().x, y + font_size * 0.5f);
        ctx.draw_text(m_title, title_pos, font, font_size + 2,
                      Vec4(1.0f, 1.0f, 1.0f, 1.0f), HAlign::Center);
        y += title_height;
    }

    // Draw message
    if (!m_message.empty()) {
        Vec2 message_pos(dialog_rect.center().x, y + font_size * 0.5f);
        ctx.draw_text(m_message, message_pos, font, font_size,
                      Vec4(0.85f, 0.85f, 0.85f, 1.0f), HAlign::Center);
        y += message_height;
    }

    // Calculate button positions
    float total_button_width = m_dialog_buttons.size() * 80.0f + (m_dialog_buttons.size() - 1) * m_button_spacing;
    float button_x = dialog_rect.center().x - total_button_width * 0.5f;
    float button_y = dialog_rect.bottom() - m_padding - m_button_height;

    for (auto& btn : m_dialog_buttons) {
        btn.bounds = Rect(button_x, button_y, 80.0f, m_button_height);

        // Button colors
        Vec4 bg_color(0.25f, 0.25f, 0.25f, 1.0f);
        Vec4 text_color(0.95f, 0.95f, 0.95f, 1.0f);

        // Primary button (first one) gets accent color
        bool is_primary = (&btn == &m_dialog_buttons.front());
        if (is_primary) {
            bg_color = Vec4(0.2f, 0.45f, 0.85f, 1.0f);
        }

        if (btn.pressed) {
            bg_color = bg_color * 0.7f;
            bg_color.a = 1.0f;
        } else if (btn.hovered) {
            bg_color = bg_color * 1.2f;
            bg_color.a = 1.0f;
        }

        // Draw button
        ctx.draw_rect_rounded(btn.bounds, bg_color, 4.0f);

        // Draw button text
        Vec2 btn_text_pos(btn.bounds.center().x, btn.bounds.center().y);
        ctx.draw_text(btn.label, btn_text_pos, font, font_size,
                      text_color, HAlign::Center);

        button_x += 80.0f + m_button_spacing;
    }

    m_dirty = false;
}

void UIDialog::on_render(UIRenderContext& ctx) {
    // Rendering is handled entirely in render() override
}

Vec2 UIDialog::on_measure(Vec2 available_size) {
    // Dialog sizes itself based on content
    return Vec2(m_dialog_width, 150.0f);
}

} // namespace engine::ui
