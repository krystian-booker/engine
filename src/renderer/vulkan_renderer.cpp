#include "renderer/vulkan_renderer.h"

#include "core/math.h"
#include "core/time.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/systems/camera_system.h"
#include "ecs/systems/render_system.h"
#include "ecs/systems/shadow_system.h"
#include "ecs/components/camera.h"
#include "ecs/components/transform.h"
#include "ecs/components/light.h"
#include "platform/window.h"
#include "renderer/vulkan_context.h"
#include "renderer/uniform_buffers.h"
#include "renderer/vertex.h"
#include "renderer/push_constants.h"
#include "renderer/material_buffer.h"
#include "renderer/viewport.h"
#include "renderer/viewport_manager.h"
#include "renderer/shadow_profiler.h"
#include "resources/mesh_manager.h"
#include "resources/texture_manager.h"
#include "resources/material_manager.h"
#include "core/texture_data.h"

#include <stdexcept>
#include <iostream>
#include <fstream>

namespace {

VkFenceCreateInfo CreateFenceInfo() {
    VkFenceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    return info;
}

VkSemaphoreCreateInfo CreateSemaphoreInfo() {
    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    return info;
}

} // namespace

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer() {
    Shutdown();
}

void VulkanRenderer::Init(VulkanContext* context, Window* window, ECSCoordinator* ecs, SceneManager* sceneManager) {
    if (m_Initialized) {
        return;
    }

    if (!context || !window) {
        throw std::invalid_argument("VulkanRenderer::Init requires valid context and window");
    }

    m_Context = context;
    m_Window = window;
    m_ECS = ecs;
    m_SceneManager = sceneManager;
    m_CameraSystem = (ecs != nullptr) ? ecs->GetCameraSystem() : nullptr;

    // Initialize async upload pipeline
    m_StagingPool.Init(m_Context);
    m_TransferQueue.Init(m_Context, MAX_FRAMES_IN_FLIGHT);

    m_Swapchain.Init(m_Context, m_Window);
    m_Descriptors.Init(m_Context, MAX_FRAMES_IN_FLIGHT);

    // Initialize TextureManager async pipeline (needs m_Descriptors to be initialized first)
    TextureManager::Instance().InitAsyncPipeline(m_Context, &m_TransferQueue, &m_StagingPool, &m_Descriptors);

    // Initialize MaterialManager GPU buffer (creates buffer with default material at index 0)
    MaterialManager::Instance().InitGPUBuffer(m_Context);

    // Bind MaterialManager's material buffer to descriptor sets
    VulkanMaterialBuffer* materialBuffer = MaterialManager::Instance().GetGPUBuffer();
    if (materialBuffer) {
        m_Descriptors.BindMaterialBuffer(materialBuffer->GetBuffer(), 0, materialBuffer->GetBufferSize());
        std::cout << "Bound MaterialManager's buffer to descriptor set" << std::endl;
    } else {
        std::cerr << "Failed to get MaterialManager's GPU buffer!" << std::endl;
    }

    // Create default texture for bindless array (MUST be done before any rendering)
    CreateDefaultTexture();

    // Create IBL placeholder textures (for scenes without IBL)
    CreateIBLPlaceholders();

    // Initialize Forward+ light culling system (MUST be done before InitSwapchainResources)
    m_LightCulling = std::make_unique<VulkanLightCulling>();
    LightCullingConfig lightCullingConfig{};
    lightCullingConfig.tileSize = 16;
    lightCullingConfig.maxLightsPerTile = 256;
    m_LightCulling->Init(m_Context, m_Swapchain.GetExtent().width, m_Swapchain.GetExtent().height,
                         MAX_FRAMES_IN_FLIGHT, lightCullingConfig);

    InitSwapchainResources();
    CreateFrameContexts();
    InitMeshResources();

    // Initialize shadow system
    m_ShadowSystem = std::make_unique<ShadowSystem>(m_ECS);
    std::cout << "Shadow system initialized" << std::endl;

    // Initialize shadow renderer
    m_ShadowRenderer = std::make_unique<VulkanShadowRenderer>();
    m_ShadowRenderer->Init(m_Context, m_ECS);
    m_ShadowRenderer->SetShadowSystem(m_ShadowSystem.get());

    // Initialize EVSM shadow filtering system
    m_EVSMShadow = std::make_unique<VulkanEVSMShadow>();
    m_EVSMShadow->Initialize(m_Context, 2048, 4);  // 2048x2048 resolution, 4 cascades

    // Bind EVSM moment texture to descriptor sets
    m_Descriptors.BindEVSMShadows(m_EVSMShadow->GetMomentsImageView(), m_EVSMShadow->GetSampler());
    std::cout << "Shadow rendering and EVSM filtering systems initialized" << std::endl;

    if (m_ECS) {
        m_RenderSystem = std::make_unique<RenderSystem>(m_ECS, m_Context);
        m_RenderSystem->UploadMeshes();
    } else {
        m_RenderSystem.reset();
    }

#ifdef _DEBUG
    // Initialize ImGui (debug builds only)
    m_ImGuiLayer.Init(m_Context, m_Window, m_RenderPass.Get(), m_SceneManager, m_ECS);
#endif

    m_Initialized = true;
}

void VulkanRenderer::Shutdown() {
    if (!m_Initialized || !m_Context) {
        return;
    }

    VkDevice device = m_Context->GetDevice();
    vkDeviceWaitIdle(device);

#ifdef _DEBUG
    // Shutdown ImGui (debug builds only)
    m_ImGuiLayer.Shutdown();
#endif

    if (m_RenderSystem) {
        m_RenderSystem->Shutdown();
        m_RenderSystem.reset();
    }

    // Shutdown async upload pipeline
    TextureManager::Instance().ShutdownAsyncPipeline();
    MaterialManager::Instance().ShutdownGPUBuffer();
    m_TransferQueue.Shutdown();
    m_StagingPool.Shutdown();

    // Shutdown Forward+ light culling
    if (m_LightCulling) {
        m_LightCulling->Destroy();
        m_LightCulling.reset();
    }

    // Shutdown shadow renderer
    if (m_ShadowRenderer) {
        m_ShadowRenderer->Shutdown();
        m_ShadowRenderer.reset();
    }

    // Shutdown shadow system
    if (m_ShadowSystem) {
        m_ShadowSystem.reset();
    }

    // Shutdown EVSM shadow filtering
    if (m_EVSMShadow) {
        m_EVSMShadow->Shutdown();
        m_EVSMShadow.reset();
    }

    DestroyDefaultTexture();
    DestroyIBLPlaceholders();
    DestroyMeshResources();
    DestroyFrameContexts();
    DestroySwapchainResources();
    m_Descriptors.Shutdown();
    m_Swapchain.Shutdown();

    m_Context = nullptr;
    m_Window = nullptr;
    m_ECS = nullptr;
    m_CameraSystem = nullptr;
    m_Initialized = false;
}

