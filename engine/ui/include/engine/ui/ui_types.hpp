#pragma once

#include <engine/core/math.hpp>
#include <cstdint>
#include <string>
#include <functional>

namespace engine::ui {

using namespace engine::core;

// Forward declarations
class UIElement;
class UICanvas;
class UIContext;

// Handle types
using FontHandle = uint32_t;
constexpr FontHandle INVALID_FONT_HANDLE = 0;

// Anchor point for positioning
enum class Anchor : uint8_t {
    TopLeft,
    Top,
    TopRight,
    Left,
    Center,
    Right,
    BottomLeft,
    Bottom,
    BottomRight
};

// Horizontal alignment
enum class HAlign : uint8_t {
    Left,
    Center,
    Right
};

// Vertical alignment
enum class VAlign : uint8_t {
    Top,
    Center,
    Bottom
};

// Layout direction
enum class LayoutDirection : uint8_t {
    Horizontal,
    Vertical
};

// Size mode
enum class SizeMode : uint8_t {
    Fixed,          // Use explicit size
    FitContent,     // Size to fit children/content
    FillParent,     // Fill available space
    Percentage      // Percentage of parent
};

// Overflow behavior
enum class Overflow : uint8_t {
    Visible,    // Content extends beyond bounds
    Hidden,     // Content clipped to bounds
    Scroll      // Enable scrolling
};

// Input state
struct UIInputState {
    // Mouse input
    Vec2 mouse_position{0.0f};
    Vec2 mouse_delta{0.0f};
    Vec2 scroll_delta{0.0f};
    bool mouse_buttons[3] = {false, false, false};  // Left, Right, Middle
    bool prev_mouse_buttons[3] = {false, false, false};

    // Keyboard input for text entry
    std::string text_input;           // Characters typed this frame (UTF-8)
    bool key_backspace = false;
    bool key_delete = false;
    bool key_left = false;
    bool key_right = false;
    bool key_home = false;
    bool key_end = false;
    bool key_enter = false;

    bool is_mouse_down(int button) const { return mouse_buttons[button]; }
    bool is_mouse_up(int button) const { return !mouse_buttons[button]; }
    bool was_mouse_pressed(int button) const { return mouse_buttons[button] && !prev_mouse_buttons[button]; }
    bool was_mouse_released(int button) const { return !mouse_buttons[button] && prev_mouse_buttons[button]; }
};

// Rectangle for UI bounds
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    Rect() = default;
    Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), width(w_), height(h_) {}
    Rect(Vec2 pos, Vec2 size) : x(pos.x), y(pos.y), width(size.x), height(size.y) {}

    Vec2 position() const { return Vec2(x, y); }
    Vec2 size() const { return Vec2(width, height); }
    Vec2 center() const { return Vec2(x + width * 0.5f, y + height * 0.5f); }

    float left() const { return x; }
    float right() const { return x + width; }
    float top() const { return y; }
    float bottom() const { return y + height; }

    bool contains(Vec2 point) const {
        return point.x >= x && point.x <= x + width &&
               point.y >= y && point.y <= y + height;
    }

    bool intersects(const Rect& other) const {
        return x < other.right() && right() > other.x &&
               y < other.bottom() && bottom() > other.y;
    }

    Rect intersect(const Rect& other) const {
        float new_x = std::max(x, other.x);
        float new_y = std::max(y, other.y);
        float new_right = std::min(right(), other.right());
        float new_bottom = std::min(bottom(), other.bottom());

        if (new_right <= new_x || new_bottom <= new_y) {
            return Rect();
        }
        return Rect(new_x, new_y, new_right - new_x, new_bottom - new_y);
    }

    static Rect from_min_max(Vec2 min, Vec2 max) {
        return Rect(min.x, min.y, max.x - min.x, max.y - min.y);
    }
};

// Padding/margin with 4 sides
struct EdgeInsets {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    EdgeInsets() = default;
    EdgeInsets(float all) : left(all), top(all), right(all), bottom(all) {}
    EdgeInsets(float horizontal, float vertical)
        : left(horizontal), top(vertical), right(horizontal), bottom(vertical) {}
    EdgeInsets(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}

    float horizontal() const { return left + right; }
    float vertical() const { return top + bottom; }
    Vec2 total() const { return Vec2(horizontal(), vertical()); }
};

// UI event callbacks
using ClickCallback = std::function<void()>;
using HoverCallback = std::function<void(bool entered)>;
using ValueChangedCallback = std::function<void(float value)>;
using TextChangedCallback = std::function<void(const std::string& text)>;

// UI vertex for rendering
struct UIVertex {
    Vec2 position;
    Vec2 texcoord;
    uint32_t color;  // RGBA packed
};

// Draw command for batched rendering
struct UIDrawCommand {
    uint32_t texture_id = 0;
    uint32_t vertex_offset = 0;
    uint32_t vertex_count = 0;
    uint32_t index_offset = 0;
    uint32_t index_count = 0;
    Rect clip_rect;
    bool is_text = false;
};

// Pack RGBA color to uint32
inline uint32_t pack_color(const Vec4& color) {
    uint8_t r = static_cast<uint8_t>(color.r * 255.0f);
    uint8_t g = static_cast<uint8_t>(color.g * 255.0f);
    uint8_t b = static_cast<uint8_t>(color.b * 255.0f);
    uint8_t a = static_cast<uint8_t>(color.a * 255.0f);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

// Unpack uint32 to RGBA color
inline Vec4 unpack_color(uint32_t packed) {
    float r = static_cast<float>(packed & 0xFF) / 255.0f;
    float g = static_cast<float>((packed >> 8) & 0xFF) / 255.0f;
    float b = static_cast<float>((packed >> 16) & 0xFF) / 255.0f;
    float a = static_cast<float>((packed >> 24) & 0xFF) / 255.0f;
    return Vec4(r, g, b, a);
}

} // namespace engine::ui
