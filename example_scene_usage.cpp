// Example usage of SceneSerializer
// This demonstrates how to use the scene serialization system

#include "ecs/ecs_coordinator.h"
#include "ecs/scene_serializer.h"
#include "ecs/components/transform.h"
#include <iostream>

int main() {
    // Initialize ECS
    ECSCoordinator ecs;
    ecs.Init();

    // Create a scene with hierarchy
    Entity parent = ecs.CreateEntity();
    Transform parentTransform;
    parentTransform.localPosition = Vec3(10.0f, 5.0f, 0.0f);
    parentTransform.localRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    parentTransform.localScale = Vec3(1.0f, 1.0f, 1.0f);
    ecs.AddComponent(parent, parentTransform);

    Entity child = ecs.CreateEntity();
    Transform childTransform;
    childTransform.localPosition = Vec3(2.0f, 0.0f, 0.0f);
    childTransform.localRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    childTransform.localScale = Vec3(0.5f, 0.5f, 0.5f);
    ecs.AddComponent(child, childTransform);

    ecs.SetParent(child, parent);

    // Save scene
    SceneSerializer serializer(&ecs);
    serializer.SaveScene("example_scene.json");

    std::cout << "\nScene saved! Check example_scene.json" << std::endl;
    std::cout << "Original entity count: " << ecs.GetEntityCount() << std::endl;

    // Clear scene
    ecs.DestroyEntity(parent);
    ecs.DestroyEntity(child);
    std::cout << "Cleared scene. Entity count: " << ecs.GetEntityCount() << std::endl;

    // Load scene
    serializer.LoadScene("example_scene.json");
    std::cout << "Loaded scene. Entity count: " << ecs.GetEntityCount() << std::endl;

    // Verify hierarchy
    auto entities = ecs.QueryEntities<Transform>();
    std::cout << "\nLoaded entities:" << std::endl;
    for (Entity e : entities) {
        const Transform& t = ecs.GetComponent<Transform>(e);
        std::cout << "  Entity " << e.index << ": pos("
                  << t.localPosition.x << ", "
                  << t.localPosition.y << ", "
                  << t.localPosition.z << ")" << std::endl;

        Entity p = ecs.GetParent(e);
        if (p.IsValid()) {
            std::cout << "    -> Parent: Entity " << p.index << std::endl;
        }
    }

    ecs.Shutdown();
    return 0;
}