void VulkanRenderer::DrawFrame(ViewportManager* viewportManager) {
    if (m_RenderSystem) {
        m_RenderSystem->Update();
    }

    FrameContext* frame = nullptr;
    u32 imageIndex = 0;

    if (!BeginFrame(frame, imageIndex)) {
        return;
    }

    const u32 currentFrameIndex = m_CurrentFrame;
    VkCommandBuffer cmd = frame->commandBuffer;

    // ====================================================================================
    // SHADOW RENDERING
    // ====================================================================================

    // Render shadow maps for all shadow-casting lights
    if (m_ShadowRenderer && m_ShadowRenderer->HasShadowCastingLights()) {
        m_ShadowRenderer->RenderShadows(cmd, currentFrameIndex);

        // Update profiler results (must be done after command buffer submission - will be called in EndFrame)

        // Transition shadow depth image from DEPTH_ATTACHMENT to SHADER_READ_ONLY
        VkImage shadowDepthImage = m_ShadowRenderer->GetDirectionalShadowDepthImage();
        if (shadowDepthImage != VK_NULL_HANDLE) {
            VkImageMemoryBarrier shadowBarrier{};
            shadowBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            shadowBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            shadowBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            shadowBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            shadowBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            shadowBarrier.image = shadowDepthImage;
            shadowBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            shadowBarrier.subresourceRange.baseMipLevel = 0;
            shadowBarrier.subresourceRange.levelCount = 1;
            shadowBarrier.subresourceRange.baseArrayLayer = 0;
            shadowBarrier.subresourceRange.layerCount = m_ShadowRenderer->GetNumCascades();
            shadowBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            shadowBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &shadowBarrier
            );
        }

        // Generate EVSM moments from shadow depth map (if using EVSM filter mode)
        if (m_EVSMShadow && shadowDepthImage != VK_NULL_HANDLE) {
            VulkanEVSMShadow::Params evsmParams{};
            evsmParams.depthImage = shadowDepthImage;
            evsmParams.depthFormat = m_ShadowRenderer->GetShadowFormat();
            evsmParams.width = m_ShadowRenderer->GetDirectionalShadowResolution();
            evsmParams.height = m_ShadowRenderer->GetDirectionalShadowResolution();
            evsmParams.layerCount = m_ShadowRenderer->GetNumCascades();
            evsmParams.positiveExponent = 40.0f;
            evsmParams.negativeExponent = 40.0f;

            m_EVSMShadow->GenerateMoments(evsmParams);
        }

        // Bind shadow textures to descriptors
        m_Descriptors.BindShadowMap(
            m_ShadowRenderer->GetDirectionalShadowImageView(),
            m_ShadowRenderer->GetDirectionalShadowSampler()
        );

        // Bind raw depth shadow map for PCSS/Contact-Hardening
        m_Descriptors.BindRawDepthShadowMap(
            m_ShadowRenderer->GetDirectionalShadowImageView(),
            m_ShadowRenderer->GetDirectionalRawDepthSampler()
        );
    }

    // Bind IBL placeholder textures (fallback for scenes without IBL)
    // NOTE: This MUST be outside the shadow conditional block to ensure IBL descriptors
    // are always bound, even in scenes without lights.
    std::cout << "[DEBUG] Binding IBL placeholder textures to descriptors (frame " << currentFrameIndex << "):" << std::endl;

    std::cout << "  Binding irradiance map (binding 4): imageView=" << (void*)m_PlaceholderIrradianceMap->GetImageView()
              << ", sampler=" << (void*)m_PlaceholderIrradianceMap->GetSampler() << std::endl;
    m_Descriptors.BindIBLIrradiance(
        m_PlaceholderIrradianceMap->GetImageView(),
        m_PlaceholderIrradianceMap->GetSampler()
    );

    std::cout << "  Binding prefiltered map (binding 5): imageView=" << (void*)m_PlaceholderPrefilteredMap->GetImageView()
              << ", sampler=" << (void*)m_PlaceholderPrefilteredMap->GetSampler() << std::endl;
    m_Descriptors.BindIBLPrefiltered(
        m_PlaceholderPrefilteredMap->GetImageView(),
        m_PlaceholderPrefilteredMap->GetSampler()
    );

    std::cout << "  Binding BRDF LUT (binding 6): imageView=" << (void*)m_PlaceholderBRDFLUT->GetImageView()
              << ", sampler=" << (void*)m_PlaceholderBRDFLUT->GetSampler() << std::endl;
    m_Descriptors.BindIBLBRDF(
        m_PlaceholderBRDFLUT->GetImageView(),
        m_PlaceholderBRDFLUT->GetSampler()
    );

    // ====================================================================================
    // FORWARD+ PIPELINE INTEGRATION
    // ====================================================================================

    // Step 1: Depth Prepass - Render all opaque geometry to populate depth buffer
    if (m_DepthPrepassRenderPass != VK_NULL_HANDLE) {
        RenderDepthPrepass(cmd, currentFrameIndex);

        // Transition depth buffer: DEPTH_ATTACHMENT → SHADER_READ for compute shader
        TransitionDepthForRead(cmd);
    }

    // Step 2: Upload Light Data - Convert ECS lights to GPU format
    UploadLightDataForwardPlus();

    // Step 3: Light Culling Compute Shader - Dispatch per-tile light culling
    if (m_LightCulling && m_DepthBuffer.GetImageView() != VK_NULL_HANDLE) {
        u32 numLights = GetLightCount();

        if (numLights > 0 && m_CameraSystem) {
            Entity activeCamera = m_CameraSystem->GetActiveCamera();
            if (activeCamera.IsValid() && m_ECS && m_ECS->HasComponent<Camera>(activeCamera)) {
                const Camera& camera = m_ECS->GetComponent<Camera>(activeCamera);

                // Get projection matrix and invert it for compute shader
                Mat4 invProjection = glm::inverse(camera.projectionMatrix);

                // Get view matrix from camera transform
                Mat4 viewMatrix = Mat4(1.0f);
                if (m_ECS->HasComponent<Transform>(activeCamera)) {
                    const Transform& camTransform = m_ECS->GetComponent<Transform>(activeCamera);
                    viewMatrix = glm::inverse(camTransform.worldMatrix);
                }

                // Update depth buffer descriptor for this frame (done outside command buffer recording ideally, but safe here)
                m_LightCulling->UpdateDepthBuffer(currentFrameIndex, m_DepthBuffer.GetImageView());

                // Dispatch light culling compute shader
                m_LightCulling->CullLights(
                    cmd,
                    currentFrameIndex,
                    invProjection,
                    viewMatrix,
                    numLights
                );

                // Memory barrier: COMPUTE_SHADER_WRITE → FRAGMENT_SHADER_READ
                VkMemoryBarrier memoryBarrier{};
                memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(
                    cmd,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    1, &memoryBarrier,
                    0, nullptr,
                    0, nullptr
                );
            }
        }

        // Transition depth buffer back: SHADER_READ → DEPTH_ATTACHMENT for main pass
        TransitionDepthForWrite(cmd);
    }

    // ====================================================================================
    // END FORWARD+ PIPELINE
    // ====================================================================================

    // Log Forward+ performance metrics every 60 frames
    static u32 frameCounter = 0;
    if (m_LightCulling && ++frameCounter % 60 == 0) {
        f32 cullingTime = m_LightCulling->GetLastCullingTimeMs();
        if (cullingTime > 0.0f) {
            std::cout << "[Forward+] Light culling: " << cullingTime << " ms (target: < 0.5ms @ 1080p)" << std::endl;
        }
    }

    // Determine rendering path: viewport-based (Debug) or direct (Release)
    bool useDirectRendering = (viewportManager == nullptr);

#ifdef _DEBUG
    // Begin ImGui frame FIRST (debug builds only)
    if (!useDirectRendering) {
        m_ImGuiLayer.BeginFrame();

        // Setup dockspace and viewport windows to get their sizes
        if (viewportManager) {
            m_ImGuiLayer.SetupFrameLayout(viewportManager);
        }
    }
