#include "ecs/ecs_coordinator.h"
#include "ecs/components/renderable.h"
#include "ecs/components/transform.h"
#include <iostream>

// Simple test harness (mirrors other tests in this suite)
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
        } catch (...) { \
            testsFailed++; \
            std::cout << "FAILED (exception)" << std::endl; \
        } \
    } \
    static void name()

#define ASSERT(expr) \
    if (!(expr)) { \
        std::cout << "FAILED at line " << __LINE__ << ": " << #expr << std::endl; \
        testsFailed++; \
        return; \
    }

TEST(RenderableComponent_Defaults) {
    Renderable renderable;

    ASSERT(renderable.mesh == MeshHandle::Invalid);
    ASSERT(renderable.visible);
    ASSERT(renderable.castsShadows);
}

TEST(RenderableComponent_AddToEntity) {
    ECSCoordinator coordinator;
    coordinator.Init();

    Entity entity = coordinator.CreateEntity();
    ASSERT(entity.IsValid());

    Renderable renderable;
    coordinator.AddComponent(entity, renderable);

    ASSERT(coordinator.HasComponent<Renderable>(entity));

    Renderable& stored = coordinator.GetComponent<Renderable>(entity);
    ASSERT(stored.mesh == MeshHandle::Invalid);
    ASSERT(stored.visible);
    ASSERT(stored.castsShadows);

    coordinator.Shutdown();
}

TEST(RenderableComponent_Modify) {
    ECSCoordinator coordinator;
    coordinator.Init();

    Entity entity = coordinator.CreateEntity();
    Renderable renderable;

    MeshHandle customHandle;
    customHandle.index = 42;
    customHandle.generation = 7;
    renderable.mesh = customHandle;
    renderable.visible = false;
    renderable.castsShadows = false;

    coordinator.AddComponent(entity, renderable);

    Renderable& stored = coordinator.GetComponent<Renderable>(entity);
    ASSERT(stored.mesh == customHandle);
    ASSERT(!stored.visible);
    ASSERT(!stored.castsShadows);

    stored.visible = true;
    stored.castsShadows = true;

    const Renderable& reread = coordinator.GetComponent<Renderable>(entity);
    ASSERT(reread.visible);
    ASSERT(reread.castsShadows);

    coordinator.Shutdown();
}

TEST(RenderableComponent_WithTransformQuery) {
    ECSCoordinator coordinator;
    coordinator.Init();

    Entity entity = coordinator.CreateEntity();

    Transform transform;
    coordinator.AddComponent(entity, transform);

    Renderable renderable;
    coordinator.AddComponent(entity, renderable);

    int iterations = 0;
    coordinator.ForEach<Transform, Renderable>([&](Entity iterEntity, Transform&, Renderable& rend) {
        ASSERT(iterEntity == entity);
        ASSERT(rend.visible);
        ASSERT(rend.mesh == MeshHandle::Invalid);
        iterations++;
    });

    ASSERT(iterations == 1);

    coordinator.Shutdown();
}

int main() {
    std::cout << "=== Renderable Component Tests ===" << std::endl << std::endl;

    RenderableComponent_Defaults_runner();
    RenderableComponent_AddToEntity_runner();
    RenderableComponent_Modify_runner();
    RenderableComponent_WithTransformQuery_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed == 0 ? 0 : 1;
}

