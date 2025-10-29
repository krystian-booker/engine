#include "renderer/vulkan_renderer.h"

#include "core/math.h"
#include "core/time.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/systems/camera_system.h"
#include "ecs/systems/render_system.h"
#include "platform/window.h"
#include "renderer/vulkan_context.h"
#include "renderer/uniform_buffers.h"
#include "renderer/vertex.h"
#include "renderer/push_constants.h"
#include "renderer/material_buffer.h"
#include "resources/mesh_manager.h"
#include "resources/texture_manager.h"

#include <stdexcept>

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

void VulkanRenderer::Init(VulkanContext* context, Window* window, ECSCoordinator* ecs) {
    if (m_Initialized) {
        return;
    }

    if (!context || !window) {
        throw std::invalid_argument("VulkanRenderer::Init requires valid context and window");
    }

    m_Context = context;
    m_Window = window;
    m_ECS = ecs;
    m_CameraSystem = (ecs != nullptr) ? ecs->GetCameraSystem() : nullptr;

    // Initialize async upload pipeline
    m_StagingPool.Init(m_Context);
    m_TransferQueue.Init(m_Context, MAX_FRAMES_IN_FLIGHT);

    // Initialize TextureManager async pipeline
    TextureManager::Instance().InitAsyncPipeline(m_Context, &m_TransferQueue, &m_StagingPool);

    m_Swapchain.Init(m_Context, m_Window);
    m_Descriptors.Init(m_Context, MAX_FRAMES_IN_FLIGHT);

    // Initialize material buffer with one default material
    m_MaterialBuffer.Init(m_Context, 256);  // Start with capacity for 256 materials

    // Create a default material
    GPUMaterial defaultMaterial{};
    defaultMaterial.albedoIndex = 0;
    defaultMaterial.normalIndex = 0;
    defaultMaterial.metalRoughIndex = 0;
    defaultMaterial.aoIndex = 0;
    defaultMaterial.emissiveIndex = 0;
    defaultMaterial.flags = 0;
    defaultMaterial.albedoTint = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    defaultMaterial.emissiveFactor = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    defaultMaterial.metallicFactor = 0.0f;
    defaultMaterial.roughnessFactor = 0.5f;
    defaultMaterial.normalScale = 1.0f;
    defaultMaterial.aoStrength = 1.0f;
    m_MaterialBuffer.UploadMaterial(defaultMaterial);

    // Bind material buffer to descriptor sets
    m_Descriptors.BindMaterialBuffer(m_MaterialBuffer.GetBuffer(), 0, m_MaterialBuffer.GetBufferSize());

    InitSwapchainResources();
    CreateFrameContexts();
    InitMeshResources();

    if (m_ECS) {
        m_RenderSystem = std::make_unique<RenderSystem>(m_ECS, m_Context);
        m_RenderSystem->UploadMeshes();
    } else {
        m_RenderSystem.reset();
    }

    m_Initialized = true;
}

void VulkanRenderer::Shutdown() {
    if (!m_Initialized || !m_Context) {
        return;
    }

    VkDevice device = m_Context->GetDevice();
    vkDeviceWaitIdle(device);

    if (m_RenderSystem) {
        m_RenderSystem->Shutdown();
        m_RenderSystem.reset();
    }

    // Shutdown async upload pipeline
    TextureManager::Instance().ShutdownAsyncPipeline();
    m_TransferQueue.Shutdown();
    m_StagingPool.Shutdown();

    DestroyMeshResources();
    DestroyFrameContexts();
    DestroySwapchainResources();
    m_Descriptors.Shutdown();
    m_MaterialBuffer.Shutdown();
    m_Swapchain.Shutdown();

    m_Context = nullptr;
    m_Window = nullptr;
    m_ECS = nullptr;
    m_CameraSystem = nullptr;
    m_Initialized = false;
}

