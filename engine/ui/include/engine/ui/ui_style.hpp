#pragma once

#include <engine/ui/ui_types.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <unordered_map>
#include <optional>

namespace engine::ui {

using namespace engine::core;

// Style state for interactive elements
enum class StyleState : uint8_t {
    Normal,
    Hovered,
    Pressed,
    Disabled,
    Focused
};

// Individual style property with optional override per state
template<typename T>
struct StateProperty {
    T normal{};
    std::optional<T> hovered;
    std::optional<T> pressed;
    std::optional<T> disabled;
    std::optional<T> focused;

    StateProperty() = default;
    StateProperty(const T& value) : normal(value) {}

    const T& get(StyleState state) const {
        switch (state) {
            case StyleState::Hovered:  return hovered.value_or(normal);
            case StyleState::Pressed:  return pressed.value_or(hovered.value_or(normal));
            case StyleState::Disabled: return disabled.value_or(normal);
            case StyleState::Focused:  return focused.value_or(normal);
            default: return normal;
        }
    }

    void set_all(const T& value) {
        normal = value;
        hovered.reset();
        pressed.reset();
        disabled.reset();
        focused.reset();
    }
};

// Complete UI style definition
struct UIStyle {
    // Colors
    StateProperty<Vec4> background_color{Vec4(0.0f, 0.0f, 0.0f, 0.0f)};
    StateProperty<Vec4> border_color{Vec4(0.5f, 0.5f, 0.5f, 1.0f)};
    StateProperty<Vec4> text_color{Vec4(1.0f, 1.0f, 1.0f, 1.0f)};

    // Border
    float border_width = 0.0f;
    float border_radius = 0.0f;  // Corner radius

    // Spacing
    EdgeInsets padding;
    EdgeInsets margin;

    // Size constraints
    float min_width = 0.0f;
    float min_height = 0.0f;
    float max_width = 0.0f;   // 0 = no limit
    float max_height = 0.0f;  // 0 = no limit

    // Layout
    SizeMode width_mode = SizeMode::Fixed;
    SizeMode height_mode = SizeMode::Fixed;
    float width_percent = 100.0f;   // Used when mode is Percentage
    float height_percent = 100.0f;

    // Text
    FontHandle font = INVALID_FONT_HANDLE;
    float font_size = 14.0f;
    HAlign text_align = HAlign::Left;
    VAlign text_valign = VAlign::Center;
    bool text_wrap = false;

    // Effects
    float opacity = 1.0f;

    // Constructor with common defaults
    static UIStyle panel() {
        UIStyle s;
        s.background_color.normal = Vec4(0.15f, 0.15f, 0.15f, 0.9f);
        s.border_width = 1.0f;
        s.border_radius = 4.0f;
        s.padding = EdgeInsets(8.0f);
        return s;
    }

    static UIStyle button() {
        UIStyle s;
        s.background_color.normal = Vec4(0.3f, 0.3f, 0.3f, 1.0f);
        s.background_color.hovered = Vec4(0.4f, 0.4f, 0.4f, 1.0f);
        s.background_color.pressed = Vec4(0.2f, 0.2f, 0.2f, 1.0f);
        s.background_color.disabled = Vec4(0.2f, 0.2f, 0.2f, 0.5f);
        s.border_width = 1.0f;
        s.border_radius = 4.0f;
        s.padding = EdgeInsets(12.0f, 6.0f);
        s.text_align = HAlign::Center;
        s.min_width = 60.0f;
        s.min_height = 24.0f;
        return s;
    }

    static UIStyle label() {
        UIStyle s;
        s.text_color.normal = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        return s;
    }

    static UIStyle slider() {
        UIStyle s;
        s.background_color.normal = Vec4(0.2f, 0.2f, 0.2f, 1.0f);
        s.border_radius = 4.0f;
        s.min_height = 20.0f;
        return s;
    }
};

// Style class system for reusable styles
class UIStyleSheet {
public:
    void define_class(const std::string& name, const UIStyle& style) {
        m_classes[name] = style;
    }

    const UIStyle* get_class(const std::string& name) const {
        auto it = m_classes.find(name);
        return it != m_classes.end() ? &it->second : nullptr;
    }

    void remove_class(const std::string& name) {
        m_classes.erase(name);
    }

    // Merge multiple classes into one style
    UIStyle merge_classes(const std::vector<std::string>& class_names) const {
        UIStyle result;
        for (const auto& name : class_names) {
            if (auto* style = get_class(name)) {
                merge_into(result, *style);
            }
        }
        return result;
    }

private:
    void merge_into(UIStyle& target, const UIStyle& source) const {
        // This is a simplified merge - in a full implementation,
        // you'd track which properties are explicitly set
        target = source;
    }

    std::unordered_map<std::string, UIStyle> m_classes;
};

// Theme with predefined color palette
struct UITheme {
    Vec4 primary{0.2f, 0.5f, 0.9f, 1.0f};
    Vec4 secondary{0.6f, 0.6f, 0.6f, 1.0f};
    Vec4 success{0.2f, 0.8f, 0.2f, 1.0f};
    Vec4 warning{0.9f, 0.7f, 0.1f, 1.0f};
    Vec4 danger{0.9f, 0.2f, 0.2f, 1.0f};

    Vec4 background{0.1f, 0.1f, 0.1f, 1.0f};
    Vec4 surface{0.15f, 0.15f, 0.15f, 1.0f};
    Vec4 surface_variant{0.2f, 0.2f, 0.2f, 1.0f};

    Vec4 on_background{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 on_surface{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 on_primary{1.0f, 1.0f, 1.0f, 1.0f};

    float corner_radius = 4.0f;
    float border_width = 1.0f;

    // Apply theme to generate common styles
    UIStyle button_style() const {
        UIStyle s = UIStyle::button();
        s.background_color.normal = surface_variant;
        s.background_color.hovered = primary * 0.8f;
        s.background_color.pressed = primary * 0.6f;
        s.text_color.normal = on_surface;
        s.border_radius = corner_radius;
        return s;
    }

    UIStyle primary_button_style() const {
        UIStyle s = UIStyle::button();
        s.background_color.normal = primary;
        s.background_color.hovered = primary * 1.2f;
        s.background_color.pressed = primary * 0.8f;
        s.text_color.normal = on_primary;
        s.border_radius = corner_radius;
        return s;
    }

    UIStyle panel_style() const {
        UIStyle s = UIStyle::panel();
        s.background_color.normal = surface;
        s.border_color.normal = surface_variant;
        s.border_radius = corner_radius;
        s.border_width = border_width;
        return s;
    }

    static UITheme dark() {
        return UITheme();  // Default is dark
    }

    static UITheme light() {
        UITheme t;
        t.background = Vec4(0.95f, 0.95f, 0.95f, 1.0f);
        t.surface = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        t.surface_variant = Vec4(0.9f, 0.9f, 0.9f, 1.0f);
        t.on_background = Vec4(0.1f, 0.1f, 0.1f, 1.0f);
        t.on_surface = Vec4(0.1f, 0.1f, 0.1f, 1.0f);
        return t;
    }
};

} // namespace engine::ui
