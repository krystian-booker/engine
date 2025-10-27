#include "ecs/ecs_coordinator.h"
#include "core/job_system.h"
#include <atomic>
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

struct ParallelComponent {
    int value;
};

TEST(ForEachParallel_ProcessesAllEntities) {
    JobSystem::Init();

    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<ParallelComponent>();

    const int entityCount = 64;
    int expectedSum = 0;

    for (int i = 0; i < entityCount; ++i) {
        Entity e = coordinator.CreateEntity();
        coordinator.AddComponent(e, ParallelComponent{ i });
        expectedSum += i;
    }

    std::atomic<int> actualSum{ 0 };

    coordinator.ForEachParallel<ParallelComponent>(8, [&](Entity, ParallelComponent& comp) {
        actualSum.fetch_add(comp.value, std::memory_order_relaxed);
    });

    ASSERT(actualSum.load() == expectedSum);

    coordinator.Shutdown();
    JobSystem::Shutdown();
}

int main() {
    std::cout << "=== ECS Parallel Tests ===" << std::endl;
    std::cout << std::endl;

    ForEachParallel_ProcessesAllEntities_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
