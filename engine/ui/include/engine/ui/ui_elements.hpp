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

    // Scroll control
    void set_scroll_offset(Vec2 offset);
    Vec2 get_scroll_offset() const { return m_scroll_offset; }
    Vec2 get_content_size() const { return m_content_size; }
    Vec2 get_max_scroll() const;
    void scroll_to_top() { set_scroll_offset({0.0f, 0.0f}); }
    void scroll_to_bottom();

    // Scroll bar visibility
    void set_show_scrollbar(bool show) { m_show_scrollbar = show; }
    bool get_show_scrollbar() const { return m_show_scrollbar; }

    void render(UIRenderContext& ctx) override;

protected:
    void on_update(float dt, const UIInputState& input) override;
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;
    void on_layout(const Rect& bounds) override;

private:
    void render_scrollbar(UIRenderContext& ctx);
    Vec2 calculate_content_size();

    LayoutDirection m_layout_direction = LayoutDirection::Vertical;
    float m_spacing = 4.0f;
    Overflow m_overflow = Overflow::Visible;
    Vec2 m_scroll_offset{0.0f};
    Vec2 m_content_size{0.0f};
    bool m_show_scrollbar = true;
    float m_scrollbar_width = 8.0f;
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

// Text input field
class UITextInput : public UIElement {
public:
    UITextInput();
    explicit UITextInput(const std::string& placeholder);

    void set_text(const std::string& text);
    const std::string& get_text() const { return m_text; }

    void set_placeholder(const std::string& text) { m_placeholder = text; mark_dirty(); }
    const std::string& get_placeholder() const { return m_placeholder; }

    void set_max_length(size_t max) { m_max_length = max; }
    size_t get_max_length() const { return m_max_length; }

    TextChangedCallback on_text_changed;
    std::function<void(const std::string&)> on_submit;  // Enter pressed

protected:
    void on_update(float dt, const UIInputState& input) override;
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;
    void on_click_internal() override;
    void on_focus_changed(bool focused) override;

private:
    void insert_text(const std::string& text);
    void delete_char_before_cursor();
    void delete_char_after_cursor();
    void move_cursor_left();
    void move_cursor_right();

    std::string m_text;
    std::string m_placeholder;
    size_t m_cursor_pos = 0;
    size_t m_max_length = 256;
    float m_cursor_blink_timer = 0.0f;
    bool m_cursor_visible = true;

    static constexpr float CURSOR_BLINK_RATE = 0.53f;
};

// Dropdown item
struct DropdownItem {
    std::string id;
    std::string label;
};

// Dropdown/Select - value selection from a list
class UIDropdown : public UIElement {
public:
    UIDropdown();

    // Items management
    void add_item(const std::string& id, const std::string& label);
    void clear_items();
    const std::vector<DropdownItem>& get_items() const { return m_items; }

    // Selection
    void set_selected_id(const std::string& id);
    const std::string& get_selected_id() const { return m_selected_id; }
    const std::string& get_selected_label() const;

    // Placeholder when nothing selected
    void set_placeholder(const std::string& text) { m_placeholder = text; mark_dirty(); }
    const std::string& get_placeholder() const { return m_placeholder; }

    // Dropdown state
    bool is_open() const { return m_is_open; }
    void open();
    void close();
    void toggle() { if (m_is_open) close(); else open(); }

    // Max visible items before scrolling
    void set_max_visible_items(int count) { m_max_visible_items = count; }
    int get_max_visible_items() const { return m_max_visible_items; }

    // Callbacks
    std::function<void(const std::string& id, const std::string& label)> on_selection_changed;

    void render(UIRenderContext& ctx) override;

protected:
    void on_update(float dt, const UIInputState& input) override;
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;
    void on_click_internal() override;

private:
    void render_dropdown_list(UIRenderContext& ctx);
    int get_item_at_position(Vec2 pos) const;

    std::vector<DropdownItem> m_items;
    std::string m_selected_id;
    std::string m_placeholder = "Select...";

    bool m_is_open = false;
    int m_hovered_item = -1;
    int m_max_visible_items = 5;
    float m_item_height = 28.0f;
    float m_dropdown_scroll = 0.0f;

    Rect m_dropdown_bounds;
};

// Dialog button configuration
enum class DialogButtons : uint8_t {
    OK,             // Single OK button
    OKCancel,       // OK and Cancel buttons
    YesNo,          // Yes and No buttons
    YesNoCancel     // Yes, No, and Cancel buttons
};

// Dialog result
enum class DialogResult : uint8_t {
    None,
    OK,
    Cancel,
    Yes,
    No
};

// Modal dialog
class UIDialog : public UIElement {
public:
    UIDialog();

    // Configuration
    void set_title(const std::string& title) { m_title = title; mark_dirty(); }
    const std::string& get_title() const { return m_title; }

    void set_message(const std::string& message) { m_message = message; mark_dirty(); }
    const std::string& get_message() const { return m_message; }

    void set_buttons(DialogButtons buttons) { m_buttons = buttons; rebuild_buttons(); }
    DialogButtons get_buttons() const { return m_buttons; }

    // Show/hide
    void show();
    void hide();
    bool is_showing() const { return m_is_showing; }

    // Result
    DialogResult get_result() const { return m_result; }

    // Callbacks
    std::function<void(DialogResult)> on_result;

    void render(UIRenderContext& ctx) override;

protected:
    void on_update(float dt, const UIInputState& input) override;
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;

private:
    void rebuild_buttons();
    void handle_button_click(DialogResult result);

    std::string m_title;
    std::string m_message;
    DialogButtons m_buttons = DialogButtons::OK;
    DialogResult m_result = DialogResult::None;
    bool m_is_showing = false;

    // Button layout
    struct DialogButton {
        std::string label;
        DialogResult result;
        Rect bounds;
        bool hovered = false;
        bool pressed = false;
    };
    std::vector<DialogButton> m_dialog_buttons;

    float m_dialog_width = 350.0f;
    float m_button_height = 32.0f;
    float m_button_spacing = 8.0f;
    float m_padding = 20.0f;
};

} // namespace engine::ui
