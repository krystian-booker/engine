#pragma once

#include "core/types.h"
#include "ecs/entity.h"
#include "renderer/vulkan_render_target.h"

#include <memory>

class VulkanContext;

// Viewport type distinguishes between scene and game viewports
enum class ViewportType : u8 {
    Scene,  // Editor scene view (uses editor camera)
    Game    // Game view (uses active game camera)
};

// Viewport represents a single rendering view with its own camera and render target
class Viewport {
public:
    Viewport() = default;
    ~Viewport() = default;

    // Create viewport with specified dimensions and camera
    void Create(VulkanContext* context, u32 width, u32 height, Entity cameraEntity, ViewportType type);

    // Resize viewport render target
    void Resize(u32 width, u32 height);

    // Destroy viewport resources
    void Destroy();

    // Accessors
    u32 GetID() const { return m_ID; }
    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }
    Entity GetCamera() const { return m_CameraEntity; }
    ViewportType GetType() const { return m_Type; }
    VulkanRenderTarget& GetRenderTarget() { return m_RenderTarget; }
    const VulkanRenderTarget& GetRenderTarget() const { return m_RenderTarget; }

    // Mutators
    void SetCamera(Entity cameraEntity) { m_CameraEntity = cameraEntity; }
    void SetID(u32 id) { m_ID = id; }

    bool IsValid() const {
        return m_RenderTarget.IsValid();
    }

    bool IsReadyToRender() const {
        return m_RenderTarget.IsValid() && m_CameraEntity.IsValid();
    }

    bool HasBeenRendered() const {
        return m_HasBeenRendered;
    }

    void MarkAsRendered() {
        m_HasBeenRendered = true;
    }

    // Prevent copying
    Viewport(const Viewport&) = delete;
    Viewport& operator=(const Viewport&) = delete;

    // Allow moving
    Viewport(Viewport&& other) noexcept = default;
    Viewport& operator=(Viewport&& other) noexcept = default;

private:
    u32 m_ID = 0;
    u32 m_Width = 0;
    u32 m_Height = 0;
    Entity m_CameraEntity = Entity::Invalid;
    ViewportType m_Type = ViewportType::Scene;
    VulkanRenderTarget m_RenderTarget;
    bool m_HasBeenRendered = false;
};
