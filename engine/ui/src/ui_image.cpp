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

    if (m_texture.valid()) {
        ctx.draw_image(m_content_bounds, m_texture, m_tint);
    }
}

Vec2 UIImage::on_measure(Vec2 available_size) {
    return UIElement::on_measure(available_size);
}

} // namespace engine::ui
