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
#include "renderer/vulkan_staging_pool.h"
#include "renderer/vulkan_transfer_queue.h"
#include "renderer/vulkan_material_buffer.h"
#include "renderer/vulkan_texture.h"
#include "ecs/systems/render_system.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

class VulkanContext;
class Window;
class ECSCoordinator;
class CameraSystem;

class VulkanRenderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer();

    void Init(VulkanContext* context, Window* window, ECSCoordinator* ecs);
    void Shutdown();

    void DrawFrame();
    void OnWindowResized();

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
    void UpdateGlobalUniforms(u32 frameIndex);
    void PushModelMatrix(VkCommandBuffer commandBuffer, const Mat4& modelMatrix, u32 materialIndex);

    // Default texture initialization for bindless array
    void CreateDefaultTexture();
    void DestroyDefaultTexture();

    VulkanContext* m_Context = nullptr;
    Window* m_Window = nullptr;
    ECSCoordinator* m_ECS = nullptr;
    CameraSystem* m_CameraSystem = nullptr;
    std::unique_ptr<RenderSystem> m_RenderSystem;

    VulkanSwapchain m_Swapchain;
    VulkanRenderPass m_RenderPass;
    VulkanFramebuffer m_Framebuffers;
    VulkanCommandBuffer m_CommandBuffers;
    VulkanPipeline m_Pipeline;
    VulkanDescriptors m_Descriptors;
    VulkanDepthBuffer m_DepthBuffer;

    // Async upload pipeline
    VulkanStagingPool m_StagingPool;
    VulkanTransferQueue m_TransferQueue;

    // Material system
    VulkanMaterialBuffer m_MaterialBuffer;

    // Default texture for bindless array (index 0)
    std::unique_ptr<VulkanTexture> m_DefaultTexture;

    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<FrameContext> m_Frames;
    std::vector<VkFence> m_ImagesInFlight;

    u32 m_CurrentFrame = 0;
    bool m_FramebufferResized = false;
    bool m_Initialized = false;

    f32 m_Rotation = 0.0f;

    MeshHandle m_ActiveMesh = MeshHandle::Invalid;
};