#endif

    // NOW render viewports to offscreen targets after ImGui has processed them
    // This ensures viewports are at the correct size before rendering
    bool viewportsRendered = false;
    if (!useDirectRendering && viewportManager) {
        VkDevice device = m_Context->GetDevice();
        VkCommandBuffer vpCmd = m_ViewportCommandBuffers[currentFrameIndex];

        // Wait for previous frame's viewport rendering to complete
        // IMPORTANT: Also mark viewports from PREVIOUS frame as rendered NOW,
        // after we know the GPU has finished with them
        vkWaitForFences(device, 1, &m_ViewportFences[currentFrameIndex], VK_TRUE, UINT64_MAX);

        // Mark viewports as rendered AFTER fence wait (previous frame's viewports are now safe to sample)
        auto viewports = viewportManager->GetAllViewports();
        for (Viewport* viewport : viewports) {
            if (viewport && viewport->IsReadyToRender()) {
                viewport->MarkAsRendered();
            }
        }

        vkResetFences(device, 1, &m_ViewportFences[currentFrameIndex]);

        // Begin recording viewport command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        if (vkBeginCommandBuffer(vpCmd, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin viewport command buffer");
        }

        // Render all viewports for THIS frame
        for (Viewport* viewport : viewports) {
            if (viewport && viewport->IsReadyToRender()) {
                RenderViewport(vpCmd, *viewport, viewport->GetCamera(), currentFrameIndex);
                viewportsRendered = true;
            }
        }

        if (vkEndCommandBuffer(vpCmd) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end viewport command buffer");
        }

        // Only submit if we actually rendered something
        if (viewportsRendered) {
            // Submit viewport command buffer
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &vpCmd;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &m_ViewportFinishedSemaphores[currentFrameIndex];

            if (vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, m_ViewportFences[currentFrameIndex]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to submit viewport command buffer");
            }

            // Don't wait here - let the semaphore handle synchronization
            // The main render pass will wait on viewport finished semaphore
        }
    }

    // Get clear color from active camera, or use default
    VkClearColorValue clearColor{};
    if (useDirectRendering && m_CameraSystem) {
        Entity activeCamera = m_CameraSystem->GetActiveCamera();
        if (activeCamera.IsValid() && m_ECS && m_ECS->HasComponent<Camera>(activeCamera)) {
            Vec4 clearColorVec = m_ECS->GetComponent<Camera>(activeCamera).clearColor;
            clearColor.float32[0] = clearColorVec.x;
            clearColor.float32[1] = clearColorVec.y;
            clearColor.float32[2] = clearColorVec.z;
            clearColor.float32[3] = clearColorVec.w;
        } else {
            // Default game clear color
            clearColor.float32[0] = 0.2f;
            clearColor.float32[1] = 0.2f;
            clearColor.float32[2] = 0.2f;
            clearColor.float32[3] = 1.0f;
        }
    } else {
        // Editor-style dark gray background for ImGui
        clearColor.float32[0] = 0.15f;
        clearColor.float32[1] = 0.15f;
        clearColor.float32[2] = 0.15f;
        clearColor.float32[3] = 1.0f;
    }

    BeginDefaultRenderPass(*frame, imageIndex, clearColor);

    if (useDirectRendering) {
        // Release build: render scene directly to swapchain
        RenderDirectToSwapchain(frame->commandBuffer, currentFrameIndex);
    }
#ifdef _DEBUG
    else {
        // Debug build: render ImGui UI - 3D content is rendered to viewport render targets
        m_ImGuiLayer.Render(frame->commandBuffer);
    }
#endif

    EndDefaultRenderPass(*frame);

    // Only pass viewport finished semaphore if viewports were actually rendered
    VkSemaphore* viewportSemaphore = viewportsRendered ? &m_ViewportFinishedSemaphores[currentFrameIndex] : nullptr;
    EndFrame(*frame, imageIndex, viewportSemaphore);
}

void VulkanRenderer::OnWindowResized() {
    m_FramebufferResized = true;
}

#ifdef _DEBUG
bool VulkanRenderer::ShouldChangeProject() const {
    return m_ImGuiLayer.ShouldChangeProject();
}
#endif

bool VulkanRenderer::BeginFrame(FrameContext*& outFrame, u32& outImageIndex) {
    if (!m_Initialized) {
        return false;
    }

    if (m_FramebufferResized) {
        RecreateSwapchain();
        if (!m_Initialized) {
            return false;
        }
    }

    VkDevice device = m_Context->GetDevice();
    FrameContext& frame = m_Frames[m_CurrentFrame];

    // Reset transfer queue for this frame
    m_TransferQueue.ResetForFrame(m_CurrentFrame);

    // Wait for this frame's fence and reset it immediately
    vkWaitForFences(device, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &frame.inFlightFence);

    // Acquire next swapchain image using round-robin semaphore selection
    // We cycle through semaphores to ensure each swapchain image gets its own set
    u32 imageIndex = 0;
    u32 semaphoreIndex = m_CurrentSemaphoreIndex;

    VkResult acquireResult = vkAcquireNextImageKHR(
        device,
        m_Swapchain.GetSwapchain(),
        UINT64_MAX,
        m_ImageAvailableSemaphores[semaphoreIndex],
        VK_NULL_HANDLE,
        &imageIndex);

    // Store the semaphores in frame context for use in submit/present
    frame.imageAvailableSemaphore = m_ImageAvailableSemaphores[semaphoreIndex];
    frame.renderFinishedSemaphore = m_RenderFinishedSemaphores[semaphoreIndex];

    // Advance semaphore index for next frame
    m_CurrentSemaphoreIndex = (m_CurrentSemaphoreIndex + 1) % m_ImageAvailableSemaphores.size();

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return false;
    }

    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    // Wait for the image to be available if it's still being used by another frame
    if (m_ImagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &m_ImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }

    m_ImagesInFlight[imageIndex] = frame.inFlightFence;
    m_CommandBuffers.Reset(m_CurrentFrame);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(frame.commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    outFrame = &frame;
    outImageIndex = imageIndex;
    return true;
}

