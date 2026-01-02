#pragma once

#include <engine/ui/ui_element.hpp>
#include <engine/render/types.hpp>
#include <string>

namespace engine::ui {

// Panel - container with background
class UIPanel : public UIElement {
public:
    UIPanel();

    void set_layout_direction(LayoutDirection dir) { m_layout_direction = dir; mark_layout_dirty(); }
    LayoutDirection get_layout_direction() const { return m_layout_direction; }

    void set_spacing(float spacing) { m_spacing = spacing; mark_layout_dirty(); }
    float get_spacing() const { return m_spacing; }

    void set_overflow(Overflow overflow) { m_overflow = overflow; }
    Overflow get_overflow() const { return m_overflow; }

protected:
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;
    void on_layout(const Rect& bounds) override;

private:
    LayoutDirection m_layout_direction = LayoutDirection::Vertical;
    float m_spacing = 4.0f;
    Overflow m_overflow = Overflow::Visible;
    Vec2 m_scroll_offset{0.0f};
};

// Label - text display
class UILabel : public UIElement {
public:
    UILabel();
    explicit UILabel(const std::string& text);

    void set_text(const std::string& text) { m_text = text; mark_dirty(); }
    const std::string& get_text() const { return m_text; }

protected:
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;

private:
    std::string m_text;
};

// Button - clickable element
class UIButton : public UIElement {
public:
    UIButton();
    explicit UIButton(const std::string& text);

    void set_text(const std::string& text) { m_text = text; mark_dirty(); }
    const std::string& get_text() const { return m_text; }

protected:
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;
    void on_click_internal() override;

private:
    std::string m_text;
};

// Image - texture display
class UIImage : public UIElement {
public:
    UIImage();
    explicit UIImage(render::TextureHandle texture);

    void set_texture(render::TextureHandle texture) { m_texture = texture; mark_dirty(); }
    render::TextureHandle get_texture() const { return m_texture; }

    void set_tint(const Vec4& tint) { m_tint = tint; mark_dirty(); }
    const Vec4& get_tint() const { return m_tint; }

protected:
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;

private:
    render::TextureHandle m_texture;
    Vec4 m_tint{1.0f};
};

// Slider - value input
class UISlider : public UIElement {
public:
    UISlider();

    void set_value(float value);
    float get_value() const { return m_value; }

    void set_range(float min, float max);
    float get_min() const { return m_min; }
    float get_max() const { return m_max; }

    void set_step(float step) { m_step = step; }
    float get_step() const { return m_step; }

    void set_orientation(LayoutDirection orientation) { m_orientation = orientation; mark_dirty(); }
    LayoutDirection get_orientation() const { return m_orientation; }

    // Slider-specific style
    void set_track_color(const Vec4& color) { m_track_color = color; }
    void set_fill_color(const Vec4& color) { m_fill_color = color; }
    void set_thumb_color(const Vec4& color) { m_thumb_color = color; }
    void set_thumb_size(float size) { m_thumb_size = size; }

    ValueChangedCallback on_value_changed;

protected:
    void on_update(float dt, const UIInputState& input) override;
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;

private:
    float value_from_position(float pos);
    float position_from_value(float value);

    float m_value = 0.0f;
    float m_min = 0.0f;
    float m_max = 1.0f;
    float m_step = 0.0f;  // 0 = continuous

    LayoutDirection m_orientation = LayoutDirection::Horizontal;

    Vec4 m_track_color{0.2f, 0.2f, 0.2f, 1.0f};
    Vec4 m_fill_color{0.3f, 0.5f, 0.9f, 1.0f};
    Vec4 m_thumb_color{0.8f, 0.8f, 0.8f, 1.0f};
    float m_thumb_size = 16.0f;

    bool m_dragging = false;
};

// Progress bar - display-only value indicator
class UIProgressBar : public UIElement {
public:
    UIProgressBar();

    void set_value(float value);
    float get_value() const { return m_value; }

    void set_orientation(LayoutDirection orientation) { m_orientation = orientation; mark_dirty(); }
    LayoutDirection get_orientation() const { return m_orientation; }

    void set_track_color(const Vec4& color) { m_track_color = color; }
    void set_fill_color(const Vec4& color) { m_fill_color = color; }

protected:
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;

private:
    float m_value = 0.0f;  // 0.0 to 1.0
    LayoutDirection m_orientation = LayoutDirection::Horizontal;
    Vec4 m_track_color{0.2f, 0.2f, 0.2f, 1.0f};
    Vec4 m_fill_color{0.3f, 0.7f, 0.3f, 1.0f};
};

// Toggle/Checkbox
class UIToggle : public UIElement {
public:
    UIToggle();
    explicit UIToggle(const std::string& label);

    void set_checked(bool checked);
    bool is_checked() const { return m_checked; }

    void set_label(const std::string& label) { m_label = label; mark_dirty(); }
    const std::string& get_label() const { return m_label; }

    std::function<void(bool)> on_toggled;

protected:
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;
    void on_click_internal() override;

private:
    bool m_checked = false;
    std::string m_label;
    float m_box_size = 18.0f;
};

} // namespace engine::ui
