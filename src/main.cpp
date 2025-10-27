#include "platform/window.h"
#include "platform/input.h"
#include "core/time.h"
#include "renderer/vulkan_context.h"
#include "renderer/vulkan_renderer.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/systems/render_system.h"
#include "ecs/components/transform.h"
#include "ecs/components/renderable.h"
#include "resources/mesh_manager.h"

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

    ECSCoordinator ecs;
    ecs.Init();

    RenderSystem renderSystem(&ecs, &context);
    renderer.SetRenderSystem(&renderSystem);

    Entity cubeEntity = ecs.CreateEntity();

    Transform cubeTransform;
    cubeTransform.localPosition = Vec3(0.0f, 0.0f, 0.0f);
    cubeTransform.localScale = Vec3(1.0f, 1.0f, 1.0f);
    ecs.AddComponent(cubeEntity, cubeTransform);

    MeshHandle cubeMesh = MeshManager::Instance().CreateCube();

    Renderable cubeRenderable;
    cubeRenderable.mesh = cubeMesh;
    cubeRenderable.visible = true;
    ecs.AddComponent(cubeEntity, cubeRenderable);

    ecs.Update(0.0f);
    renderSystem.UploadMeshes();

    window.SetEventCallback([&renderer](WindowEvent event, u32 width, u32 height) {
        (void)width;
        (void)height;
        if (event == WindowEvent::Resize) {
            renderer.OnWindowResized();
        }
    });

    Transform& cubeTransformRef = ecs.GetComponent<Transform>(cubeEntity);

    while (!window.ShouldClose()) {
        Time::Update();
        Input::Update();
        window.PollEvents();

        if (Input::IsKeyPressed(KeyCode::Escape)) {
            break;
        }

        const f32 rotationSpeed = Radians(45.0f);
        const f32 deltaRadians = rotationSpeed * Time::DeltaTime();
        if (deltaRadians != 0.0f) {
            const Quat delta = glm::angleAxis(deltaRadians, Vec3(0.0f, 1.0f, 0.0f));
            cubeTransformRef.localRotation = glm::normalize(delta * cubeTransformRef.localRotation);
            cubeTransformRef.MarkDirty();
        }

        ecs.Update(Time::DeltaTime());
        renderSystem.Update();
        renderer.DrawFrame();

        if (Time::FrameCount() % 60 == 0) {
            const std::string title = "Rotating Cube - FPS: " + std::to_string(static_cast<int>(Time::FPS()));
            window.SetTitle(title);
        }
    }

    renderSystem.Shutdown();
    MeshManager::Instance().Destroy(cubeMesh);

    ecs.Shutdown();

    renderer.Shutdown();
    context.Shutdown();

    std::cout << "Engine shutdown complete." << std::endl;
    return 0;
}