void VulkanRenderer::BeginDefaultRenderPass(FrameContext& frame, u32 imageIndex, const VkClearColorValue& clearColor) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass.Get();
    renderPassInfo.framebuffer = m_Framebuffers.Get(imageIndex);
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_Swapchain.GetExtent();

    VkClearValue clearValues[2]{};
    clearValues[0].color = clearColor;
    clearValues[1].depthStencil.depth = 1.0f;
    clearValues[1].depthStencil.stencil = 0;

    renderPassInfo.clearValueCount = ARRAY_COUNT(clearValues);
    renderPassInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(frame.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderer::EndDefaultRenderPass(FrameContext& frame) {
    vkCmdEndRenderPass(frame.commandBuffer);
}

void VulkanRenderer::EndFrame(FrameContext& frame, u32 imageIndex, VkSemaphore* waitSemaphore) {
    if (vkEndCommandBuffer(frame.commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }

    // Setup wait semaphores and stages
    std::vector<VkSemaphore> waitSemaphores = { frame.imageAvailableSemaphore };
    std::vector<VkPipelineStageFlags> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT };

    // If viewport rendering occurred, also wait on viewport finished semaphore
    if (waitSemaphore != nullptr) {
        waitSemaphores.push_back(*waitSemaphore);
        waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = static_cast<u32>(waitSemaphores.size());
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame.renderFinishedSemaphore;

    VkResult result = vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, frame.inFlightFence);
    if (result != VK_SUCCESS) {
        std::cerr << "ERROR: vkQueueSubmit failed with error code: " << result << std::endl;
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    VkSwapchainKHR swapchains[] = { m_Swapchain.GetSwapchain() };

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame.renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(m_Context->GetPresentQueue(), &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || m_FramebufferResized) {
        RecreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }

    // Advance staging pool with current timeline value
    u64 currentTimelineValue = m_Context->GetCurrentTransferTimelineValue();
    m_StagingPool.AdvanceFrame(currentTimelineValue);

    // Update shadow profiler results (after command buffer has been submitted)
    if (m_ShadowRenderer && m_ShadowRenderer->GetProfiler()) {
        m_ShadowRenderer->GetProfiler()->UpdateResults(m_CurrentFrame);
    }

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::UpdateGlobalUniforms(u32 frameIndex) {
    const VkExtent2D extent = m_Swapchain.GetExtent();
    const f32 width = static_cast<f32>(extent.width);
    const f32 height = static_cast<f32>(extent.height == 0 ? 1 : extent.height);
    const f32 aspect = height != 0.0f ? width / height : 1.0f;

    UniformBufferObject ubo{};

    const Entity activeCamera = (m_CameraSystem != nullptr) ? m_CameraSystem->GetActiveCamera() : Entity::Invalid;
    if (m_CameraSystem != nullptr && activeCamera.IsValid()) {
        ubo.view = m_CameraSystem->GetViewMatrix();
        ubo.projection = m_CameraSystem->GetProjectionMatrix();
    } else {
        const Vec3 eye(3.0f, 3.0f, 3.0f);
        const Vec3 center(0.0f, 0.0f, 0.0f);
        const Vec3 up(0.0f, 1.0f, 0.0f);
        ubo.view = LookAt(eye, center, up);

        ubo.projection = Perspective(Radians(45.0f), aspect, 0.1f, 100.0f);
        ubo.projection[1][1] *= -1.0f;
    }

    m_Descriptors.UpdateUniformBuffer(frameIndex, &ubo, sizeof(ubo));
}

void VulkanRenderer::PushModelMatrix(VkCommandBuffer commandBuffer, const Mat4& modelMatrix, u32 materialIndex, u32 screenWidth, u32 screenHeight) {
    PushConstants pushConstants;
    pushConstants.model = modelMatrix;
    pushConstants.materialIndex = materialIndex;
    pushConstants.screenWidth = screenWidth;
    pushConstants.screenHeight = screenHeight;
    pushConstants.tileSize = 16;  // Forward+ tile size (must match compute shader)

    vkCmdPushConstants(commandBuffer, m_Pipeline.GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &pushConstants);
}

void VulkanRenderer::UpdateGlobalUniformsWithCamera(u32 frameIndex, Entity cameraEntity, u32 viewportWidth, u32 viewportHeight) {
    UniformBufferObject ubo{};

    if (m_ECS && cameraEntity.IsValid() && m_ECS->HasComponent<Camera>(cameraEntity) && m_ECS->HasComponent<Transform>(cameraEntity)) {
        const Camera& camera = m_ECS->GetComponent<Camera>(cameraEntity);
        const Transform& transform = m_ECS->GetComponent<Transform>(cameraEntity);

        // Compute view matrix from transform
        Vec3 position = transform.worldMatrix[3];
        Vec3 forward = -Vec3(transform.worldMatrix[2]);
        Vec3 up = Vec3(transform.worldMatrix[1]);
        ubo.view = LookAt(position, position + forward, up);

        // Compute projection matrix from camera
        f32 aspect = (viewportHeight > 0) ? static_cast<f32>(viewportWidth) / static_cast<f32>(viewportHeight) : 1.0f;
        if (camera.projection == CameraProjection::Perspective) {
            ubo.projection = Perspective(Radians(camera.fov), aspect, camera.nearPlane, camera.farPlane);
            ubo.projection[1][1] *= -1.0f;  // Vulkan Y-flip
        } else {
            f32 halfWidth = camera.orthoSize * aspect * 0.5f;
            f32 halfHeight = camera.orthoSize * 0.5f;
            ubo.projection = Ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, camera.nearPlane, camera.farPlane);
            ubo.projection[1][1] *= -1.0f;  // Vulkan Y-flip
        }
    } else {
        // Fallback camera
        const Vec3 eye(3.0f, 3.0f, 3.0f);
        const Vec3 center(0.0f, 0.0f, 0.0f);
        const Vec3 up(0.0f, 1.0f, 0.0f);
        ubo.view = LookAt(eye, center, up);

        f32 aspect = (viewportHeight > 0) ? static_cast<f32>(viewportWidth) / static_cast<f32>(viewportHeight) : 1.0f;
        ubo.projection = Perspective(Radians(45.0f), aspect, 0.1f, 100.0f);
        ubo.projection[1][1] *= -1.0f;
    }

    m_Descriptors.UpdateUniformBuffer(frameIndex, &ubo, sizeof(ubo));

    // Update shadow system with camera info
    if (m_ShadowSystem && cameraEntity.IsValid() && m_ECS && m_ECS->HasComponent<Camera>(cameraEntity)) {
        const Camera& camera = m_ECS->GetComponent<Camera>(cameraEntity);
        m_ShadowSystem->Update(cameraEntity, camera.nearPlane, camera.farPlane);

        // Update shadow uniform buffer
        const ShadowUniforms& shadowUniforms = m_ShadowSystem->GetShadowUniforms();
        m_Descriptors.BindShadowUBO(frameIndex, &shadowUniforms, sizeof(shadowUniforms));
    }
}

void VulkanRenderer::RenderScene(VkCommandBuffer commandBuffer, u32 frameIndex, u32 screenWidth, u32 screenHeight) {
    (void)frameIndex;  // Currently unused

    if (!m_RenderSystem) {
        return;
    }

    static u32 debugFrameCount = 0;
    bool shouldLog = (debugFrameCount < 2);  // Log first 2 frames only

    const auto& renderList = m_RenderSystem->GetRenderData();
    if (shouldLog) {
        std::cout << "[DEBUG] RenderScene: Rendering " << renderList.size() << " objects (frame " << debugFrameCount << ")" << std::endl;
    }

    for (const RenderData& renderData : renderList) {
        VulkanMesh* mesh = m_RenderSystem->GetVulkanMesh(renderData.meshHandle);
        if (!mesh || !mesh->IsValid()) {
            continue;
        }

        if (shouldLog) {
            std::cout << "  Drawing mesh with materialIndex=" << renderData.materialIndex << std::endl;
        }

        PushModelMatrix(commandBuffer, renderData.modelMatrix, renderData.materialIndex, screenWidth, screenHeight);
        mesh->Bind(commandBuffer);
        mesh->Draw(commandBuffer);
    }

    if (shouldLog) {
        debugFrameCount++;
    }
}

void VulkanRenderer::RenderViewport(VkCommandBuffer commandBuffer, Viewport& viewport, Entity cameraEntity, u32 frameIndex) {
    if (!viewport.IsReadyToRender() || !cameraEntity.IsValid()) {
        return;
    }

    // Ensure offscreen pipelines are initialized
    VulkanRenderTarget& renderTarget = viewport.GetRenderTarget();
    VkExtent2D extent = { viewport.GetWidth(), viewport.GetHeight() };
    EnsureOffscreenPipelinesInitialized(renderTarget.GetRenderPass(), extent);

    // Get camera clear color
    Vec4 clearColorVec = Vec4(0.2f, 0.2f, 0.2f, 1.0f);
    if (m_ECS && m_ECS->HasComponent<Camera>(cameraEntity)) {
        clearColorVec = m_ECS->GetComponent<Camera>(cameraEntity).clearColor;
    }

    VkClearColorValue clearColor{};
    clearColor.float32[0] = clearColorVec.x;
    clearColor.float32[1] = clearColorVec.y;
    clearColor.float32[2] = clearColorVec.z;
    clearColor.float32[3] = clearColorVec.w;

    // Use the provided command buffer for offscreen rendering
    VkCommandBuffer cmd = commandBuffer;

    // Begin offscreen render pass
    BeginOffscreenRenderPass(cmd, viewport, clearColor);

    // Bind offscreen pipeline (HDR-compatible)
    VkPipeline pipeline = m_Pipeline.GetOffscreenPipeline(PipelineVariant::Opaque);
    if (pipeline == VK_NULL_HANDLE) {
        EndOffscreenRenderPass(cmd);
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind all three descriptor sets
    VkDescriptorSet descriptorSets[3] = {
        m_Descriptors.GetTransientSet(frameIndex),  // Set 0: Per-frame camera UBO
        m_Descriptors.GetPersistentSet(),           // Set 1: Materials + bindless textures
        VK_NULL_HANDLE                               // Set 2: Forward+ tile data (bound separately)
    };

    // Bind sets 0 and 1 first
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline.GetLayout(),
        0, 2, descriptorSets, 0, nullptr);

    // Bind Forward+ tile light data (set 2)
    if (m_LightCulling) {
        m_LightCulling->BindTileLightData(cmd, m_Pipeline.GetLayout(), 2);
    }

    // Update camera UBO for this viewport
    UpdateGlobalUniformsWithCamera(frameIndex, cameraEntity, viewport.GetWidth(), viewport.GetHeight());

    // Set viewport and scissor
    VkViewport vkViewport{};
    vkViewport.x = 0.0f;
    vkViewport.y = 0.0f;
    vkViewport.width = static_cast<f32>(viewport.GetWidth());
    vkViewport.height = static_cast<f32>(viewport.GetHeight());
    vkViewport.minDepth = 0.0f;
    vkViewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vkViewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {viewport.GetWidth(), viewport.GetHeight()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Render scene
    RenderScene(cmd, frameIndex, viewport.GetWidth(), viewport.GetHeight());

    // End offscreen render pass
    EndOffscreenRenderPass(cmd);
}

void VulkanRenderer::BeginOffscreenRenderPass(VkCommandBuffer commandBuffer, Viewport& viewport, const VkClearColorValue& clearColor) {
    VulkanRenderTarget& renderTarget = viewport.GetRenderTarget();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = clearColor;
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderTarget.GetRenderPass();
    renderPassInfo.framebuffer = renderTarget.GetFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {viewport.GetWidth(), viewport.GetHeight()};
    renderPassInfo.clearValueCount = static_cast<u32>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderer::EndOffscreenRenderPass(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
}

void VulkanRenderer::RenderDirectToSwapchain(VkCommandBuffer commandBuffer, u32 frameIndex) {
    // Get active game camera from camera system
    Entity cameraEntity = Entity::Invalid;
    if (m_CameraSystem) {
        cameraEntity = m_CameraSystem->GetActiveCamera();
    }

    // If no active camera, we can't render
    if (!cameraEntity.IsValid()) {
        return;
    }

    // Bind pipeline
    VkPipeline pipeline = m_Pipeline.GetPipeline();
    if (pipeline == VK_NULL_HANDLE) {
        return;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind all three descriptor sets
    VkDescriptorSet descriptorSets[3] = {
        m_Descriptors.GetTransientSet(frameIndex),  // Set 0: Per-frame camera UBO
        m_Descriptors.GetPersistentSet(),           // Set 1: Materials + bindless textures
        VK_NULL_HANDLE                               // Set 2: Forward+ tile data (bound separately)
    };

    // Bind sets 0 and 1 first
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline.GetLayout(),
        0, 2, descriptorSets, 0, nullptr);

    // Bind Forward+ tile light data (set 2)
    if (m_LightCulling) {
        m_LightCulling->BindTileLightData(commandBuffer, m_Pipeline.GetLayout(), 2);
    }

    // Update camera UBO with window dimensions
    UpdateGlobalUniformsWithCamera(frameIndex, cameraEntity, m_Window->GetWidth(), m_Window->GetHeight());

    // Set viewport and scissor to match window size
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(m_Window->GetWidth());
    viewport.height = static_cast<f32>(m_Window->GetHeight());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {m_Window->GetWidth(), m_Window->GetHeight()};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Render scene
    RenderScene(commandBuffer, frameIndex, m_Window->GetWidth(), m_Window->GetHeight());
}

void VulkanRenderer::InitSwapchainResources() {
    m_DepthBuffer.Init(m_Context, &m_Swapchain);
    m_RenderPass.Init(m_Context, &m_Swapchain, m_DepthBuffer.GetFormat());

    // Pass all three descriptor set layouts to pipeline
    VkDescriptorSetLayout layouts[3] = {
        m_Descriptors.GetTransientLayout(),    // Set 0: Per-frame camera UBO
        m_Descriptors.GetPersistentLayout(),   // Set 1: Materials + bindless textures
        m_LightCulling->GetDescriptorLayout()  // Set 2: Forward+ tile light data
    };
    m_Pipeline.Init(m_Context, &m_RenderPass, &m_Swapchain, layouts, 3);

    m_Framebuffers.Init(m_Context, &m_Swapchain, &m_RenderPass, m_DepthBuffer.GetImageView());
    ResizeImagesInFlight();

    // Create Forward+ depth prepass resources
    CreateDepthPrepassResources();
}

void VulkanRenderer::DestroySwapchainResources() {
    // Destroy Forward+ depth prepass resources
    DestroyDepthPrepassResources();

    m_Framebuffers.Shutdown();
    m_Pipeline.Shutdown();  // This destroys both swapchain and offscreen pipelines
    m_RenderPass.Shutdown();
    m_DepthBuffer.Shutdown();
    m_ImagesInFlight.clear();
    m_OffscreenPipelinesInitialized = false;  // Will be re-initialized on next viewport render
}

void VulkanRenderer::CreateFrameContexts() {
    DestroyFrameContexts();

    m_CommandBuffers.Init(m_Context, MAX_FRAMES_IN_FLIGHT);

    VkDevice device = m_Context->GetDevice();
    m_Frames.resize(MAX_FRAMES_IN_FLIGHT);

    const auto& commandBuffers = m_CommandBuffers.GetCommandBuffers();
    if (commandBuffers.size() < m_Frames.size()) {
        throw std::runtime_error("VulkanRenderer::CreateFrameContexts insufficient command buffers allocated");
    }

    // Create per-frame fences only (semaphores are per-swapchain-image)
    for (size_t i = 0; i < m_Frames.size(); ++i) {
        FrameContext& frame = m_Frames[i];
        frame.commandBuffer = commandBuffers[i];

        VkFenceCreateInfo fenceInfo = CreateFenceInfo();
        if (vkCreateFence(device, &fenceInfo, nullptr, &frame.inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create in-flight fence");
        }

        // Semaphores are now per-swapchain-image, not per-frame
        frame.imageAvailableSemaphore = VK_NULL_HANDLE;
        frame.renderFinishedSemaphore = VK_NULL_HANDLE;
    }

    // Create per-swapchain-image semaphores (will be properly sized in InitSwapchainResources)
    u32 imageCount = m_Swapchain.GetImageCount();
    if (imageCount > 0) {
        m_ImageAvailableSemaphores.resize(imageCount);
        m_RenderFinishedSemaphores.resize(imageCount);

        VkSemaphoreCreateInfo semaphoreInfo = CreateSemaphoreInfo();
        for (u32 i = 0; i < imageCount; ++i) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create per-swapchain-image semaphores");
            }
        }
    }

    // Create viewport command pool and buffers
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &m_ViewportCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport command pool");
    }

    // Allocate viewport command buffers (one per frame in flight)
    m_ViewportCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_ViewportCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(device, &allocInfo, m_ViewportCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate viewport command buffers");
    }

    // Create viewport semaphores and fences
    m_ViewportFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_ViewportFences.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkSemaphoreCreateInfo semInfo = CreateSemaphoreInfo();
        if (vkCreateSemaphore(device, &semInfo, nullptr, &m_ViewportFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create viewport finished semaphore");
        }

        VkFenceCreateInfo fenceInfo = CreateFenceInfo();
        if (vkCreateFence(device, &fenceInfo, nullptr, &m_ViewportFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create viewport fence");
        }
    }
}

void VulkanRenderer::DestroyFrameContexts() {
    if (!m_Context) {
        m_CommandBuffers.Shutdown();
        m_Frames.clear();
        m_ImageAvailableSemaphores.clear();
        m_RenderFinishedSemaphores.clear();
        return;
    }

    VkDevice device = m_Context->GetDevice();

    // Destroy per-frame fences
    for (FrameContext& frame : m_Frames) {
        if (frame.inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, frame.inFlightFence, nullptr);
        }

        // Semaphores are no longer stored in FrameContext
        frame = FrameContext{};
    }

    // Destroy per-swapchain-image semaphores
    for (VkSemaphore semaphore : m_ImageAvailableSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
    }

    for (VkSemaphore semaphore : m_RenderFinishedSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
    }

    // Destroy viewport synchronization objects
    for (VkSemaphore semaphore : m_ViewportFinishedSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
    }

    for (VkFence fence : m_ViewportFences) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(device, fence, nullptr);
        }
    }

    // Destroy viewport command pool (this also frees command buffers)
    if (m_ViewportCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, m_ViewportCommandPool, nullptr);
        m_ViewportCommandPool = VK_NULL_HANDLE;
    }

    m_CommandBuffers.Shutdown();
    m_Frames.clear();
    m_ImageAvailableSemaphores.clear();
    m_RenderFinishedSemaphores.clear();
    m_ViewportCommandBuffers.clear();
    m_ViewportFinishedSemaphores.clear();
    m_ViewportFences.clear();
}

