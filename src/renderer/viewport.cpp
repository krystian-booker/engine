#include "renderer/viewport.h"

#include "renderer/vulkan_context.h"

#include <stdexcept>

void Viewport::Create(VulkanContext* context, u32 width, u32 height, Entity cameraEntity, ViewportType type) {
    if (!context) {
        throw std::invalid_argument("Viewport::Create requires valid context");
    }
    if (width == 0 || height == 0) {
        throw std::invalid_argument("Viewport::Create requires non-zero dimensions");
    }
    // Note: cameraEntity can be invalid initially (e.g., for Game viewport before a scene is loaded)
    // It should be set later via SetCamera()

    Destroy();

    m_Width = width;
    m_Height = height;
    m_CameraEntity = cameraEntity;
    m_Type = type;

    m_RenderTarget.Create(context, width, height);
}

void Viewport::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("Viewport::Resize requires non-zero dimensions");
    }

    m_Width = width;
    m_Height = height;
    m_RenderTarget.Resize(width, height);

    // Reset rendered flag since we've recreated the images
    m_HasBeenRendered = false;
}

void Viewport::Destroy() {
    m_RenderTarget.Destroy();
    m_Width = 0;
    m_Height = 0;
    m_CameraEntity = Entity::Invalid;
}
