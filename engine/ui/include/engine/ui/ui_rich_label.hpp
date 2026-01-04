#pragma once

#include <engine/ui/ui_element.hpp>
#include <engine/ui/ui_rich_text.hpp>

namespace engine::ui {

// Rich text label element - displays text with inline formatting
// Supports markup tags: <b>, <i>, <u>, <s>, <color=...>, <size=...>
class UIRichLabel : public UIElement {
public:
    UIRichLabel();
    explicit UIRichLabel(const std::string& markup);

    // Set markup text with formatting tags
    void set_markup(const std::string& markup);
    const std::string& get_markup() const { return m_markup; }

    // Set plain text (no formatting, escapes special characters)
    void set_text(const std::string& text);

    // Base style (default for unmarked text)
    void set_base_color(const Vec4& color) { m_base_style.color = color; invalidate_layout(); }
    void set_base_font(FontHandle font) { m_base_style.font = font; invalidate_layout(); }
    void set_base_font_size(float size) { m_base_style.font_size = size; invalidate_layout(); }

    const RichTextStyle& get_base_style() const { return m_base_style; }

    // Text alignment
    void set_text_align(HAlign align) { m_text_align = align; mark_dirty(); }
    HAlign get_text_align() const { return m_text_align; }

    // Vertical alignment
    void set_vertical_align(VAlign align) { m_vertical_align = align; mark_dirty(); }
    VAlign get_vertical_align() const { return m_vertical_align; }

    // Word wrapping
    void set_word_wrap(bool wrap) { m_word_wrap = wrap; invalidate_layout(); }
    bool get_word_wrap() const { return m_word_wrap; }

    // Get the computed layout (for debugging or advanced usage)
    const RichTextLayout& get_layout() const { return m_layout; }

protected:
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;
    void on_layout(const Rect& bounds) override;

private:
    void rebuild_layout(float max_width);
    void invalidate_layout();

    std::string m_markup;
    RichTextStyle m_base_style;
    RichTextLayout m_layout;
    bool m_layout_dirty = true;
    float m_last_layout_width = 0.0f;

    HAlign m_text_align = HAlign::Left;
    VAlign m_vertical_align = VAlign::Top;
    bool m_word_wrap = false;
};

} // namespace engine::ui