void VulkanRenderer::RecreateSwapchain() {
    if (!m_Context) {
        return;
    }

    u32 width = m_Window->GetWidth();
    u32 height = m_Window->GetHeight();

    while (width == 0 || height == 0) {
        m_Window->PollEvents();
        width = m_Window->GetWidth();
        height = m_Window->GetHeight();
    }

    VkDevice device = m_Context->GetDevice();
    vkDeviceWaitIdle(device);

    DestroySwapchainResources();
    m_Swapchain.Recreate(m_Window);
    InitSwapchainResources();

    // Resize Forward+ light culling buffers
    if (m_LightCulling) {
        m_LightCulling->Resize(m_Swapchain.GetExtent().width, m_Swapchain.GetExtent().height);
    }

    // Validate that pipeline was successfully recreated
    if (m_Pipeline.GetPipeline() == VK_NULL_HANDLE) {
        std::cerr << "ERROR: Pipeline is NULL after swapchain recreation!" << std::endl;
        throw std::runtime_error("Failed to recreate pipeline after swapchain recreation");
    }

    m_FramebufferResized = false;
}

void VulkanRenderer::ResizeImagesInFlight() {
    m_ImagesInFlight.assign(m_Swapchain.GetImageCount(), VK_NULL_HANDLE);
}