void VulkanRenderer::DrawFrame() {
    if (m_RenderSystem) {
        m_RenderSystem->Update();
    }

    FrameContext* frame = nullptr;
    u32 imageIndex = 0;

    if (!BeginFrame(frame, imageIndex)) {
        return;
    }

    const u32 currentFrameIndex = m_CurrentFrame;
    Vec4 clearColorVec = Vec4(0.1f, 0.1f, 0.1f, 1.0f);
    if (m_CameraSystem != nullptr) {
        clearColorVec = m_CameraSystem->GetClearColor();
    }

    VkClearColorValue clearColor{};
    clearColor.float32[0] = clearColorVec.x;
    clearColor.float32[1] = clearColorVec.y;
    clearColor.float32[2] = clearColorVec.z;
    clearColor.float32[3] = clearColorVec.w;

    BeginDefaultRenderPass(*frame, imageIndex, clearColor);

    vkCmdBindPipeline(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline.GetPipeline());
    VkDescriptorSet descriptorSet = m_Descriptors.GetDescriptorSet(currentFrameIndex);
    vkCmdBindDescriptorSets(
        frame->commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline.GetLayout(),
        0,
        1,
        &descriptorSet,
        0,
        nullptr);

    UpdateGlobalUniforms(currentFrameIndex);

    bool rendered = false;

    if (m_RenderSystem) {
        const auto& renderList = m_RenderSystem->GetRenderData();
        for (const RenderData& renderData : renderList) {
            VulkanMesh* mesh = m_RenderSystem->GetVulkanMesh(renderData.meshHandle);
            if (!mesh || !mesh->IsValid()) {
                continue;
            }

            PushModelMatrix(frame->commandBuffer, renderData.modelMatrix, renderData.materialIndex);
            mesh->Bind(frame->commandBuffer);
            mesh->Draw(frame->commandBuffer);
            rendered = true;
        }
    }

    if (!rendered) {
        MeshManager& meshManager = MeshManager::Instance();
        MeshData* meshData = meshManager.Get(m_ActiveMesh);
        if (meshData == nullptr || !meshData->gpuUploaded) {
            throw std::runtime_error("VulkanRenderer::DrawFrame missing uploaded mesh data");
        }

        const f32 rotationSpeed = Radians(45.0f);
        const f32 fullRotation = Radians(360.0f);
        m_Rotation += rotationSpeed * Time::DeltaTime();
        while (m_Rotation > fullRotation) {
            m_Rotation -= fullRotation;
        }

        const Mat4 fallbackModel = Rotate(Mat4(1.0f), m_Rotation, Vec3(0.0f, 1.0f, 0.0f));
        PushModelMatrix(frame->commandBuffer, fallbackModel, 0); // Use default material index

        meshData->gpuMesh.Bind(frame->commandBuffer);
        meshData->gpuMesh.Draw(frame->commandBuffer);
    }

    EndDefaultRenderPass(*frame);
    EndFrame(*frame, imageIndex);
}

void VulkanRenderer::OnWindowResized() {
    m_FramebufferResized = true;
}

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

    vkWaitForFences(device, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);

    u32 imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        device,
        m_Swapchain.GetSwapchain(),
        UINT64_MAX,
        frame.imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return false;
    }

    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    if (m_ImagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &m_ImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }

    m_ImagesInFlight[imageIndex] = frame.inFlightFence;

    vkResetFences(device, 1, &frame.inFlightFence);
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

void VulkanRenderer::EndFrame(FrameContext& frame, u32 imageIndex) {
    if (vkEndCommandBuffer(frame.commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame.renderFinishedSemaphore;

    if (vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, frame.inFlightFence) != VK_SUCCESS) {
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

void VulkanRenderer::InitSwapchainResources() {
    m_DepthBuffer.Init(m_Context, &m_Swapchain);
    m_RenderPass.Init(m_Context, &m_Swapchain, m_DepthBuffer.GetFormat());
    m_Pipeline.Init(m_Context, &m_RenderPass, &m_Swapchain, m_Descriptors.GetLayout());
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

    for (size_t i = 0; i < m_Frames.size(); ++i) {
        FrameContext& frame = m_Frames[i];
        frame.commandBuffer = commandBuffers[i];

        VkFenceCreateInfo fenceInfo = CreateFenceInfo();
        if (vkCreateFence(device, &fenceInfo, nullptr, &frame.inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create in-flight fence");
        }

        VkSemaphoreCreateInfo semaphoreInfo = CreateSemaphoreInfo();
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.renderFinishedSemaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization semaphores");
        }
    }
}

void VulkanRenderer::DestroyFrameContexts() {
    if (!m_Context) {
        m_CommandBuffers.Shutdown();
        m_Frames.clear();
        return;
    }

    VkDevice device = m_Context->GetDevice();

    for (FrameContext& frame : m_Frames) {
        if (frame.inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, frame.inFlightFence, nullptr);
        }

        if (frame.imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frame.imageAvailableSemaphore, nullptr);
        }

        if (frame.renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frame.renderFinishedSemaphore, nullptr);
        }

        frame = FrameContext{};
    }

    m_CommandBuffers.Shutdown();
    m_Frames.clear();
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
