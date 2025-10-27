#include "core/time.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rotator.h"
#include "ecs/components/transform.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/systems/camera_system.h"
#include "ecs/systems/camera_controller.h"
#include "examples/test_scene.h"
#include "platform/input.h"
#include "platform/window.h"
#include "renderer/vulkan_context.h"
#include "renderer/vulkan_renderer.h"
#include "resources/mesh_manager.h"

#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "=== Vulkan Test Scene ===" << std::endl;

    WindowProperties props;
    props.title = "3D Scene";
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

    ECSCoordinator ecs;
    ecs.Init();

    CameraSystem* cameraSystem = ecs.GetCameraSystem();

    // Setup camera controller
    ecs.SetupCameraController(&window);
    CameraController* cameraController = ecs.GetCameraController();

    CreateTestScene(ecs);

    MeshHandle cubeMeshHandle = MeshHandle::Invalid;
    std::vector<Entity> renderableEntities = ecs.QueryEntities<Renderable>();
    if (!renderableEntities.empty()) {
        cubeMeshHandle = ecs.GetComponent<Renderable>(renderableEntities.front()).mesh;
    }

    ecs.Update(0.0f);
    if (cameraSystem) {
        cameraSystem->Update(window.GetWidth(), window.GetHeight());
    }

    // Set the active camera as the controlled camera (AFTER camera system has found it)
    if (cameraController && cameraSystem) {
        Entity activeCamera = cameraSystem->GetActiveCamera();
        if (activeCamera.IsValid()) {
            cameraController->SetControlledCamera(activeCamera);
        }
    }

    VulkanRenderer renderer;
    renderer.Init(&context, &window, &ecs);

    window.SetEventCallback([&renderer, cameraSystem](WindowEvent event, u32 width, u32 height) {
        if (event == WindowEvent::Resize) {
            renderer.OnWindowResized();
            if (cameraSystem) {
                cameraSystem->Update(width, height);
            }
        }
    });

    while (!window.ShouldClose()) {
        Time::Update();
        Input::Update();
        window.PollEvents();

        if (Input::IsKeyPressed(KeyCode::Escape)) {
            break;
        }

        const f32 deltaTime = Time::DeltaTime();

        // Update camera controller
        if (cameraController) {
            cameraController->Update(deltaTime);
        }

        ecs.ForEach<Rotator, Transform>([deltaTime](Entity, Rotator& rotator, Transform& transform) {
            if (rotator.speed == 0.0f || deltaTime == 0.0f) {
                return;
            }

            Vec3 axis = rotator.axis;
            const f32 axisLength = Length(axis);
            if (axisLength == 0.0f) {
                return;
            }

            axis /= axisLength;
            const f32 radians = Radians(rotator.speed) * deltaTime;
            if (radians == 0.0f) {
                return;
            }

            const Quat delta = QuatFromAxisAngle(axis, radians);
            transform.localRotation = glm::normalize(delta * transform.localRotation);
            transform.MarkDirty();
        });

        ecs.Update(deltaTime);

        if (cameraSystem) {
            cameraSystem->Update(window.GetWidth(), window.GetHeight());
        }

        renderer.DrawFrame();

        if (Time::FrameCount() % 60 == 0) {
            const std::string title = "3D Scene - FPS: " + std::to_string(static_cast<int>(Time::FPS())) +
                " - Objects: " + std::to_string(ecs.QueryEntities<Renderable>().size());
            window.SetTitle(title);
        }
    }

    renderer.Shutdown();

    if (cubeMeshHandle.IsValid()) {
        MeshManager::Instance().Destroy(cubeMeshHandle);
    }

    ecs.Shutdown();
    context.Shutdown();

    std::cout << "Engine shutdown complete." << std::endl;
    return 0;
}