void VulkanRenderer::InitMeshResources() {
    MeshManager& meshManager = MeshManager::Instance();

    if (m_ActiveMesh.IsValid()) {
        DestroyMeshResources();
    }

    m_ActiveMesh = meshManager.CreateCube();
    MeshData* meshData = meshManager.Get(m_ActiveMesh);

    if (meshData == nullptr) {
        throw std::runtime_error("VulkanRenderer::InitMeshResources failed to create cube mesh");
    }

    if (!m_Context) {
        throw std::runtime_error("VulkanRenderer::InitMeshResources requires valid Vulkan context");
    }

    if (!meshData->gpuUploaded) {
        meshData->gpuMesh.Create(m_Context, meshData);
        meshData->gpuUploaded = true;
    }
}

void VulkanRenderer::DestroyMeshResources() {
    MeshManager& meshManager = MeshManager::Instance();

    if (!m_ActiveMesh.IsValid()) {
        return;
    }

    MeshData* meshData = meshManager.Get(m_ActiveMesh);
    if (meshData != nullptr && meshData->gpuUploaded) {
        meshData->gpuMesh.Destroy();
        meshData->gpuUploaded = false;
    }

    meshManager.Destroy(m_ActiveMesh);
    m_ActiveMesh = MeshHandle::Invalid;
}

void VulkanRenderer::EnsureOffscreenPipelinesInitialized(VkRenderPass offscreenRenderPass, VkExtent2D extent) {
    if (m_OffscreenPipelinesInitialized) {
        return;
    }

    if (offscreenRenderPass == VK_NULL_HANDLE) {
        std::cerr << "ERROR: Cannot initialize offscreen pipelines without valid render pass" << std::endl;
        return;
    }

    std::cout << "Initializing offscreen pipelines for HDR render targets..." << std::endl;
    m_Pipeline.InitOffscreenPipelines(offscreenRenderPass, extent);
    m_OffscreenPipelinesInitialized = true;
    std::cout << "Offscreen pipelines initialized successfully" << std::endl;
}

void VulkanRenderer::CreateDefaultTexture() {
    // Create a 1x1 white texture to use as default for all unbound texture indices
    std::cout << "Creating default texture for bindless array..." << std::endl;

    // Allocate texture data
    TextureData* textureData = new TextureData();
    textureData->width = 1;
    textureData->height = 1;
    textureData->channels = 4;  // RGBA
    textureData->mipLevels = 1;
    textureData->arrayLayers = 1;
    textureData->usage = TextureUsage::Generic;
    textureData->type = TextureType::Texture2D;

    // Allocate pixel data (1x1 RGBA white)
    textureData->pixels = new u8[4];
    textureData->pixels[0] = 255;  // R
    textureData->pixels[1] = 255;  // G
    textureData->pixels[2] = 255;  // B
    textureData->pixels[3] = 255;  // A

    // Create VulkanTexture
    m_DefaultTexture = std::make_unique<VulkanTexture>();
    m_DefaultTexture->Create(m_Context, textureData);

    // Register with bindless descriptor array (should get index 0)
    u32 descriptorIndex = m_Descriptors.RegisterTexture(
        m_DefaultTexture->GetImageView(),
        m_DefaultTexture->GetSampler()
    );

    m_DefaultTexture->SetDescriptorIndex(descriptorIndex);

    std::cout << "Default texture created and registered at bindless index " << descriptorIndex << std::endl;

    // Clean up TextureData (pixels are copied to GPU)
    delete textureData;
}

