#include "examples/test_scene.h"

#include "ecs/components/camera.h"
#include "ecs/components/transform.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rotator.h"
#include "resources/mesh_manager.h"

#include <iostream>

void CreateTestScene(ECSCoordinator& ecs) {
    std::cout << "Creating test scene..." << std::endl;

    MeshHandle cubeMesh = MeshManager::Instance().CreateCube();

    constexpr int minCoord = -2;
    constexpr int maxCoord = 2;
    constexpr f32 spacing = 2.5f;

    int spawned = 0;
    for (int x = minCoord; x <= maxCoord; ++x) {
        for (int z = minCoord; z <= maxCoord; ++z) {
            Entity entity = ecs.CreateEntity();

            Transform transform;
            transform.localPosition = Vec3(static_cast<f32>(x) * spacing, 0.0f, static_cast<f32>(z) * spacing);
            transform.localScale = Vec3(1.0f, 1.0f, 1.0f);
            transform.MarkDirty();
            ecs.AddComponent(entity, transform);

            Renderable renderable;
            renderable.mesh = cubeMesh;
            renderable.visible = true;
            ecs.AddComponent(entity, renderable);

            ++spawned;
        }
    }

    Entity floatingCube = ecs.CreateEntity();

    Transform floatTransform;
    floatTransform.localPosition = Vec3(0.0f, 3.0f, 0.0f);
    floatTransform.localScale = Vec3(0.5f, 0.5f, 0.5f);
    floatTransform.MarkDirty();
    ecs.AddComponent(floatingCube, floatTransform);

    Renderable floatRenderable;
    floatRenderable.mesh = cubeMesh;
    floatRenderable.visible = true;
    ecs.AddComponent(floatingCube, floatRenderable);

    Rotator rotator;
    rotator.axis = Vec3(0.0f, 1.0f, 0.0f);
    rotator.speed = 45.0f;
    ecs.AddComponent(floatingCube, rotator);

    Entity cameraEntity = ecs.CreateEntity();

    Transform cameraTransform;
    cameraTransform.localPosition = Vec3(0.0f, 5.0f, 10.0f);
    cameraTransform.localRotation = QuatFromAxisAngle(Vec3(1.0f, 0.0f, 0.0f), Radians(-25.0f));
    cameraTransform.MarkDirty();
    ecs.AddComponent(cameraEntity, cameraTransform);

    Camera cameraComponent;
    cameraComponent.isActive = true;
    cameraComponent.clearColor = Vec4(0.05f, 0.05f, 0.08f, 1.0f);
    cameraComponent.fov = 60.0f;
    cameraComponent.nearPlane = 0.1f;
    cameraComponent.farPlane = 500.0f;
    ecs.AddComponent(cameraEntity, cameraComponent);

    std::cout << "Created scene with " << (spawned + 1) << " cubes" << std::endl;
}
