#include "renderer/vulkan_buffer.h"
#include "renderer/vulkan_context.h"
#include "renderer/vertex.h"
#include "platform/window.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void ImmediateCopyBuffer(VulkanContext* context, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkDevice device = context->GetDevice();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = context->GetGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("ImmediateCopyBuffer failed to create command pool");
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        vkDestroyCommandPool(device, pool, nullptr);
        throw std::runtime_error("ImmediateCopyBuffer failed to allocate command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
        vkDestroyCommandPool(device, pool, nullptr);
        throw std::runtime_error("ImmediateCopyBuffer failed to begin command buffer");
    }

    VkBufferCopy region{};
    region.size = size;

    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &region);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
        vkDestroyCommandPool(device, pool, nullptr);
        throw std::runtime_error("ImmediateCopyBuffer failed to end command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue queue = context->GetGraphicsQueue();
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
        vkDestroyCommandPool(device, pool, nullptr);
        throw std::runtime_error("ImmediateCopyBuffer failed to submit work");
    }

    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
    vkDestroyCommandPool(device, pool, nullptr);
}

} // namespace

static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) \
    static void name(); \
    static void name##_runner() { \
        testsRun++; \
        std::cout << "Running " << #name << "... "; \
        try { \
            name(); \
            testsPassed++; \
            std::cout << "PASSED" << std::endl; \
        } catch (const std::exception& ex) { \
            testsFailed++; \
            std::cout << "FAILED (" << ex.what() << ")" << std::endl; \
        } catch (...) { \
            testsFailed++; \
            std::cout << "FAILED (unknown exception)" << std::endl; \
        } \
    } \
    static void name()

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            throw std::runtime_error("Assertion failed: " #expr); \
        } \
    } while (0)

TEST(VulkanBuffer_HostVisibleUploadCopiesData) {
    WindowProperties props;
    props.title = "Vulkan Buffer Host Visible Test";
    props.width = 320;
    props.height = 240;
    props.resizable = false;

    Window window(props);

    VulkanContext context;
    context.Init(&window);

    VulkanBuffer buffer;
    buffer.Create(
        &context,
        sizeof(std::array<std::uint32_t, 4>),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    std::array<std::uint32_t, 4> source = { 0x12345678u, 0xABCDEF01u, 0x0u, 0xFFFFFFFFu };
    buffer.CopyFrom(source.data(), sizeof(source));

    auto* mapped = static_cast<std::uint32_t*>(buffer.Map());
    std::array<std::uint32_t, 4> readback{};
    std::copy(mapped, mapped + readback.size(), readback.begin());
    buffer.Unmap();

    ASSERT(readback == source);

    buffer.Destroy();
    context.Shutdown();
}

TEST(VulkanBuffer_DeviceLocalUploadUsesStaging) {
    WindowProperties props;
    props.title = "Vulkan Buffer Device Local Test";
    props.width = 320;
    props.height = 240;
    props.resizable = false;

    Window window(props);

    VulkanContext context;
    context.Init(&window);

    const std::array<std::uint32_t, 8> source = {
        0, 1, 2, 3, 4, 5, 6, 7
    };

    VulkanBuffer deviceLocalBuffer;
    deviceLocalBuffer.Create(
        &context,
        sizeof(source),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    deviceLocalBuffer.CopyFrom(source.data(), sizeof(source));

    VulkanBuffer readbackBuffer;
    readbackBuffer.Create(
        &context,
        sizeof(source),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    ImmediateCopyBuffer(&context, deviceLocalBuffer.GetBuffer(), readbackBuffer.GetBuffer(), sizeof(source));

    auto* mapped = static_cast<std::uint32_t*>(readbackBuffer.Map());
    std::array<std::uint32_t, source.size()> readback{};
    std::copy(mapped, mapped + readback.size(), readback.begin());
    readbackBuffer.Unmap();

    ASSERT(readback == source);

    readbackBuffer.Destroy();
    deviceLocalBuffer.Destroy();
    context.Shutdown();
}

TEST(VulkanBuffer_MoveTransfersOwnership) {
    WindowProperties props;
    props.title = "Vulkan Buffer Move Test";
    props.width = 320;
    props.height = 240;
    props.resizable = false;

    Window window(props);

    VulkanContext context;
    context.Init(&window);

    VulkanBuffer buffer;
    buffer.Create(
        &context,
        1024,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkBuffer originalHandle = buffer.GetBuffer();
    ASSERT(originalHandle != VK_NULL_HANDLE);

    VulkanBuffer moved = std::move(buffer);
    ASSERT(buffer.GetBuffer() == VK_NULL_HANDLE);
    ASSERT(moved.GetBuffer() == originalHandle);

    VulkanBuffer reassigned;
    reassigned = std::move(moved);
    ASSERT(moved.GetBuffer() == VK_NULL_HANDLE);
    ASSERT(reassigned.GetBuffer() == originalHandle);

    reassigned.Destroy();
    context.Shutdown();
}

TEST(VulkanBuffer_CreateAndUploadVertexBuffer) {
    WindowProperties props;
    props.title = "Vulkan Vertex Buffer Upload Test";
    props.width = 320;
    props.height = 240;
    props.resizable = false;

    Window window(props);

    VulkanContext context;
    context.Init(&window);

    std::array<Vertex, 3> vertices = {
        Vertex{Vec3(-0.5f, -0.5f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(1.0f, 0.0f, 0.0f), Vec2(0.0f, 0.0f)},
        Vertex{Vec3(0.5f, -0.5f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(1.0f, 0.0f)},
        Vertex{Vec3(0.0f, 0.5f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f), Vec2(0.5f, 1.0f)}
    };

    VulkanBuffer vertexBuffer;
    vertexBuffer.CreateAndUpload(
        &context,
        sizeof(vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertices.data());

    ASSERT(vertexBuffer.GetSize() == sizeof(vertices));
    ASSERT(vertexBuffer.GetUsage() & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    VulkanBuffer readback;
    readback.Create(
        &context,
        sizeof(vertices),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    ImmediateCopyBuffer(&context, vertexBuffer.GetBuffer(), readback.GetBuffer(), sizeof(vertices));

    auto* mapped = static_cast<Vertex*>(readback.Map());
    std::array<Vertex, vertices.size()> readbackData{};
    std::copy(mapped, mapped + readbackData.size(), readbackData.begin());
    readback.Unmap();

    for (size_t i = 0; i < vertices.size(); ++i) {
        ASSERT(std::memcmp(&readbackData[i], &vertices[i], sizeof(Vertex)) == 0);
    }

    readback.Destroy();
    vertexBuffer.Destroy();
    context.Shutdown();
}

int main() {
    std::cout << "=== Vulkan Buffer Tests ===" << std::endl << std::endl;

    VulkanBuffer_HostVisibleUploadCopiesData_runner();
    VulkanBuffer_DeviceLocalUploadUsesStaging_runner();
    VulkanBuffer_MoveTransfersOwnership_runner();
    VulkanBuffer_CreateAndUploadVertexBuffer_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed == 0 ? 0 : 1;
}
