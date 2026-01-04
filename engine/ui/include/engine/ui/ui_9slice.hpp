#pragma once

#include <engine/ui/ui_element.hpp>
#include <engine/render/types.hpp>

namespace engine::ui {

// Border insets defining the 9-slice regions (in pixels relative to texture)
struct NineSliceBorder {
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;

    NineSliceBorder() = default;
    explicit NineSliceBorder(float all) : left(all), right(all), top(all), bottom(all) {}
    NineSliceBorder(float horizontal, float vertical)
        : left(horizontal), right(horizontal), top(vertical), bottom(vertical) {}
    NineSliceBorder(float l, float t, float r, float b)
        : left(l), right(r), top(t), bottom(b) {}

    float horizontal() const { return left + right; }
    float vertical() const { return top + bottom; }
};

// Fill mode for the center region
enum class NineSliceFillMode : uint8_t {
    Stretch,    // Stretch center to fill (default)
    Tile        // Tile center pattern (not yet implemented)
};

// 9-slice image element - renders a texture with preserved corner proportions
class UI9SliceImage : public UIElement {
public:
    UI9SliceImage();
    UI9SliceImage(render::TextureHandle texture, const NineSliceBorder& border);

    void set_texture(render::TextureHandle texture) { m_texture = texture; mark_dirty(); }
    render::TextureHandle get_texture() const { return m_texture; }

    void set_border(const NineSliceBorder& border) { m_border = border; mark_dirty(); }
    const NineSliceBorder& get_border() const { return m_border; }

    // Set border from pixel values - requires texture dimensions
    void set_border_pixels(float left, float top, float right, float bottom);

    // Texture dimensions (needed for correct UV calculation)
    void set_texture_size(uint32_t width, uint32_t height);
    uint32_t get_texture_width() const { return m_texture_width; }
    uint32_t get_texture_height() const { return m_texture_height; }

    void set_tint(const Vec4& tint) { m_tint = tint; mark_dirty(); }
    const Vec4& get_tint() const { return m_tint; }

    void set_fill_mode(NineSliceFillMode mode) { m_fill_mode = mode; }
    NineSliceFillMode get_fill_mode() const { return m_fill_mode; }

protected:
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;

private:
    render::TextureHandle m_texture;
    NineSliceBorder m_border;
    Vec4 m_tint{1.0f};
    NineSliceFillMode m_fill_mode = NineSliceFillMode::Stretch;
    uint32_t m_texture_width = 0;
    uint32_t m_texture_height = 0;
};

} // namespace engine::ui