void VulkanRenderer::DestroyDefaultTexture() {
    if (m_DefaultTexture) {
        u32 descriptorIndex = m_DefaultTexture->GetDescriptorIndex();
        if (descriptorIndex != 0xFFFFFFFF) {
            m_Descriptors.UnregisterTexture(descriptorIndex);
        }
        m_DefaultTexture->Destroy();
        m_DefaultTexture.reset();
    }
}

void VulkanRenderer::CreateIBLPlaceholders() {
    std::cout << "Creating IBL placeholder textures..." << std::endl;

    // Create 1x1 dark gray cubemap for irradiance map (subtle ambient lighting)
    {
        TextureData* textureData = new TextureData();
        textureData->width = 1;
        textureData->height = 1;
        textureData->channels = 4;
        textureData->mipLevels = 1;
        textureData->arrayLayers = 6;  // Cubemap has 6 faces
        textureData->usage = TextureUsage::Generic;
        textureData->type = TextureType::Cubemap;

        // Allocate separate pixel data for each face (required for proper cleanup)
        for (u32 face = 0; face < 6; ++face) {
            u8* facePixels = new u8[4];  // 1x1 RGBA
            facePixels[0] = 30;  // R - dark gray for subtle ambient (0.12 intensity)
            facePixels[1] = 30;  // G
            facePixels[2] = 30;  // B
            facePixels[3] = 255;  // A
            textureData->layerPixels.push_back(facePixels);
        }

        // Pack layers into contiguous staging buffer (required by VulkanTexture)
        if (!textureData->PackLayersIntoStagingBuffer()) {
            std::cerr << "Failed to pack irradiance map layers" << std::endl;
            delete textureData;
            return;
        }

        // Create VulkanTexture
        m_PlaceholderIrradianceMap = std::make_unique<VulkanTexture>();
        m_PlaceholderIrradianceMap->Create(m_Context, textureData);

        std::cout << "  Created placeholder irradiance map (1x1 dark gray cubemap for subtle ambient)" << std::endl;
        std::cout << "    ImageView: " << (void*)m_PlaceholderIrradianceMap->GetImageView() << std::endl;
        std::cout << "    Sampler: " << (void*)m_PlaceholderIrradianceMap->GetSampler() << std::endl;
        delete textureData;
    }

    // Create 1x1 dark gray cubemap for prefiltered map (subtle reflections)
    {
        TextureData* textureData = new TextureData();
        textureData->width = 1;
        textureData->height = 1;
        textureData->channels = 4;
        textureData->mipLevels = 1;
        textureData->arrayLayers = 6;  // Cubemap has 6 faces
        textureData->usage = TextureUsage::Generic;
        textureData->type = TextureType::Cubemap;

        // Allocate separate pixel data for each face (required for proper cleanup)
        for (u32 face = 0; face < 6; ++face) {
            u8* facePixels = new u8[4];  // 1x1 RGBA
            facePixels[0] = 30;  // R - dark gray for subtle reflections (0.12 intensity)
            facePixels[1] = 30;  // G
            facePixels[2] = 30;  // B
            facePixels[3] = 255;  // A
            textureData->layerPixels.push_back(facePixels);
        }

        // Pack layers into contiguous staging buffer (required by VulkanTexture)
        if (!textureData->PackLayersIntoStagingBuffer()) {
            std::cerr << "Failed to pack prefiltered map layers" << std::endl;
            delete textureData;
            return;
        }

        m_PlaceholderPrefilteredMap = std::make_unique<VulkanTexture>();
        m_PlaceholderPrefilteredMap->Create(m_Context, textureData);

        std::cout << "  Created placeholder prefiltered map (1x1 dark gray cubemap for subtle reflections)" << std::endl;
        std::cout << "    ImageView: " << (void*)m_PlaceholderPrefilteredMap->GetImageView() << std::endl;
        std::cout << "    Sampler: " << (void*)m_PlaceholderPrefilteredMap->GetSampler() << std::endl;
        delete textureData;
    }

    // Create 1x1 neutral 2D texture for BRDF LUT (neutral Fresnel response)
    {
        TextureData* textureData = new TextureData();
        textureData->width = 1;
        textureData->height = 1;
        textureData->channels = 4;
        textureData->mipLevels = 1;
        textureData->arrayLayers = 1;
        textureData->usage = TextureUsage::Generic;
        textureData->type = TextureType::Texture2D;

        // Allocate pixel data (1x1 RGBA)
        textureData->pixels = new u8[4];
        textureData->pixels[0] = 128;  // R - neutral Fresnel scale (0.5)
        textureData->pixels[1] = 0;    // G - no bias
        textureData->pixels[2] = 0;    // B - unused
        textureData->pixels[3] = 255;  // A

        m_PlaceholderBRDFLUT = std::make_unique<VulkanTexture>();
        m_PlaceholderBRDFLUT->Create(m_Context, textureData);

        std::cout << "  Created placeholder BRDF LUT (1x1 neutral texture with Fresnel 0.5)" << std::endl;
        std::cout << "    ImageView: " << (void*)m_PlaceholderBRDFLUT->GetImageView() << std::endl;
        std::cout << "    Sampler: " << (void*)m_PlaceholderBRDFLUT->GetSampler() << std::endl;
        delete textureData;
    }

    std::cout << "IBL placeholder textures created successfully" << std::endl;
}

void VulkanRenderer::DestroyIBLPlaceholders() {
    if (m_PlaceholderIrradianceMap) {
        m_PlaceholderIrradianceMap->Destroy();
        m_PlaceholderIrradianceMap.reset();
    }
    if (m_PlaceholderPrefilteredMap) {
        m_PlaceholderPrefilteredMap->Destroy();
        m_PlaceholderPrefilteredMap.reset();
    }
    if (m_PlaceholderBRDFLUT) {
        m_PlaceholderBRDFLUT->Destroy();
        m_PlaceholderBRDFLUT.reset();
    }
    std::cout << "IBL placeholder textures destroyed" << std::endl;
}

