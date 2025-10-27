#include "ecs/ecs_coordinator.h"
#include <iostream>

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
        testsRun++; \
        return; \
    }

struct DeferredComponent {
    int tag;
};

TEST(ForEach_DefersRemovalsUntilAfterIteration) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<DeferredComponent>();

    Entity keep = coordinator.CreateEntity();
    Entity remove = coordinator.CreateEntity();
    Entity destroy = coordinator.CreateEntity();

    coordinator.AddComponent(keep, DeferredComponent{ 0 });
    coordinator.AddComponent(remove, DeferredComponent{ 1 });
    coordinator.AddComponent(destroy, DeferredComponent{ 2 });

    int iterations = 0;

    coordinator.ForEach<DeferredComponent>([&](Entity entity, DeferredComponent& comp) {
        ++iterations;
        if (comp.tag == 1) {
            coordinator.RemoveComponent<DeferredComponent>(entity);
        }
        if (comp.tag == 2) {
            coordinator.DestroyEntity(entity);
        }
    });

    ASSERT(iterations == 3);
    ASSERT(coordinator.HasComponent<DeferredComponent>(keep));
    ASSERT(!coordinator.HasComponent<DeferredComponent>(remove));
    ASSERT(!coordinator.IsEntityAlive(destroy));

    coordinator.Shutdown();
}

TEST(SafeForEach_BlocksStructuralChanges) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<DeferredComponent>();

    Entity entity = coordinator.CreateEntity();
    coordinator.AddComponent(entity, DeferredComponent{ 42 });

#ifdef NDEBUG
    coordinator.SafeForEach<DeferredComponent>([&](Entity e, DeferredComponent&) {
        coordinator.RemoveComponent<DeferredComponent>(e);
    });

    ASSERT(coordinator.HasComponent<DeferredComponent>(entity));
#else
    bool visited = false;
    coordinator.SafeForEach<DeferredComponent>([&](Entity, DeferredComponent&) {
        visited = true;
    });

    ASSERT(visited);
    ASSERT(coordinator.HasComponent<DeferredComponent>(entity));
#endif

    coordinator.Shutdown();
}

int main() {
    std::cout << "=== ECS Deferred Ops Tests ===" << std::endl;
    std::cout << std::endl;

    ForEach_DefersRemovalsUntilAfterIteration_runner();
    SafeForEach_BlocksStructuralChanges_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
