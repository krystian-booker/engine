#include <engine/ui/ui_elements.hpp>
#include <engine/ui/ui_renderer.hpp>

namespace engine::ui {

UIImage::UIImage() {
    // Images are not interactive by default
}

UIImage::UIImage(render::TextureHandle texture) : UIImage() {
    m_texture = texture;
}

void UIImage::on_render(UIRenderContext& ctx) {
    render_background(ctx, m_bounds);

    if (m_texture.idx != bgfx::kInvalidHandle) {
        ctx.draw_image(m_content_bounds, m_texture, m_tint);
    }
}

Vec2 UIImage::on_measure(Vec2 available_size) {
    Vec2 size = UIElement::on_measure(available_size);

    // If preserving aspect ratio and we have a texture, adjust size
    // This would require knowing the texture dimensions
    // For now, just use the explicit size

    return size;
}

} // namespace engine::ui
