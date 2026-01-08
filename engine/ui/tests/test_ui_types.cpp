#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/ui/ui_types.hpp>

using namespace engine::ui;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Anchor enum Tests
// ============================================================================

TEST_CASE("Anchor enum", "[ui][types]") {
    REQUIRE(static_cast<uint8_t>(Anchor::TopLeft) == 0);
    REQUIRE(static_cast<uint8_t>(Anchor::Top) == 1);
    REQUIRE(static_cast<uint8_t>(Anchor::TopRight) == 2);
    REQUIRE(static_cast<uint8_t>(Anchor::Left) == 3);
    REQUIRE(static_cast<uint8_t>(Anchor::Center) == 4);
    REQUIRE(static_cast<uint8_t>(Anchor::Right) == 5);
    REQUIRE(static_cast<uint8_t>(Anchor::BottomLeft) == 6);
    REQUIRE(static_cast<uint8_t>(Anchor::Bottom) == 7);
    REQUIRE(static_cast<uint8_t>(Anchor::BottomRight) == 8);
}

// ============================================================================
// HAlign enum Tests
// ============================================================================

TEST_CASE("HAlign enum", "[ui][types]") {
    REQUIRE(static_cast<uint8_t>(HAlign::Left) == 0);
    REQUIRE(static_cast<uint8_t>(HAlign::Center) == 1);
    REQUIRE(static_cast<uint8_t>(HAlign::Right) == 2);
}

// ============================================================================
// VAlign enum Tests
// ============================================================================

TEST_CASE("VAlign enum", "[ui][types]") {
    REQUIRE(static_cast<uint8_t>(VAlign::Top) == 0);
    REQUIRE(static_cast<uint8_t>(VAlign::Center) == 1);
    REQUIRE(static_cast<uint8_t>(VAlign::Bottom) == 2);
}

// ============================================================================
// LayoutDirection enum Tests
// ============================================================================

TEST_CASE("LayoutDirection enum", "[ui][types]") {
    REQUIRE(static_cast<uint8_t>(LayoutDirection::Horizontal) == 0);
    REQUIRE(static_cast<uint8_t>(LayoutDirection::Vertical) == 1);
}

// ============================================================================
// SizeMode enum Tests
// ============================================================================

TEST_CASE("SizeMode enum", "[ui][types]") {
    REQUIRE(static_cast<uint8_t>(SizeMode::Fixed) == 0);
    REQUIRE(static_cast<uint8_t>(SizeMode::FitContent) == 1);
    REQUIRE(static_cast<uint8_t>(SizeMode::FillParent) == 2);
    REQUIRE(static_cast<uint8_t>(SizeMode::Percentage) == 3);
}

// ============================================================================
// Overflow enum Tests
// ============================================================================

TEST_CASE("Overflow enum", "[ui][types]") {
    REQUIRE(static_cast<uint8_t>(Overflow::Visible) == 0);
    REQUIRE(static_cast<uint8_t>(Overflow::Hidden) == 1);
    REQUIRE(static_cast<uint8_t>(Overflow::Scroll) == 2);
}

// ============================================================================
// NavDirection enum Tests
// ============================================================================

TEST_CASE("NavDirection enum", "[ui][types]") {
    REQUIRE(static_cast<uint8_t>(NavDirection::None) == 0);
    REQUIRE(static_cast<uint8_t>(NavDirection::Up) == 1);
    REQUIRE(static_cast<uint8_t>(NavDirection::Down) == 2);
    REQUIRE(static_cast<uint8_t>(NavDirection::Left) == 3);
    REQUIRE(static_cast<uint8_t>(NavDirection::Right) == 4);
}

// ============================================================================
// Rect Tests
// ============================================================================

