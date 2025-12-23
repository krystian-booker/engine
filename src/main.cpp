#include "core/time.h"
#include "core/job_system.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/components/transform.h"
#include "ecs/components/rotator.h"

#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "=== Headless Engine Startup ===" << std::endl;

    // Initialize core systems
    Time::Init();
    JobSystem::Init();

    std::cout << "Core systems initialized" << std::endl;

    // Initialize ECS
    ECSCoordinator ecs;
    ecs.Init();

    std::cout << "ECS initialized" << std::endl;

    // Create test entities with transforms
    Entity entity1 = ecs.CreateEntity();
    Transform transform1;
    transform1.localPosition = Vec3(0, 0, 0);
    transform1.localRotation = Quat(1, 0, 0, 0);
    transform1.localScale = Vec3(1, 1, 1);
    ecs.AddComponent<Transform>(entity1, transform1);

    Entity entity2 = ecs.CreateEntity();
    Transform transform2;
    transform2.localPosition = Vec3(5, 0, 0);
    transform2.localRotation = Quat(1, 0, 0, 0);
    transform2.localScale = Vec3(1, 1, 1);
    ecs.AddComponent<Transform>(entity2, transform2);

    // Add rotator to second entity
    Rotator rotator;
    rotator.axis = Vec3(0, 1, 0);
    rotator.speed = 90.0f;  // degrees per second
    ecs.AddComponent<Rotator>(entity2, rotator);

    // Set up hierarchy: entity2 is child of entity1
    ecs.SetParent(entity2, entity1);

    std::cout << "Created " << ecs.GetEntityCount() << " entities" << std::endl;

    // Run a simple update loop
    const int updateCount = 100;
    std::cout << "Running " << updateCount << " update cycles..." << std::endl;

    for (int i = 0; i < updateCount; ++i) {
        Time::Update();
        const f32 deltaTime = Time::DeltaTime();

        // Manually update rotator (since we don't have a rotator system)
        ecs.ForEach<Rotator, Transform>([deltaTime](Entity, Rotator& rot, Transform& t) {
            if (rot.speed == 0.0f || deltaTime == 0.0f) {
                return;
            }

            Vec3 axis = rot.axis;
            const f32 axisLength = Length(axis);
            if (axisLength == 0.0f) {
                return;
            }

            axis /= axisLength;
            const f32 radians = Radians(rot.speed) * deltaTime;

            const Quat delta = QuatFromAxisAngle(axis, radians);
            t.localRotation = glm::normalize(delta * t.localRotation);
            t.MarkDirty();
        });

        // Update ECS (runs TransformSystem)
        ecs.Update(deltaTime);

        // Sleep to simulate frame timing
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Print final state
    std::cout << "Update loop complete" << std::endl;
    std::cout << "Total time: " << Time::TotalTime() << " seconds" << std::endl;
    std::cout << "Frame count: " << Time::FrameCount() << std::endl;

    // Get final transform of entity2
    if (ecs.HasComponent<Transform>(entity2)) {
        const Transform& t = ecs.GetComponent<Transform>(entity2);
        std::cout << "Entity2 world position: ("
                  << t.worldMatrix[3][0] << ", "
                  << t.worldMatrix[3][1] << ", "
                  << t.worldMatrix[3][2] << ")" << std::endl;
    }

    // Shutdown
    ecs.Shutdown();
    JobSystem::Shutdown();

    std::cout << "=== Headless Engine Shutdown Complete ===" << std::endl;
    return 0;
}
