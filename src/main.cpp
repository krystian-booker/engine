#include "core/time.h"
#include "core/scene_manager.h"
#include "core/engine_settings.h"
#include "core/project_manager.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rotator.h"
#include "ecs/components/transform.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/systems/camera_system.h"
#include "ecs/systems/camera_controller.h"
#include "ecs/systems/editor_camera_controller.h"
#include "platform/input.h"
#include "platform/window.h"
#include "renderer/vulkan_context.h"
#include "renderer/vulkan_renderer.h"
#include "renderer/viewport_manager.h"
#include "resources/mesh_manager.h"
#include "resources/texture_manager.h"
#include "resources/material_manager.h"
#include "ui/imgui_project_picker.h"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

int main() {
    std::cout << "=== Engine Startup ===" << std::endl;

    bool shouldRestart = false;
    do {
        shouldRestart = false;  // Reset for this iteration

        // Load engine settings
        EngineSettings engineSettings = EngineSettings::Load();

        // Initialize project manager
        ProjectManager projectManager;

        // Check if we should skip project picker
        bool needsProjectPicker = true;
        if (engineSettings.skipProjectPicker && !engineSettings.defaultProjectPath.empty()) {
            std::cout << "Loading default project: " << engineSettings.defaultProjectPath << std::endl;
            if (projectManager.LoadProject(engineSettings.defaultProjectPath)) {
                needsProjectPicker = false;
            } else {
                std::cerr << "Failed to load default project, showing project picker" << std::endl;
                // Clear invalid default
                engineSettings.skipProjectPicker = false;
                engineSettings.defaultProjectPath = "";
                engineSettings.Save();
            }
        }

        // Show project picker if needed
        if (needsProjectPicker) {
            std::cout << "Showing project selection window..." << std::endl;

            // Show project picker using Dear ImGui
            ImGuiProjectPicker projectPicker(&projectManager);
            ProjectPickerResult pickerResult = projectPicker.Show();

            // Handle picker result
            if (pickerResult.cancelled || !pickerResult.success) {
                std::cout << "Project selection cancelled. Exiting." << std::endl;
                return 0;
            }

            // Update engine settings if user requested default project
            if (pickerResult.setAsDefault) {
                engineSettings.skipProjectPicker = true;
                engineSettings.defaultProjectPath = pickerResult.projectPath;
                engineSettings.Save();
                std::cout << "Set default project: " << pickerResult.projectPath << std::endl;
            }
        }

        // At this point, we have a valid project loaded
        if (!projectManager.HasActiveProject()) {
            std::cerr << "No project loaded. Exiting." << std::endl;
            return 1;
        }

        const ProjectConfig& project = projectManager.GetProject();
        std::cout << "Loaded project: " << project.name << std::endl;
        std::cout << "Project path: " << project.rootPath << std::endl;

        // Set working directory to project root
        std::filesystem::current_path(project.rootPath);

        std::cout << "=== Starting Engine ===" << std::endl;

        WindowProperties props;
        props.title = project.name;
        props.width = project.windowWidth;
        props.height = project.windowHeight;
        props.vsync = project.windowVSync;
        props.resizable = true;
        props.fullscreen = project.windowFullscreen;

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

        // Create Scene Manager with camera system and controller references
        SceneManager sceneManager(&ecs, cameraSystem, cameraController);

        // Initialize renderer BEFORE creating scene so TextureManager has access to descriptors
        VulkanRenderer renderer;
        renderer.Init(&context, &window, &ecs, &sceneManager);

        // Initialize viewport manager and editor camera controller
        ViewportManager viewportManager;
        viewportManager.Init(&context);

        EditorCameraController editorCameraController(&ecs, &window);

        // Create editor camera (persistent across scenes)
        Entity editorCamera = sceneManager.EnsureEditorCamera();
        editorCameraController.SetControlledCamera(editorCamera);

        // Create default viewports (Scene and Game)
        u32 sceneViewportID = viewportManager.CreateViewport(800, 600, editorCamera, ViewportType::Scene);
        u32 gameViewportID = viewportManager.CreateViewport(800, 600, Entity::Invalid, ViewportType::Game);

        std::cout << "Created Scene viewport (ID: " << sceneViewportID << ") and Game viewport (ID: " << gameViewportID << ")" << std::endl;

        // Load last opened scene from project (if available)
        if (!project.lastOpenedScene.empty()) {
            std::string scenePath = projectManager.GetAbsolutePath(project.lastOpenedScene);
            std::cout << "Loading last opened scene: " << scenePath << std::endl;
            if (!sceneManager.LoadScene(scenePath)) {
                std::cerr << "Failed to load last opened scene, starting with empty scene" << std::endl;
                sceneManager.NewScene();
            }
        } else {
            std::cout << "No last opened scene, starting with empty scene" << std::endl;
            sceneManager.NewScene();
        }

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

        // Get focused viewport ID from ImGui (0 = none focused)
        u32 focusedViewportID = 0;
#ifdef _DEBUG
        // Get the actual focused viewport from ImGuiLayer
        focusedViewportID = renderer.GetImGuiLayer()->GetFocusedViewportID();
#endif

        // Update editor camera controller (only when scene viewport is focused)
        editorCameraController.SetEnabled(focusedViewportID == sceneViewportID);
        editorCameraController.Update(deltaTime);

        // Update game camera controller (only when game viewport is focused)
        if (cameraController) {
            // Simple approach: disable game camera control when scene viewport is focused
            // In the future, enable only when game viewport is focused
            if (focusedViewportID != sceneViewportID) {
                cameraController->Update(deltaTime);
            }
        }

        // Update game viewport to use active game camera
        Viewport* gameViewport = viewportManager.GetViewport(gameViewportID);
        if (gameViewport && cameraSystem) {
            Entity activeGameCamera = cameraSystem->GetActiveCamera();
            if (activeGameCamera.IsValid() && activeGameCamera != editorCamera) {
                gameViewport->SetCamera(activeGameCamera);
            }
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

        // Process async texture uploads
        TextureManager::Instance().Update();

        // Render with viewport manager
        renderer.DrawFrame(&viewportManager);

        if (Time::FrameCount() % 60 == 0) {
            std::string title = "Game Engine";

            // Show current scene file name if available
            if (sceneManager.HasCurrentFile()) {
                std::string scenePath = sceneManager.GetCurrentFilePath();
                // Extract filename from path
                size_t lastSlash = scenePath.find_last_of("/\\");
                std::string filename = (lastSlash != std::string::npos) ? scenePath.substr(lastSlash + 1) : scenePath;
                title += " - " + filename;
            } else {
                title += " - Untitled";
            }

            // Show dirty indicator
            if (sceneManager.IsDirty()) {
                title += "*";
            }

            // Show FPS and object count
            title += " - FPS: " + std::to_string(static_cast<int>(Time::FPS())) +
                " - Objects: " + std::to_string(ecs.QueryEntities<Renderable>().size());

            window.SetTitle(title);
        }
        }

        // Check if user requested to change project
#ifdef _DEBUG
        bool shouldChangeProject = renderer.ShouldChangeProject();
#else
        bool shouldChangeProject = false;
#endif

        // Save current scene and update project before shutdown
        if (sceneManager.HasCurrentFile() && sceneManager.IsDirty()) {
            std::cout << "Saving current scene..." << std::endl;
            sceneManager.SaveScene();
        }

        // Update project's last opened scene
        if (sceneManager.HasCurrentFile()) {
            std::string scenePath = sceneManager.GetCurrentFilePath();
            // Convert to relative path if possible
            std::filesystem::path sceneAbsPath(scenePath);
            std::filesystem::path projectRoot(project.rootPath);
            try {
                std::filesystem::path relativePath = std::filesystem::relative(sceneAbsPath, projectRoot);
                projectManager.SetLastOpenedScene(relativePath.string());
            } catch (...) {
                projectManager.SetLastOpenedScene(scenePath);
            }
        }

        // Save project configuration
        if (projectManager.IsDirty()) {
            std::cout << "Saving project configuration..." << std::endl;
            projectManager.SaveProject();
        }

        renderer.Shutdown();
        viewportManager.Shutdown();

        if (cubeMeshHandle.IsValid()) {
            MeshManager::Instance().Destroy(cubeMeshHandle);
        }

        ecs.Shutdown();
        context.Shutdown();

        std::cout << "Engine shutdown complete." << std::endl;

        // If user requested project change, restart with project picker
        if (shouldChangeProject) {
            std::cout << "Restarting with project picker..." << std::endl;
            // Note: Settings were already cleared when user clicked OK in the dialog

            // Set flag to restart the main loop
            shouldRestart = true;
        }
    } while (shouldRestart);

    return 0;
}
