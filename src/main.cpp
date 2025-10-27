#include "platform/window.h"
#include "platform/input.h"
#include "core/time.h"
#include "renderer/vulkan_context.h"
#include "renderer/vulkan_renderer.h"

#include <iostream>
#include <string>

int main() {
    std::cout << "=== Vulkan Rotating Cube ===" << std::endl;

    WindowProperties props;
    props.title = "Rotating Cube";
    props.width = 1280;
    props.height = 720;
    props.vsync = true;
    props.resizable = true;
    props.fullscreen = false;

    Window window(props);

    Input::Init(&window);
    Time::Init();

    VulkanContext context;
    context.Init(&window);

    VulkanRenderer renderer;
    renderer.Init(&context, &window);

    window.SetEventCallback([&renderer](WindowEvent event, u32 width, u32 height) {
        (void)width;
        (void)height;
        if (event == WindowEvent::Resize) {
            renderer.OnWindowResized();
        }
    });

    while (!window.ShouldClose()) {
        Time::Update();
        Input::Update();
        window.PollEvents();

        if (Input::IsKeyPressed(KeyCode::Escape)) {
            break;
        }

        renderer.DrawFrame();

        if (Time::FrameCount() % 60 == 0) {
            const std::string title = "Rotating Cube - FPS: " + std::to_string(static_cast<int>(Time::FPS()));
            window.SetTitle(title);
        }
    }

    renderer.Shutdown();
    context.Shutdown();

    std::cout << "Engine shutdown complete." << std::endl;
    return 0;
}
