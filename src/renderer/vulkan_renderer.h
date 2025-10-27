#pragma once

#include "core/types.h"
#include "core/math.h"
#include "core/resource_handle.h"
#include "renderer/frame_context.h"
#include "renderer/vulkan_framebuffer.h"
#include "renderer/vulkan_command_buffer.h"
#include "renderer/vulkan_pipeline.h"
#include "renderer/vulkan_render_pass.h"
#include "renderer/vulkan_swapchain.h"
#include "renderer/vulkan_descriptors.h"
#include "renderer/vulkan_depth.h"
#include "ecs/systems/render_system.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

class VulkanContext;
class Window;
class ECSCoordinator;

class VulkanRenderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer();

    void Init(VulkanContext* context, Window* window, ECSCoordinator* ecs);
    void Shutdown();

    void DrawFrame();
    void OnWindowResized();
    void SetCameraMatrices(const Mat4& view, const Mat4& projection);

    bool BeginFrame(FrameContext*& outFrame, u32& outImageIndex);
    void BeginDefaultRenderPass(FrameContext& frame, u32 imageIndex, const VkClearColorValue& clearColor);
    void EndDefaultRenderPass(FrameContext& frame);
    void EndFrame(FrameContext& frame, u32 imageIndex);
    VkCommandBuffer GetCommandBuffer(const FrameContext& frame) const { return frame.commandBuffer; }

private:
    void InitSwapchainResources();
    void DestroySwapchainResources();

    void CreateFrameContexts();
    void DestroyFrameContexts();
    void RecreateSwapchain();
    void ResizeImagesInFlight();
    void InitMeshResources();
    void DestroyMeshResources();
    void UpdateObjectUniforms(u32 frameIndex, const Mat4& modelMatrix);

    VulkanContext* m_Context = nullptr;
    Window* m_Window = nullptr;
    ECSCoordinator* m_ECS = nullptr;
    std::unique_ptr<RenderSystem> m_RenderSystem;

    VulkanSwapchain m_Swapchain;
    VulkanRenderPass m_RenderPass;
    VulkanFramebuffer m_Framebuffers;
    VulkanCommandBuffer m_CommandBuffers;
    VulkanPipeline m_Pipeline;
    VulkanDescriptors m_Descriptors;
    VulkanDepthBuffer m_DepthBuffer;

    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<FrameContext> m_Frames;
    std::vector<VkFence> m_ImagesInFlight;

    u32 m_CurrentFrame = 0;
    bool m_FramebufferResized = false;
    bool m_Initialized = false;

    f32 m_Rotation = 0.0f;
    Mat4 m_ViewMatrix = Mat4(1.0f);
    Mat4 m_ProjectionMatrix = Mat4(1.0f);
    bool m_HasCameraMatrices = false;

    MeshHandle m_ActiveMesh = MeshHandle::Invalid;
};
