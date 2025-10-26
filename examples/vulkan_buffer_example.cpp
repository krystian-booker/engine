#include "platform/window.h"
#include "renderer/vulkan_buffer.h"
#include "renderer/vulkan_context.h"

#include <cstdint>
#include <array>
#include <iostream>

struct Vertex {
    float position[3];
    float color[3];
};

struct UniformBlock {
    float model[16];
    float view[16];
    float projection[16];
};

int main() {
    std::cout << "=== Vulkan Buffer Example ===" << std::endl;

    WindowProperties props;
    props.title = "Vulkan Buffer Example";
    props.width = 800;
    props.height = 600;
    props.resizable = false;

    Window window(props);

    VulkanContext context;
    context.Init(&window);

    std::array<Vertex, 3> vertices = {{
        {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}
    }};

    std::array<std::uint16_t, 3> indices = {{0, 1, 2}};

    UniformBlock ubo{};
    for (int i = 0; i < 16; ++i) {
        ubo.model[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        ubo.view[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        ubo.projection[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }

    VulkanBuffer vertexBuffer;
    vertexBuffer.Create(
        &context,
        sizeof(vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VulkanBuffer indexBuffer;
    indexBuffer.Create(
        &context,
        sizeof(indices),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VulkanBuffer uniformBuffer;
    uniformBuffer.Create(
        &context,
        sizeof(UniformBlock),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    std::cout << "Uploading vertex data (" << sizeof(vertices) << " bytes)..." << std::endl;
    vertexBuffer.CopyFrom(vertices.data(), sizeof(vertices));

    std::cout << "Uploading index data (" << sizeof(indices) << " bytes)..." << std::endl;
    indexBuffer.CopyFrom(indices.data(), sizeof(indices));

    std::cout << "Uploading uniform data (" << sizeof(UniformBlock) << " bytes)..." << std::endl;
    uniformBuffer.CopyFrom(&ubo, sizeof(UniformBlock));

    std::cout << "Mapping uniform buffer to tweak first color component..." << std::endl;
    auto* mapped = static_cast<std::uint8_t*>(uniformBuffer.Map());
    mapped[0] = 0x3f; // Just poke the buffer to demonstrate CPU access
    uniformBuffer.Unmap();

    std::cout << "Vertex buffer handle: " << vertexBuffer.GetBuffer() << std::endl;
    std::cout << "Index buffer handle: " << indexBuffer.GetBuffer() << std::endl;
    std::cout << "Uniform buffer handle: " << uniformBuffer.GetBuffer() << std::endl;

    uniformBuffer.Destroy();
    indexBuffer.Destroy();
    vertexBuffer.Destroy();

    context.Shutdown();

    std::cout << "=== Vulkan Buffer Example Complete ===" << std::endl;
    return 0;
}