TEST_CASE("Rect default constructor", "[ui][types][rect]") {
    Rect rect;

    REQUIRE_THAT(rect.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(rect.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(rect.width, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(rect.height, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Rect parameterized constructor", "[ui][types][rect]") {
    Rect rect(10.0f, 20.0f, 100.0f, 50.0f);

    REQUIRE_THAT(rect.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(rect.y, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(rect.width, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(rect.height, WithinAbs(50.0f, 0.001f));
}

TEST_CASE("Rect Vec2 constructor", "[ui][types][rect]") {
    Rect rect(Vec2{10.0f, 20.0f}, Vec2{100.0f, 50.0f});

    REQUIRE_THAT(rect.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(rect.y, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(rect.width, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(rect.height, WithinAbs(50.0f, 0.001f));
}

TEST_CASE("Rect accessors", "[ui][types][rect]") {
    Rect rect(10.0f, 20.0f, 100.0f, 50.0f);

    Vec2 pos = rect.position();
    REQUIRE_THAT(pos.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(pos.y, WithinAbs(20.0f, 0.001f));

    Vec2 size = rect.size();
    REQUIRE_THAT(size.x, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(size.y, WithinAbs(50.0f, 0.001f));

    Vec2 center = rect.center();
    REQUIRE_THAT(center.x, WithinAbs(60.0f, 0.001f));
    REQUIRE_THAT(center.y, WithinAbs(45.0f, 0.001f));

    REQUIRE_THAT(rect.left(), WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(rect.right(), WithinAbs(110.0f, 0.001f));
    REQUIRE_THAT(rect.top(), WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(rect.bottom(), WithinAbs(70.0f, 0.001f));
}

TEST_CASE("Rect contains point", "[ui][types][rect]") {
    Rect rect(0.0f, 0.0f, 100.0f, 100.0f);

    REQUIRE(rect.contains(Vec2{50.0f, 50.0f}));
    REQUIRE(rect.contains(Vec2{0.0f, 0.0f}));
    REQUIRE(rect.contains(Vec2{100.0f, 100.0f}));

    REQUIRE_FALSE(rect.contains(Vec2{-1.0f, 50.0f}));
    REQUIRE_FALSE(rect.contains(Vec2{50.0f, 101.0f}));
    REQUIRE_FALSE(rect.contains(Vec2{-10.0f, -10.0f}));
}

TEST_CASE("Rect intersects", "[ui][types][rect]") {
    Rect rect1(0.0f, 0.0f, 100.0f, 100.0f);
    Rect rect2(50.0f, 50.0f, 100.0f, 100.0f);
    Rect rect3(200.0f, 200.0f, 50.0f, 50.0f);

    REQUIRE(rect1.intersects(rect2));
    REQUIRE(rect2.intersects(rect1));

    REQUIRE_FALSE(rect1.intersects(rect3));
    REQUIRE_FALSE(rect3.intersects(rect1));
}

TEST_CASE("Rect from_min_max", "[ui][types][rect]") {
    Rect rect = Rect::from_min_max(Vec2{10.0f, 20.0f}, Vec2{110.0f, 70.0f});

    REQUIRE_THAT(rect.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(rect.y, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(rect.width, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(rect.height, WithinAbs(50.0f, 0.001f));
}

// ============================================================================
// EdgeInsets Tests
// ============================================================================

TEST_CASE("EdgeInsets default constructor", "[ui][types][insets]") {
    EdgeInsets insets;

    REQUIRE_THAT(insets.left, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(insets.top, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(insets.right, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(insets.bottom, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("EdgeInsets uniform constructor", "[ui][types][insets]") {
    EdgeInsets insets(10.0f);

    REQUIRE_THAT(insets.left, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(insets.top, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(insets.right, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(insets.bottom, WithinAbs(10.0f, 0.001f));
}

TEST_CASE("EdgeInsets symmetric constructor", "[ui][types][insets]") {
    EdgeInsets insets(10.0f, 20.0f);

    REQUIRE_THAT(insets.left, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(insets.top, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(insets.right, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(insets.bottom, WithinAbs(20.0f, 0.001f));
}

TEST_CASE("EdgeInsets individual constructor", "[ui][types][insets]") {
    EdgeInsets insets(5.0f, 10.0f, 15.0f, 20.0f);

    REQUIRE_THAT(insets.left, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(insets.top, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(insets.right, WithinAbs(15.0f, 0.001f));
    REQUIRE_THAT(insets.bottom, WithinAbs(20.0f, 0.001f));
}

TEST_CASE("EdgeInsets total calculations", "[ui][types][insets]") {
    EdgeInsets insets(5.0f, 10.0f, 15.0f, 20.0f);

    REQUIRE_THAT(insets.horizontal(), WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(insets.vertical(), WithinAbs(30.0f, 0.001f));

    Vec2 total = insets.total();
    REQUIRE_THAT(total.x, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(total.y, WithinAbs(30.0f, 0.001f));
}

// ============================================================================
// UIInputState Tests
// ============================================================================

TEST_CASE("UIInputState defaults", "[ui][types][input]") {
    UIInputState input;

    REQUIRE_THAT(input.mouse_position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(input.mouse_position.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_FALSE(input.mouse_buttons[0]);
    REQUIRE_FALSE(input.mouse_buttons[1]);
    REQUIRE_FALSE(input.mouse_buttons[2]);
    REQUIRE(input.text_input.empty());
    REQUIRE_FALSE(input.key_backspace);
    REQUIRE_FALSE(input.key_enter);
    REQUIRE_FALSE(input.nav_up);
    REQUIRE_FALSE(input.nav_confirm);
}

TEST_CASE("UIInputState mouse helpers", "[ui][types][input]") {
    UIInputState input;

    // Set up mouse state
    input.mouse_buttons[0] = true;
    input.prev_mouse_buttons[0] = false;

    REQUIRE(input.is_mouse_down(0));
    REQUIRE(input.was_mouse_pressed(0));
    REQUIRE_FALSE(input.was_mouse_released(0));

    // Simulate release
    input.prev_mouse_buttons[0] = true;
    input.mouse_buttons[0] = false;

    REQUIRE(input.is_mouse_up(0));
    REQUIRE(input.was_mouse_released(0));
    REQUIRE_FALSE(input.was_mouse_pressed(0));
}

TEST_CASE("UIInputState navigation helpers", "[ui][types][input]") {
    UIInputState input;

    // Set up nav state
    input.nav_up = true;
    input.prev_nav_up = false;
    input.nav_confirm = true;
    input.prev_nav_confirm = false;

    REQUIRE(input.was_nav_pressed(NavDirection::Up));
    REQUIRE_FALSE(input.was_nav_pressed(NavDirection::Down));
    REQUIRE(input.was_confirm_pressed());
    REQUIRE_FALSE(input.was_cancel_pressed());

    NavDirection dir = input.get_nav_direction();
    REQUIRE(dir == NavDirection::Up);
}

// ============================================================================
// Color Packing Tests
// ============================================================================

TEST_CASE("pack_color and unpack_color", "[ui][types][color]") {
    Vec4 original{1.0f, 0.5f, 0.25f, 0.75f};

    uint32_t packed = pack_color(original);
    Vec4 unpacked = unpack_color(packed);

    // Allow for some precision loss from 8-bit quantization
    REQUIRE_THAT(unpacked.r, WithinAbs(1.0f, 0.01f));
    REQUIRE_THAT(unpacked.g, WithinAbs(0.5f, 0.01f));
    REQUIRE_THAT(unpacked.b, WithinAbs(0.25f, 0.01f));
    REQUIRE_THAT(unpacked.a, WithinAbs(0.75f, 0.01f));
}

TEST_CASE("pack_color white", "[ui][types][color]") {
    Vec4 white{1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t packed = pack_color(white);

    REQUIRE(packed == 0xFFFFFFFF);
}

TEST_CASE("pack_color black transparent", "[ui][types][color]") {
    Vec4 black{0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t packed = pack_color(black);

    REQUIRE(packed == 0x00000000);
}

// ============================================================================
// UIVertex Tests
// ============================================================================

TEST_CASE("UIVertex structure", "[ui][types][vertex]") {
    UIVertex vertex;
    vertex.position = Vec2{100.0f, 200.0f};
    vertex.texcoord = Vec2{0.5f, 0.5f};
    vertex.color = pack_color(Vec4{1.0f, 0.0f, 0.0f, 1.0f});

    REQUIRE_THAT(vertex.position.x, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(vertex.position.y, WithinAbs(200.0f, 0.001f));
    REQUIRE_THAT(vertex.texcoord.x, WithinAbs(0.5f, 0.001f));
}

// ============================================================================
// UIDrawCommand Tests
// ============================================================================

TEST_CASE("UIDrawCommand defaults", "[ui][types][draw]") {
    UIDrawCommand cmd;

    REQUIRE(cmd.texture_id == 0);
    REQUIRE(cmd.vertex_offset == 0);
    REQUIRE(cmd.vertex_count == 0);
    REQUIRE(cmd.index_offset == 0);
    REQUIRE(cmd.index_count == 0);
    REQUIRE_FALSE(cmd.is_text);
}

TEST_CASE("UIDrawCommand configuration", "[ui][types][draw]") {
    UIDrawCommand cmd;
    cmd.texture_id = 5;
    cmd.vertex_offset = 100;
    cmd.vertex_count = 6;
    cmd.index_offset = 200;
    cmd.index_count = 12;
    cmd.clip_rect = Rect(0.0f, 0.0f, 1920.0f, 1080.0f);
    cmd.is_text = true;

    REQUIRE(cmd.texture_id == 5);
    REQUIRE(cmd.vertex_count == 6);
    REQUIRE(cmd.index_count == 12);
    REQUIRE(cmd.is_text);
    REQUIRE_THAT(cmd.clip_rect.width, WithinAbs(1920.0f, 0.001f));
}
