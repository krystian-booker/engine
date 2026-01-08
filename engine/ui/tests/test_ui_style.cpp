#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/ui/ui_style.hpp>

using namespace engine::ui;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// StyleState enum Tests
// ============================================================================

TEST_CASE("StyleState enum", "[ui][style]") {
    REQUIRE(static_cast<uint8_t>(StyleState::Normal) == 0);
    REQUIRE(static_cast<uint8_t>(StyleState::Hovered) == 1);
    REQUIRE(static_cast<uint8_t>(StyleState::Pressed) == 2);
    REQUIRE(static_cast<uint8_t>(StyleState::Disabled) == 3);
    REQUIRE(static_cast<uint8_t>(StyleState::Focused) == 4);
}

// ============================================================================
// StateProperty Tests
// ============================================================================

TEST_CASE("StateProperty default", "[ui][style][property]") {
    StateProperty<float> prop;
    prop.normal = 1.0f;

    REQUIRE_THAT(prop.get(StyleState::Normal), WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(prop.get(StyleState::Hovered), WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(prop.get(StyleState::Pressed), WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(prop.get(StyleState::Disabled), WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(prop.get(StyleState::Focused), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("StateProperty with overrides", "[ui][style][property]") {
    StateProperty<float> prop;
    prop.normal = 1.0f;
    prop.hovered = 1.5f;
    prop.pressed = 0.8f;
    prop.disabled = 0.5f;
    prop.focused = 1.2f;

    REQUIRE_THAT(prop.get(StyleState::Normal), WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(prop.get(StyleState::Hovered), WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(prop.get(StyleState::Pressed), WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(prop.get(StyleState::Disabled), WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(prop.get(StyleState::Focused), WithinAbs(1.2f, 0.001f));
}

TEST_CASE("StateProperty pressed fallback", "[ui][style][property]") {
    StateProperty<float> prop;
    prop.normal = 1.0f;
    prop.hovered = 1.5f;
    // No pressed set - should fall back to hovered

    REQUIRE_THAT(prop.get(StyleState::Pressed), WithinAbs(1.5f, 0.001f));
}

TEST_CASE("StateProperty set_all", "[ui][style][property]") {
    StateProperty<float> prop;
    prop.normal = 1.0f;
    prop.hovered = 1.5f;
    prop.pressed = 0.8f;

    prop.set_all(2.0f);

    REQUIRE_THAT(prop.get(StyleState::Normal), WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(prop.get(StyleState::Hovered), WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(prop.get(StyleState::Pressed), WithinAbs(2.0f, 0.001f));
}

TEST_CASE("StateProperty color type", "[ui][style][property]") {
    StateProperty<Vec4> prop;
    prop.normal = Vec4{1.0f, 0.0f, 0.0f, 1.0f};  // Red
    prop.hovered = Vec4{0.0f, 1.0f, 0.0f, 1.0f}; // Green

    Vec4 normal = prop.get(StyleState::Normal);
    REQUIRE_THAT(normal.r, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(normal.g, WithinAbs(0.0f, 0.001f));

    Vec4 hovered = prop.get(StyleState::Hovered);
    REQUIRE_THAT(hovered.r, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(hovered.g, WithinAbs(1.0f, 0.001f));
}

// ============================================================================
// UIStyle Tests
// ============================================================================

TEST_CASE("UIStyle defaults", "[ui][style]") {
    UIStyle style;

    REQUIRE_THAT(style.border_width, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(style.border_radius, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(style.min_width, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(style.min_height, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(style.max_width, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(style.max_height, WithinAbs(0.0f, 0.001f));
    REQUIRE(style.width_mode == SizeMode::Fixed);
    REQUIRE(style.height_mode == SizeMode::Fixed);
    REQUIRE_THAT(style.width_percent, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(style.height_percent, WithinAbs(100.0f, 0.001f));
    REQUIRE(style.font == INVALID_FONT_HANDLE);
    REQUIRE_THAT(style.font_size, WithinAbs(14.0f, 0.001f));
    REQUIRE(style.text_align == HAlign::Left);
    REQUIRE(style.text_valign == VAlign::Center);
    REQUIRE_FALSE(style.text_wrap);
    REQUIRE_THAT(style.opacity, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(style.scale, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("UIStyle panel preset", "[ui][style][preset]") {
    UIStyle style = UIStyle::panel();

    REQUIRE_THAT(style.border_width, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(style.border_radius, WithinAbs(4.0f, 0.001f));
    REQUIRE_THAT(style.padding.left, WithinAbs(8.0f, 0.001f));
    REQUIRE_THAT(style.padding.top, WithinAbs(8.0f, 0.001f));

    Vec4 bg = style.background_color.get(StyleState::Normal);
    REQUIRE_THAT(bg.a, WithinAbs(0.9f, 0.001f));  // Semi-transparent
}

TEST_CASE("UIStyle button preset", "[ui][style][preset]") {
    UIStyle style = UIStyle::button();

    REQUIRE_THAT(style.border_width, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(style.border_radius, WithinAbs(4.0f, 0.001f));
    REQUIRE(style.text_align == HAlign::Center);
    REQUIRE_THAT(style.min_width, WithinAbs(60.0f, 0.001f));
    REQUIRE_THAT(style.min_height, WithinAbs(24.0f, 0.001f));

    // Check state colors differ
    Vec4 normal = style.background_color.get(StyleState::Normal);
    Vec4 hovered = style.background_color.get(StyleState::Hovered);
    Vec4 pressed = style.background_color.get(StyleState::Pressed);

    REQUIRE(hovered.r > normal.r);  // Hovered is brighter
    REQUIRE(pressed.r < normal.r);  // Pressed is darker
}

TEST_CASE("UIStyle label preset", "[ui][style][preset]") {
    UIStyle style = UIStyle::label();

    Vec4 text = style.text_color.get(StyleState::Normal);
    REQUIRE_THAT(text.r, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(text.g, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(text.b, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(text.a, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("UIStyle slider preset", "[ui][style][preset]") {
    UIStyle style = UIStyle::slider();

    REQUIRE_THAT(style.border_radius, WithinAbs(4.0f, 0.001f));
    REQUIRE_THAT(style.min_height, WithinAbs(20.0f, 0.001f));
}

TEST_CASE("UIStyle text_input preset", "[ui][style][preset]") {
    UIStyle style = UIStyle::text_input();

    REQUIRE_THAT(style.border_width, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(style.border_radius, WithinAbs(4.0f, 0.001f));
    REQUIRE_THAT(style.min_width, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(style.min_height, WithinAbs(28.0f, 0.001f));

    // Check focused border color differs
    Vec4 normal_border = style.border_color.get(StyleState::Normal);
    Vec4 focused_border = style.border_color.get(StyleState::Focused);

    // Focused should be blue-ish (higher blue component)
    REQUIRE(focused_border.b > normal_border.b);
}

// ============================================================================
// UIStyleSheet Tests
// ============================================================================

TEST_CASE("UIStyleSheet define and get class", "[ui][style][stylesheet]") {
    UIStyleSheet sheet;

    UIStyle custom;
    custom.border_radius = 10.0f;
    custom.font_size = 18.0f;

    sheet.define_class("custom-button", custom);

    const UIStyle* retrieved = sheet.get_class("custom-button");
    REQUIRE(retrieved != nullptr);
    REQUIRE_THAT(retrieved->border_radius, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(retrieved->font_size, WithinAbs(18.0f, 0.001f));
}

TEST_CASE("UIStyleSheet get nonexistent class", "[ui][style][stylesheet]") {
    UIStyleSheet sheet;

    const UIStyle* result = sheet.get_class("nonexistent");
    REQUIRE(result == nullptr);
}

TEST_CASE("UIStyleSheet remove class", "[ui][style][stylesheet]") {
    UIStyleSheet sheet;

    UIStyle custom;
    custom.border_radius = 10.0f;
    sheet.define_class("removable", custom);

    REQUIRE(sheet.get_class("removable") != nullptr);

    sheet.remove_class("removable");

    REQUIRE(sheet.get_class("removable") == nullptr);
}

TEST_CASE("UIStyleSheet merge classes", "[ui][style][stylesheet]") {
    UIStyleSheet sheet;

    UIStyle base;
    base.border_radius = 5.0f;
    base.font_size = 14.0f;
    sheet.define_class("base", base);

    UIStyle highlight;
    highlight.border_radius = 10.0f;  // Override
    highlight.font_size = 16.0f;
    sheet.define_class("highlight", highlight);

    UIStyle merged = sheet.merge_classes({"base", "highlight"});

    // Last class takes precedence
    REQUIRE_THAT(merged.border_radius, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(merged.font_size, WithinAbs(16.0f, 0.001f));
}

// ============================================================================
// UITheme Tests
// ============================================================================

TEST_CASE("UITheme default (dark)", "[ui][style][theme]") {
    UITheme theme;

    // Primary should be blue-ish
    REQUIRE(theme.primary.b > theme.primary.r);

    // Background should be dark
    REQUIRE(theme.background.r < 0.2f);

    // Text on background should be light
    REQUIRE(theme.on_background.r > 0.8f);

    REQUIRE_THAT(theme.corner_radius, WithinAbs(4.0f, 0.001f));
    REQUIRE_THAT(theme.border_width, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("UITheme dark factory", "[ui][style][theme]") {
    UITheme theme = UITheme::dark();

    // Should be same as default
    REQUIRE(theme.background.r < 0.2f);
    REQUIRE(theme.on_background.r > 0.8f);
}

TEST_CASE("UITheme light factory", "[ui][style][theme]") {
    UITheme theme = UITheme::light();

    // Background should be light
    REQUIRE(theme.background.r > 0.8f);

    // Text on background should be dark
    REQUIRE(theme.on_background.r < 0.2f);

    // Surface should be lighter than dark theme
    REQUIRE(theme.surface.r > 0.8f);
}

TEST_CASE("UITheme button_style", "[ui][style][theme]") {
    UITheme theme;
    UIStyle style = theme.button_style();

    Vec4 bg = style.background_color.get(StyleState::Normal);
    REQUIRE_THAT(bg.r, WithinAbs(theme.surface_variant.r, 0.001f));

    REQUIRE_THAT(style.border_radius, WithinAbs(theme.corner_radius, 0.001f));
}

TEST_CASE("UITheme primary_button_style", "[ui][style][theme]") {
    UITheme theme;
    UIStyle style = theme.primary_button_style();

    Vec4 bg = style.background_color.get(StyleState::Normal);
    REQUIRE_THAT(bg.r, WithinAbs(theme.primary.r, 0.001f));
    REQUIRE_THAT(bg.g, WithinAbs(theme.primary.g, 0.001f));
    REQUIRE_THAT(bg.b, WithinAbs(theme.primary.b, 0.001f));
}

TEST_CASE("UITheme panel_style", "[ui][style][theme]") {
    UITheme theme;
    UIStyle style = theme.panel_style();

    Vec4 bg = style.background_color.get(StyleState::Normal);
    REQUIRE_THAT(bg.r, WithinAbs(theme.surface.r, 0.001f));

    REQUIRE_THAT(style.border_radius, WithinAbs(theme.corner_radius, 0.001f));
    REQUIRE_THAT(style.border_width, WithinAbs(theme.border_width, 0.001f));
}

TEST_CASE("UITheme semantic colors", "[ui][style][theme]") {
    UITheme theme;

    // Success should be green-ish
    REQUIRE(theme.success.g > theme.success.r);
    REQUIRE(theme.success.g > theme.success.b);

    // Warning should be yellow/orange-ish
    REQUIRE(theme.warning.r > theme.warning.b);
    REQUIRE(theme.warning.g > theme.warning.b);

    // Danger should be red-ish
    REQUIRE(theme.danger.r > theme.danger.g);
    REQUIRE(theme.danger.r > theme.danger.b);
}