void VulkanRenderer::UploadLightDataForwardPlus() {
    if (!m_ECS || !m_LightCulling) {
        return;
    }

    std::vector<GPULightForwardPlus> gpuLights;

    m_ECS->ForEach<Transform, Light>([&](Entity entity, Transform& transform, Light& light) {
        (void)entity;  // Unused
        GPULightForwardPlus gpuLight{};

        // Extract world position from world matrix
        Vec3 worldPosition = Vec3(transform.worldMatrix[3]);
        gpuLight.positionAndRange = Vec4(worldPosition, light.range);

        // Direction and type (0=Directional, 1=Point, 2=Spot)
        u32 lightType = 0;
        if (light.type == LightType::Point) lightType = 1;
        else if (light.type == LightType::Spot) lightType = 2;

        // Calculate forward direction from world matrix (negative Z axis)
        Vec3 forward = -Vec3(transform.worldMatrix[2]);
        forward = Normalize(forward);
        gpuLight.directionAndType = Vec4(forward, static_cast<f32>(lightType));

        // Color and intensity
        gpuLight.colorAndIntensity = Vec4(light.color, light.intensity);

        // Spot angles (convert degrees to cosine for shader)
        if (light.type == LightType::Spot) {
            gpuLight.spotAngles = Vec4(
                std::cos(Radians(light.innerConeAngle)),
                std::cos(Radians(light.outerConeAngle)),
                0.0f, 0.0f
            );
        } else {
            gpuLight.spotAngles = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        // Shadow data (placeholder for now - will integrate with shadow system later)
        gpuLight.castsShadows = light.castsShadows ? 1u : 0u;
        gpuLight.shadowIndex = 0;
        gpuLight.shadowBias = 0.005f;
        gpuLight.shadowPCFRadius = 2.0f;
        gpuLight.shadowAtlasUV = Vec4(0.0f, 0.0f, 1.0f, 1.0f);

        gpuLights.push_back(gpuLight);
    });

    // Upload to GPU
    if (!gpuLights.empty()) {
        m_LightCulling->UploadLightData(gpuLights);
    }
}

u32 VulkanRenderer::GetLightCount() const {
    if (!m_ECS) {
        return 0;
    }

    u32 count = 0;
    m_ECS->ForEach<Light>([&](Entity entity, Light& light) {
        (void)entity;  // Unused
        (void)light;   // Unused
        count++;
    });

    return count;
}

void VulkanRenderer::CreateDepthPrepassResources() {
    VkDevice device = m_Context->GetDevice();

    // 1. Create depth-only renderpass
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_DepthBuffer.GetFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // Store for later use
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;  // No color attachments
    subpass.pDepthStencilAttachment = &depthRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_DepthPrepassRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth prepass render pass!");
    }

    // 2. Create framebuffer
    VkImageView attachments[] = { m_DepthBuffer.GetImageView() };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_DepthPrepassRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = m_Swapchain.GetExtent().width;
    framebufferInfo.height = m_Swapchain.GetExtent().height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_DepthPrepassFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth prepass framebuffer!");
    }

    // 3. Create pipeline layout (uses same descriptor set layout as main pipeline)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    VkDescriptorSetLayout descriptorLayout = m_Descriptors.GetLayout();
    pipelineLayoutInfo.pSetLayouts = &descriptorLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_DepthPrepassPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth prepass pipeline layout!");
    }

    // 4. Load shaders
    std::filesystem::path vertPath = std::filesystem::path(ENGINE_SOURCE_DIR) / "assets" / "shaders" / "depth_prepass.vert.spv";
    std::filesystem::path fragPath = std::filesystem::path(ENGINE_SOURCE_DIR) / "assets" / "shaders" / "depth_prepass.frag.spv";
    std::ifstream vertFile(vertPath, std::ios::ate | std::ios::binary);
    std::ifstream fragFile(fragPath, std::ios::ate | std::ios::binary);

    if (!vertFile.is_open() || !fragFile.is_open()) {
        throw std::runtime_error("Failed to open depth prepass shader files!");
    }

    size_t vertFileSize = static_cast<size_t>(vertFile.tellg());
    size_t fragFileSize = static_cast<size_t>(fragFile.tellg());
    std::vector<char> vertShaderCode(vertFileSize);
    std::vector<char> fragShaderCode(fragFileSize);

    vertFile.seekg(0);
    fragFile.seekg(0);
    vertFile.read(vertShaderCode.data(), vertFileSize);
    fragFile.read(fragShaderCode.data(), fragFileSize);
    vertFile.close();
    fragFile.close();

    // Create shader modules
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = vertShaderCode.size();
    createInfo.pCode = reinterpret_cast<const u32*>(vertShaderCode.data());

    VkShaderModule vertShaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &vertShaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vertex shader module!");
    }

    createInfo.codeSize = fragShaderCode.size();
    createInfo.pCode = reinterpret_cast<const u32*>(fragShaderCode.data());

    VkShaderModule fragShaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &fragShaderModule) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        throw std::runtime_error("Failed to create fragment shader module!");
    }

    // 5. Create pipeline
    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShaderModule;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShaderModule;
    shaderStages[1].pName = "main";

    // Vertex input (only position needed)
    auto bindingDescription = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;  // Only position
    vertexInputInfo.pVertexAttributeDescriptions = &attributeDescriptions[0];

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(m_Swapchain.GetExtent().width);
    viewport.height = static_cast<f32>(m_Swapchain.GetExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_Swapchain.GetExtent();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 0;  // No color attachments
    colorBlending.pAttachments = nullptr;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.layout = m_DepthPrepassPipelineLayout;
    pipelineInfo.renderPass = m_DepthPrepassRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_DepthPrepassPipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        throw std::runtime_error("Failed to create depth prepass pipeline!");
    }

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    std::cout << "Depth prepass resources created successfully" << std::endl;
}

void VulkanRenderer::DestroyDepthPrepassResources() {
    if (!m_Context) {
        return;
    }

    VkDevice device = m_Context->GetDevice();

    if (m_DepthPrepassPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_DepthPrepassPipeline, nullptr);
        m_DepthPrepassPipeline = VK_NULL_HANDLE;
    }

    if (m_DepthPrepassPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_DepthPrepassPipelineLayout, nullptr);
        m_DepthPrepassPipelineLayout = VK_NULL_HANDLE;
    }

    if (m_DepthPrepassFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_DepthPrepassFramebuffer, nullptr);
        m_DepthPrepassFramebuffer = VK_NULL_HANDLE;
    }

    if (m_DepthPrepassRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_DepthPrepassRenderPass, nullptr);
        m_DepthPrepassRenderPass = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::RenderDepthPrepass(VkCommandBuffer commandBuffer, u32 frameIndex) {
    if (!m_RenderSystem) {
        return;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_DepthPrepassRenderPass;
    renderPassInfo.framebuffer = m_DepthPrepassFramebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_Swapchain.GetExtent();

    VkClearValue clearValue{};
    clearValue.depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DepthPrepassPipeline);

    // Bind descriptor set (for MVP matrices)
    VkDescriptorSet descriptorSet = m_Descriptors.GetDescriptorSet(frameIndex);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_DepthPrepassPipelineLayout, 0, 1,
                            &descriptorSet,
                            0, nullptr);

    // Get screen dimensions for push constants
    VkExtent2D extent = m_Swapchain.GetExtent();
    u32 screenWidth = extent.width;
    u32 screenHeight = extent.height;

    // Render all meshes (depth only)
    const auto& renderList = m_RenderSystem->GetRenderData();
    for (const RenderData& renderData : renderList) {
        VulkanMesh* mesh = m_RenderSystem->GetVulkanMesh(renderData.meshHandle);
        if (!mesh || !mesh->IsValid()) {
            continue;
        }

        PushModelMatrix(commandBuffer, renderData.modelMatrix, renderData.materialIndex, screenWidth, screenHeight);
        mesh->Bind(commandBuffer);
        mesh->Draw(commandBuffer);
    }

    vkCmdEndRenderPass(commandBuffer);
}

void VulkanRenderer::TransitionDepthForRead(VkCommandBuffer commandBuffer) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_DepthBuffer.GetImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);
}

void VulkanRenderer::TransitionDepthForWrite(VkCommandBuffer commandBuffer) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_DepthBuffer.GetImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);
}
