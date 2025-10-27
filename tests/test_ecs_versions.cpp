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

struct VersionedComponent {
    int value;
};

TEST(ComponentVersions_IncrementOnMutation) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<VersionedComponent>();

    Entity entity = coordinator.CreateEntity();
    coordinator.AddComponent(entity, VersionedComponent{ 5 });

    u32 addVersion = coordinator.GetComponentVersion<VersionedComponent>(entity);
    ASSERT(addVersion != 0);

    VersionedComponent& mutableRef = coordinator.GetMutableComponent<VersionedComponent>(entity);
    mutableRef.value = 10;

    u32 mutableVersion = coordinator.GetComponentVersion<VersionedComponent>(entity);
    ASSERT(mutableVersion != addVersion);

    coordinator.MarkComponentDirty<VersionedComponent>(entity);
    u32 dirtyVersion = coordinator.GetComponentVersion<VersionedComponent>(entity);
    ASSERT(dirtyVersion != mutableVersion);

    coordinator.Shutdown();
}

int main() {
    std::cout << "=== ECS Version Tests ===" << std::endl;
    std::cout << std::endl;

    ComponentVersions_IncrementOnMutation_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
