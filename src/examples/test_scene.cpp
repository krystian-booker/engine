#include "examples/test_scene.h"

#include "ecs/components/camera.h"
#include "ecs/components/transform.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rotator.h"
#include "resources/mesh_manager.h"

#include <iostream>

void CreateTestScene(ECSCoordinator& ecs) {
    std::cout << "Creating test scene..." << std::endl;

    // Load the avocado model WITH embedded textures and material
    MeshLoadResult avocadoResult = MeshManager::Instance().LoadWithMaterial(ENGINE_SOURCE_DIR "/assets/models/Avocado.fbx");

    // Check if it's a multi-mesh file
    if (avocadoResult.HasSubMeshes()) {
        std::cout << "Avocado has " << avocadoResult.subMeshes.size() << " sub-meshes, loading all..." << std::endl;

        // Create a parent entity
        Entity avocadoParent = ecs.CreateEntity();
        Transform parentTransform;
        parentTransform.localPosition = Vec3(0.0f, 0.0f, 0.0f);
        parentTransform.localScale = Vec3(20.0f, 20.0f, 20.0f);  // Scale up the avocado
        parentTransform.MarkDirty();
        ecs.AddComponent(avocadoParent, parentTransform);

        // Add rotator to parent for nice spinning effect
        Rotator rotator;
        rotator.axis = Vec3(0.0f, 1.0f, 0.0f);
        rotator.speed = 30.0f;
        ecs.AddComponent(avocadoParent, rotator);

        // Create entities for each sub-mesh with their materials
        for (const auto& subResult : avocadoResult.subMeshes) {
            Entity subEntity = ecs.CreateEntity();
            ecs.SetParent(subEntity, avocadoParent);

            Transform subTransform;
            subTransform.localPosition = Vec3(0.0f, 0.0f, 0.0f);
            subTransform.localScale = Vec3(1.0f, 1.0f, 1.0f);
            subTransform.MarkDirty();
            ecs.AddComponent(subEntity, subTransform);

            Renderable subRenderable;
            subRenderable.mesh = subResult.mesh;
            subRenderable.material = subResult.material;  // Assign loaded material with embedded textures!
            subRenderable.visible = true;
            ecs.AddComponent(subEntity, subRenderable);
        }
    } else if (avocadoResult.IsValid()) {
        // Single mesh - display directly
        Entity avocadoEntity = ecs.CreateEntity();

        Transform avocadoTransform;
        avocadoTransform.localPosition = Vec3(0.0f, 0.0f, 0.0f);
        avocadoTransform.localScale = Vec3(20.0f, 20.0f, 20.0f);  // Scale up the avocado
        avocadoTransform.MarkDirty();
        ecs.AddComponent(avocadoEntity, avocadoTransform);

        Renderable avocadoRenderable;
        avocadoRenderable.mesh = avocadoResult.mesh;
        avocadoRenderable.material = avocadoResult.material;  // Assign loaded material with embedded textures!
        avocadoRenderable.visible = true;
        ecs.AddComponent(avocadoEntity, avocadoRenderable);

        // Add rotation for visual interest
        Rotator rotator;
        rotator.axis = Vec3(0.0f, 1.0f, 0.0f);
        rotator.speed = 30.0f;
        ecs.AddComponent(avocadoEntity, rotator);
    } else {
        std::cerr << "Failed to load Avocado.fbx!" << std::endl;
    }

    // Add a ground plane for reference
    MeshHandle planeMesh = MeshManager::Instance().CreatePlane();
    Entity groundEntity = ecs.CreateEntity();

    Transform groundTransform;
    groundTransform.localPosition = Vec3(0.0f, -5.0f, 0.0f);
    groundTransform.localScale = Vec3(10.0f, 1.0f, 10.0f);
    groundTransform.MarkDirty();
    ecs.AddComponent(groundEntity, groundTransform);

    Renderable groundRenderable;
    groundRenderable.mesh = planeMesh;
    groundRenderable.visible = true;
    ecs.AddComponent(groundEntity, groundRenderable);

    // Setup camera to view the avocado
    Entity cameraEntity = ecs.CreateEntity();

    Transform cameraTransform;
    cameraTransform.localPosition = Vec3(0.0f, 0.0f, 15.0f);  // Back away from the avocado
    cameraTransform.localRotation = QuatFromAxisAngle(Vec3(1.0f, 0.0f, 0.0f), Radians(0.0f));
    cameraTransform.MarkDirty();
    ecs.AddComponent(cameraEntity, cameraTransform);

    Camera cameraComponent;
    cameraComponent.isActive = true;
    cameraComponent.clearColor = Vec4(0.1f, 0.1f, 0.15f, 1.0f);  // Slightly brighter background
    cameraComponent.fov = 60.0f;
    cameraComponent.nearPlane = 0.1f;
    cameraComponent.farPlane = 500.0f;
    ecs.AddComponent(cameraEntity, cameraComponent);

    std::cout << "Created scene with avocado model!" << std::endl;
}
