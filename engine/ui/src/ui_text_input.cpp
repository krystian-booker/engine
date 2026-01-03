#include <engine/ui/ui_elements.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <algorithm>

namespace engine::ui {

UITextInput::UITextInput() {
    m_style = UIStyle::text_input();
    m_interactive = true;
}

UITextInput::UITextInput(const std::string& placeholder) : UITextInput() {
    m_placeholder = placeholder;
}

void UITextInput::set_text(const std::string& text) {
    if (m_text != text) {
        m_text = text;
        if (m_text.length() > m_max_length) {
            m_text = m_text.substr(0, m_max_length);
        }
        m_cursor_pos = std::min(m_cursor_pos, m_text.length());
        mark_dirty();
    }
}

void UITextInput::on_update(float dt, const UIInputState& input) {
    if (!m_enabled || !is_focused()) return;

    // Cursor blink
    m_cursor_blink_timer += dt;
    if (m_cursor_blink_timer >= CURSOR_BLINK_RATE) {
        m_cursor_blink_timer = 0.0f;
        m_cursor_visible = !m_cursor_visible;
        mark_dirty();
    }

    // Handle text input
    if (!input.text_input.empty()) {
        insert_text(input.text_input);
    }

    // Handle special keys
    if (input.key_backspace) {
        delete_char_before_cursor();
    }

    if (input.key_delete) {
        delete_char_after_cursor();
    }

    if (input.key_left) {
        move_cursor_left();
    }

    if (input.key_right) {
        move_cursor_right();
    }

    if (input.key_home) {
        if (m_cursor_pos != 0) {
            m_cursor_pos = 0;
            m_cursor_visible = true;
            m_cursor_blink_timer = 0.0f;
            mark_dirty();
        }
    }

    if (input.key_end) {
        if (m_cursor_pos != m_text.length()) {
            m_cursor_pos = m_text.length();
            m_cursor_visible = true;
            m_cursor_blink_timer = 0.0f;
            mark_dirty();
        }
    }

    if (input.key_enter) {
        if (on_submit) {
            on_submit(m_text);
        }
    }
}

void UITextInput::on_render(UIRenderContext& ctx) {
    StyleState state = get_current_state();

    // Background
    render_background(ctx, m_bounds);

    // Clip text to content bounds
    ctx.push_clip_rect(m_content_bounds);

    bool show_placeholder = m_text.empty() && !m_placeholder.empty();
    const std::string& display_text = show_placeholder ? m_placeholder : m_text;

    // Text color (placeholder is dimmer)
    Vec4 text_color = m_style.text_color.get(state);
    if (show_placeholder) {
        text_color.a *= 0.5f;
    }

    // Calculate text position
    Vec2 text_pos(m_content_bounds.x, m_content_bounds.y + m_content_bounds.height * 0.5f);

    // Draw text
    if (!display_text.empty()) {
        ctx.draw_text(display_text, text_pos, m_style.font,
                      m_style.font_size, text_color, HAlign::Left);
    }

    // Draw cursor when focused
    if (is_focused() && m_cursor_visible && !show_placeholder) {
        // Estimate cursor x position based on character count before cursor
        float cursor_x = m_content_bounds.x;
        if (m_cursor_pos > 0) {
            // Rough estimate - ideally should use font metrics
            cursor_x += m_cursor_pos * m_style.font_size * 0.6f;
        }

        float cursor_y = m_content_bounds.y + 2.0f;
        float cursor_height = m_content_bounds.height - 4.0f;

        Vec4 cursor_color = m_style.text_color.get(state);
        ctx.draw_rect(Rect(cursor_x, cursor_y, 1.5f, cursor_height), cursor_color);
    }

    ctx.pop_clip_rect();
}

Vec2 UITextInput::on_measure(Vec2 available_size) {
    Vec2 size = UIElement::on_measure(available_size);

    // Ensure minimum size for comfortable text entry
    size.x = std::max(size.x, m_style.min_width);
    size.y = std::max(size.y, m_style.font_size + m_style.padding.vertical());

    return size;
}

void UITextInput::on_click_internal() {
    request_focus();
    m_cursor_visible = true;
    m_cursor_blink_timer = 0.0f;
}

void UITextInput::on_focus_changed(bool focused) {
    if (focused) {
        m_cursor_visible = true;
        m_cursor_blink_timer = 0.0f;
    }
    mark_dirty();
}

void UITextInput::insert_text(const std::string& text) {
    if (m_text.length() + text.length() > m_max_length) {
        size_t allowed = m_max_length - m_text.length();
        if (allowed == 0) return;
        m_text.insert(m_cursor_pos, text.substr(0, allowed));
        m_cursor_pos += allowed;
    } else {
        m_text.insert(m_cursor_pos, text);
        m_cursor_pos += text.length();
    }

    m_cursor_visible = true;
    m_cursor_blink_timer = 0.0f;
    mark_dirty();

    if (on_text_changed) {
        on_text_changed(m_text);
    }
}

void UITextInput::delete_char_before_cursor() {
    if (m_cursor_pos > 0 && !m_text.empty()) {
        m_text.erase(m_cursor_pos - 1, 1);
        m_cursor_pos--;
        m_cursor_visible = true;
        m_cursor_blink_timer = 0.0f;
        mark_dirty();

        if (on_text_changed) {
            on_text_changed(m_text);
        }
    }
}

void UITextInput::delete_char_after_cursor() {
    if (m_cursor_pos < m_text.length()) {
        m_text.erase(m_cursor_pos, 1);
        m_cursor_visible = true;
        m_cursor_blink_timer = 0.0f;
        mark_dirty();

        if (on_text_changed) {
            on_text_changed(m_text);
        }
    }
}

void UITextInput::move_cursor_left() {
    if (m_cursor_pos > 0) {
        m_cursor_pos--;
        m_cursor_visible = true;
        m_cursor_blink_timer = 0.0f;
        mark_dirty();
    }
}

void UITextInput::move_cursor_right() {
    if (m_cursor_pos < m_text.length()) {
        m_cursor_pos++;
        m_cursor_visible = true;
        m_cursor_blink_timer = 0.0f;
        mark_dirty();
    }
}

} // namespace engine::ui
