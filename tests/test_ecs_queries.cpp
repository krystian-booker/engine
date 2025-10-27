#include "ecs/ecs_coordinator.h"
#include <iostream>
#include <vector>

// Test framework macros (copied pattern from existing tests)
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

struct PositionComponent {
    int value;
};

struct VelocityComponent {
    int value;
};

TEST(QueryEntities_HandlesImbalancedComponentSets) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<PositionComponent>();
    coordinator.RegisterComponent<VelocityComponent>();

    const int manyEntities = 50;
    std::vector<Entity> velocityOnly;
    std::vector<Entity> both;

    for (int i = 0; i < manyEntities; ++i) {
        Entity e = coordinator.CreateEntity();
        VelocityComponent vel{ i };
        coordinator.AddComponent(e, vel);
        velocityOnly.push_back(e);
    }

    for (int i = 0; i < 5; ++i) {
        Entity e = coordinator.CreateEntity();
        PositionComponent pos{ i };
        VelocityComponent vel{ i * 10 };
        coordinator.AddComponent(e, pos);
        coordinator.AddComponent(e, vel);
        both.push_back(e);
    }

    auto result = coordinator.QueryEntities<PositionComponent, VelocityComponent>();

    ASSERT(result.size() == both.size());
    for (size_t i = 0; i < both.size(); ++i) {
        ASSERT(result[i] == both[i]);
    }

    coordinator.Shutdown();
}

TEST(ForEach_AllowsMutableComponentAccess) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<PositionComponent>();

    Entity e1 = coordinator.CreateEntity();
    Entity e2 = coordinator.CreateEntity();

    coordinator.AddComponent(e1, PositionComponent{ 1 });
    coordinator.AddComponent(e2, PositionComponent{ 2 });

    coordinator.ForEach<PositionComponent>([](Entity, PositionComponent& pos) {
        pos.value *= 2;
    });

    ASSERT(coordinator.GetComponent<PositionComponent>(e1).value == 2);
    ASSERT(coordinator.GetComponent<PositionComponent>(e2).value == 4);

    coordinator.Shutdown();
}

int main() {
    std::cout << "=== ECS Query Tests ===" << std::endl;
    std::cout << std::endl;

    QueryEntities_HandlesImbalancedComponentSets_runner();
    ForEach_AllowsMutableComponentAccess_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
