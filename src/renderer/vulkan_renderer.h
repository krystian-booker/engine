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
#include "renderer/vulkan_light_culling.h"
#include "renderer/vulkan_evsm_shadow.h"
#include "renderer/vulkan_shadow_renderer.h"
#include "ecs/systems/render_system.h"

#ifdef _DEBUG
#include "renderer/imgui_layer.h"
#endif

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

class VulkanContext;
class Window;
class ECSCoordinator;
class CameraSystem;
class ShadowSystem;
class SceneManager;
class Viewport;
class ViewportManager;

class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    void Init(VulkanContext* context, Window* window, ECSCoordinator* ecs, SceneManager* sceneManager);
    void Shutdown();

    void DrawFrame(ViewportManager* viewportManager = nullptr);
    void OnWindowResized();

#ifdef _DEBUG
    // Check if user requested to change project via ImGui
    bool ShouldChangeProject() const;

    // Get ImGui layer for editor functionality
    ImGuiLayer* GetImGuiLayer() { return &m_ImGuiLayer; }
#endif

    bool BeginFrame(FrameContext*& outFrame, u32& outImageIndex);
    void BeginDefaultRenderPass(FrameContext& frame, u32 imageIndex, const VkClearColorValue& clearColor);
    void EndDefaultRenderPass(FrameContext& frame);
    void EndFrame(FrameContext& frame, u32 imageIndex, VkSemaphore* waitSemaphore = nullptr);
    VkCommandBuffer GetCommandBuffer(const FrameContext& frame) const { return frame.commandBuffer; }

    // Multi-viewport rendering
    void RenderViewport(VkCommandBuffer commandBuffer, Viewport& viewport, Entity cameraEntity, u32 frameIndex);
    void BeginOffscreenRenderPass(VkCommandBuffer commandBuffer, Viewport& viewport, const VkClearColorValue& clearColor);
    void EndOffscreenRenderPass(VkCommandBuffer commandBuffer);

    // Direct swapchain rendering (for Release builds without viewport system)
    void RenderDirectToSwapchain(VkCommandBuffer commandBuffer, u32 frameIndex);

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
    void UpdateGlobalUniformsWithCamera(u32 frameIndex, Entity cameraEntity, u32 viewportWidth, u32 viewportHeight);
    void PushModelMatrix(VkCommandBuffer commandBuffer, const Mat4& modelMatrix, u32 materialIndex, u32 screenWidth, u32 screenHeight);
    void RenderScene(VkCommandBuffer commandBuffer, u32 frameIndex, u32 screenWidth, u32 screenHeight);

    // Forward+ light data upload
    void UploadLightDataForwardPlus();
    u32 GetLightCount() const;

    // Depth prepass for Forward+
    void CreateDepthPrepassResources();
    void DestroyDepthPrepassResources();
    void RenderDepthPrepass(VkCommandBuffer commandBuffer, u32 frameIndex);
    void TransitionDepthForRead(VkCommandBuffer commandBuffer);
    void TransitionDepthForWrite(VkCommandBuffer commandBuffer);

    // Lazy initialization of offscreen pipelines when first viewport is rendered
    void EnsureOffscreenPipelinesInitialized(VkRenderPass offscreenRenderPass, VkExtent2D extent);

    // Default texture initialization for bindless array
    void CreateDefaultTexture();
    void DestroyDefaultTexture();

    // IBL placeholder texture initialization (for scenes without IBL)
    void CreateIBLPlaceholders();
    void DestroyIBLPlaceholders();

    VulkanContext* m_Context = nullptr;
    Window* m_Window = nullptr;
    ECSCoordinator* m_ECS = nullptr;
    SceneManager* m_SceneManager = nullptr;
    CameraSystem* m_CameraSystem = nullptr;
    std::unique_ptr<RenderSystem> m_RenderSystem;

    VulkanSwapchain m_Swapchain;
    VulkanRenderPass m_RenderPass;
    VulkanFramebuffer m_Framebuffers;
    VulkanCommandBuffer m_CommandBuffers;
    VulkanPipeline m_Pipeline;
    VulkanDescriptors m_Descriptors;
    VulkanDepthBuffer m_DepthBuffer;

    // Depth prepass resources
    VkRenderPass m_DepthPrepassRenderPass = VK_NULL_HANDLE;
    VkPipeline m_DepthPrepassPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_DepthPrepassPipelineLayout = VK_NULL_HANDLE;
    VkFramebuffer m_DepthPrepassFramebuffer = VK_NULL_HANDLE;

    // Async upload pipeline
    VulkanStagingPool m_StagingPool;
    VulkanTransferQueue m_TransferQueue;

    // Forward+ light culling system
    std::unique_ptr<VulkanLightCulling> m_LightCulling;

    // Shadow rendering system
    std::unique_ptr<VulkanShadowRenderer> m_ShadowRenderer;
    std::unique_ptr<ShadowSystem> m_ShadowSystem;

    // EVSM shadow filtering system
    std::unique_ptr<VulkanEVSMShadow> m_EVSMShadow;

    // Default texture for bindless array (index 0)
    std::unique_ptr<VulkanTexture> m_DefaultTexture;

    // IBL placeholder textures (for scenes without IBL)
    std::unique_ptr<VulkanTexture> m_PlaceholderIrradianceMap;
    std::unique_ptr<VulkanTexture> m_PlaceholderPrefilteredMap;
    std::unique_ptr<VulkanTexture> m_PlaceholderBRDFLUT;

#ifdef _DEBUG
    // ImGui layer for debug UI (only in debug builds)
    ImGuiLayer m_ImGuiLayer;
#endif

    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<FrameContext> m_Frames;
    std::vector<VkFence> m_ImagesInFlight;

    // Per-swapchain-image semaphores to avoid reuse issues
    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    u32 m_CurrentSemaphoreIndex = 0;  // Round-robin semaphore selection

    // Viewport rendering command buffers (one per frame in flight)
    VkCommandPool m_ViewportCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_ViewportCommandBuffers;  // One per frame
    std::vector<VkSemaphore> m_ViewportFinishedSemaphores;  // Signal when viewport rendering done
    std::vector<VkFence> m_ViewportFences;  // One per frame to track completion

    u32 m_CurrentFrame = 0;
    bool m_FramebufferResized = false;
    bool m_Initialized = false;
    bool m_OffscreenPipelinesInitialized = false;

    MeshHandle m_ActiveMesh = MeshHandle::Invalid;
};
