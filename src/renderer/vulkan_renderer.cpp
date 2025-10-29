#include "renderer/vulkan_renderer.h"

#include "core/math.h"
#include "core/time.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/systems/camera_system.h"
#include "ecs/systems/render_system.h"
#include "ecs/components/camera.h"
#include "ecs/components/transform.h"
#include "platform/window.h"
#include "renderer/vulkan_context.h"
#include "renderer/uniform_buffers.h"
#include "renderer/vertex.h"
#include "renderer/push_constants.h"
#include "renderer/material_buffer.h"
#include "renderer/viewport.h"
#include "renderer/viewport_manager.h"
#include "resources/mesh_manager.h"
#include "resources/texture_manager.h"
#include "resources/material_manager.h"
#include "core/texture_data.h"

#include <stdexcept>
#include <iostream>

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

    InitSwapchainResources();
    CreateFrameContexts();
    InitMeshResources();

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

    DestroyDefaultTexture();
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

void VulkanRenderer::PushModelMatrix(VkCommandBuffer commandBuffer, const Mat4& modelMatrix, u32 materialIndex) {
    PushConstants pushConstants;
    pushConstants.model = modelMatrix;
    pushConstants.materialIndex = materialIndex;
    pushConstants.padding[0] = 0;

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
}

void VulkanRenderer::RenderScene(VkCommandBuffer commandBuffer, u32 frameIndex) {
    (void)frameIndex;  // Currently unused

    if (!m_RenderSystem) {
        return;
    }

    const auto& renderList = m_RenderSystem->GetRenderData();
    for (const RenderData& renderData : renderList) {
        VulkanMesh* mesh = m_RenderSystem->GetVulkanMesh(renderData.meshHandle);
        if (!mesh || !mesh->IsValid()) {
            continue;
        }

        PushModelMatrix(commandBuffer, renderData.modelMatrix, renderData.materialIndex);
        mesh->Bind(commandBuffer);
        mesh->Draw(commandBuffer);
    }
}

void VulkanRenderer::RenderViewport(VkCommandBuffer commandBuffer, Viewport& viewport, Entity cameraEntity, u32 frameIndex) {
    if (!viewport.IsReadyToRender() || !cameraEntity.IsValid()) {
        return;
    }

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

    // Bind pipeline
    VkPipeline pipeline = m_Pipeline.GetPipeline();
    if (pipeline == VK_NULL_HANDLE) {
        EndOffscreenRenderPass(cmd);
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind descriptor sets
    VkDescriptorSet descriptorSets[2] = {
        m_Descriptors.GetTransientSet(frameIndex),
        m_Descriptors.GetPersistentSet()
    };
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline.GetLayout(),
        0, 2, descriptorSets, 0, nullptr);

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
    RenderScene(cmd, frameIndex);

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

    // Bind descriptor sets
    VkDescriptorSet descriptorSets[2] = {
        m_Descriptors.GetTransientSet(frameIndex),
        m_Descriptors.GetPersistentSet()
    };
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline.GetLayout(),
        0, 2, descriptorSets, 0, nullptr);

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
    RenderScene(commandBuffer, frameIndex);
}

void VulkanRenderer::InitSwapchainResources() {
    m_DepthBuffer.Init(m_Context, &m_Swapchain);
    m_RenderPass.Init(m_Context, &m_Swapchain, m_DepthBuffer.GetFormat());

    // Pass both descriptor set layouts to pipeline
    VkDescriptorSetLayout layouts[2] = {
        m_Descriptors.GetTransientLayout(),   // Set 0: Per-frame camera UBO
        m_Descriptors.GetPersistentLayout()   // Set 1: Materials + bindless textures
    };
    m_Pipeline.Init(m_Context, &m_RenderPass, &m_Swapchain, layouts, 2);

    m_Framebuffers.Init(m_Context, &m_Swapchain, &m_RenderPass, m_DepthBuffer.GetImageView());
    ResizeImagesInFlight();
}

void VulkanRenderer::DestroySwapchainResources() {
    m_Framebuffers.Shutdown();
    m_Pipeline.Shutdown();
    m_RenderPass.Shutdown();
    m_DepthBuffer.Shutdown();
    m_ImagesInFlight.clear();
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
