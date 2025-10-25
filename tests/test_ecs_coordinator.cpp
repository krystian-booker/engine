#include "ecs/ecs_coordinator.h"
#include "ecs/components/transform.h"
#include <iostream>
#include <cmath>

// Test result tracking
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

// Helper function to compare floats with epsilon
bool FloatEqual(f32 a, f32 b, f32 epsilon = 0.0001f) {
    return std::fabs(a - b) < epsilon;
}

// Helper function to compare Vec3 with epsilon
bool Vec3Equal(const Vec3& a, const Vec3& b, f32 epsilon = 0.0001f) {
    return FloatEqual(a.x, b.x, epsilon) &&
           FloatEqual(a.y, b.y, epsilon) &&
           FloatEqual(a.z, b.z, epsilon);
}

// Helper function to compare Mat4 with epsilon
bool Mat4Equal(const Mat4& a, const Mat4& b, f32 epsilon = 0.0001f) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (!FloatEqual(a[i][j], b[i][j], epsilon)) {
                return false;
            }
        }
    }
    return true;
}

// Test component types
struct TestComponent {
    Vec3 value;
};

struct AnotherComponent {
    f32 health;
    f32 maxHealth;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST(ECSCoordinator_InitShutdown) {
    ECSCoordinator coordinator;

    // Should not crash
    coordinator.Init();
    coordinator.Shutdown();
}

TEST(ECSCoordinator_MultipleInitShutdown) {
    ECSCoordinator coordinator;

    // Should handle multiple init/shutdown cycles
    coordinator.Init();
    coordinator.Shutdown();

    coordinator.Init();
    coordinator.Shutdown();
}

// ============================================================================
// Entity API Tests
// ============================================================================

TEST(ECSCoordinator_CreateEntity) {
    ECSCoordinator coordinator;
    coordinator.Init();

    Entity e1 = coordinator.CreateEntity();

    ASSERT(e1.IsValid());
    ASSERT(coordinator.IsEntityAlive(e1));
    ASSERT(coordinator.GetEntityCount() == 1);

    coordinator.Shutdown();
}

TEST(ECSCoordinator_CreateMultipleEntities) {
    ECSCoordinator coordinator;
    coordinator.Init();

    Entity e1 = coordinator.CreateEntity();
    Entity e2 = coordinator.CreateEntity();
    Entity e3 = coordinator.CreateEntity();

    ASSERT(e1.IsValid());
    ASSERT(e2.IsValid());
    ASSERT(e3.IsValid());

    ASSERT(e1 != e2);
    ASSERT(e1 != e3);
    ASSERT(e2 != e3);

    ASSERT(coordinator.GetEntityCount() == 3);

    coordinator.Shutdown();
}

TEST(ECSCoordinator_DestroyEntity) {
    ECSCoordinator coordinator;
    coordinator.Init();

    Entity e1 = coordinator.CreateEntity();
    ASSERT(coordinator.IsEntityAlive(e1));

    coordinator.DestroyEntity(e1);
    ASSERT(!coordinator.IsEntityAlive(e1));
    ASSERT(coordinator.GetEntityCount() == 0);

    coordinator.Shutdown();
}

TEST(ECSCoordinator_DestroyMultipleEntities) {
    ECSCoordinator coordinator;
    coordinator.Init();

    Entity e1 = coordinator.CreateEntity();
    Entity e2 = coordinator.CreateEntity();
    Entity e3 = coordinator.CreateEntity();

    ASSERT(coordinator.GetEntityCount() == 3);

    coordinator.DestroyEntity(e2);
    ASSERT(coordinator.IsEntityAlive(e1));
    ASSERT(!coordinator.IsEntityAlive(e2));
    ASSERT(coordinator.IsEntityAlive(e3));
    ASSERT(coordinator.GetEntityCount() == 2);

    coordinator.DestroyEntity(e1);
    ASSERT(!coordinator.IsEntityAlive(e1));
    ASSERT(coordinator.IsEntityAlive(e3));
    ASSERT(coordinator.GetEntityCount() == 1);

    coordinator.Shutdown();
}

// ============================================================================
// Component API Tests
// ============================================================================

TEST(ECSCoordinator_RegisterComponent) {
    ECSCoordinator coordinator;
    coordinator.Init();

    // Transform is already registered in Init()
    // Register additional components
    coordinator.RegisterComponent<TestComponent>();
    coordinator.RegisterComponent<AnotherComponent>();

    // Should not crash or assert
    coordinator.Shutdown();
}

TEST(ECSCoordinator_AddComponent) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<TestComponent>();

    Entity e1 = coordinator.CreateEntity();

    TestComponent comp;
    comp.value = Vec3(1.0f, 2.0f, 3.0f);

    coordinator.AddComponent(e1, comp);

    ASSERT(coordinator.HasComponent<TestComponent>(e1));

    coordinator.Shutdown();
}

TEST(ECSCoordinator_GetComponent) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<TestComponent>();

    Entity e1 = coordinator.CreateEntity();

    TestComponent comp;
    comp.value = Vec3(5.0f, 10.0f, 15.0f);

    coordinator.AddComponent(e1, comp);

    TestComponent& retrieved = coordinator.GetComponent<TestComponent>(e1);

    ASSERT(Vec3Equal(retrieved.value, Vec3(5.0f, 10.0f, 15.0f)));

    coordinator.Shutdown();
}

TEST(ECSCoordinator_GetComponent_Const) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<TestComponent>();

    Entity e1 = coordinator.CreateEntity();

    TestComponent comp;
    comp.value = Vec3(7.0f, 8.0f, 9.0f);

    coordinator.AddComponent(e1, comp);

    const ECSCoordinator& constCoord = coordinator;
    const TestComponent& retrieved = constCoord.GetComponent<TestComponent>(e1);

    ASSERT(Vec3Equal(retrieved.value, Vec3(7.0f, 8.0f, 9.0f)));

    coordinator.Shutdown();
}

TEST(ECSCoordinator_ModifyComponent) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<TestComponent>();

    Entity e1 = coordinator.CreateEntity();

    TestComponent comp;
    comp.value = Vec3(1.0f, 1.0f, 1.0f);

    coordinator.AddComponent(e1, comp);

    // Modify component
    TestComponent& compRef = coordinator.GetComponent<TestComponent>(e1);
    compRef.value = Vec3(10.0f, 20.0f, 30.0f);

    // Verify modification
    TestComponent& retrieved = coordinator.GetComponent<TestComponent>(e1);
    ASSERT(Vec3Equal(retrieved.value, Vec3(10.0f, 20.0f, 30.0f)));

    coordinator.Shutdown();
}

TEST(ECSCoordinator_RemoveComponent) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<TestComponent>();

    Entity e1 = coordinator.CreateEntity();

    TestComponent comp;
    comp.value = Vec3(1.0f, 2.0f, 3.0f);

    coordinator.AddComponent(e1, comp);
    ASSERT(coordinator.HasComponent<TestComponent>(e1));

    coordinator.RemoveComponent<TestComponent>(e1);
    ASSERT(!coordinator.HasComponent<TestComponent>(e1));

    coordinator.Shutdown();
}

TEST(ECSCoordinator_HasComponent) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<TestComponent>();

    Entity e1 = coordinator.CreateEntity();

    ASSERT(!coordinator.HasComponent<TestComponent>(e1));

    TestComponent comp;
    coordinator.AddComponent(e1, comp);

    ASSERT(coordinator.HasComponent<TestComponent>(e1));

    coordinator.Shutdown();
}

TEST(ECSCoordinator_MultipleComponentTypes) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<TestComponent>();
    coordinator.RegisterComponent<AnotherComponent>();

    Entity e1 = coordinator.CreateEntity();

    TestComponent comp1;
    comp1.value = Vec3(1.0f, 2.0f, 3.0f);

    AnotherComponent comp2;
    comp2.health = 100.0f;
    comp2.maxHealth = 100.0f;

    coordinator.AddComponent(e1, comp1);
    coordinator.AddComponent(e1, comp2);

    ASSERT(coordinator.HasComponent<TestComponent>(e1));
    ASSERT(coordinator.HasComponent<AnotherComponent>(e1));

    TestComponent& retrieved1 = coordinator.GetComponent<TestComponent>(e1);
    AnotherComponent& retrieved2 = coordinator.GetComponent<AnotherComponent>(e1);

    ASSERT(Vec3Equal(retrieved1.value, Vec3(1.0f, 2.0f, 3.0f)));
    ASSERT(FloatEqual(retrieved2.health, 100.0f));

    coordinator.Shutdown();
}

// ============================================================================
// Entity Destruction with Components Tests
// ============================================================================

TEST(ECSCoordinator_DestroyEntity_RemovesComponents) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<TestComponent>();

    Entity e1 = coordinator.CreateEntity();

    TestComponent comp;
    comp.value = Vec3(1.0f, 2.0f, 3.0f);
    coordinator.AddComponent(e1, comp);

    ASSERT(coordinator.HasComponent<TestComponent>(e1));

    coordinator.DestroyEntity(e1);

    // Entity is no longer alive, so we can't query components
    ASSERT(!coordinator.IsEntityAlive(e1));

    coordinator.Shutdown();
}

TEST(ECSCoordinator_DestroyEntity_MultipleComponents) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<TestComponent>();
    coordinator.RegisterComponent<AnotherComponent>();

    Entity e1 = coordinator.CreateEntity();

    TestComponent comp1;
    comp1.value = Vec3(1.0f, 2.0f, 3.0f);

    AnotherComponent comp2;
    comp2.health = 50.0f;
    comp2.maxHealth = 100.0f;

    coordinator.AddComponent(e1, comp1);
    coordinator.AddComponent(e1, comp2);

    ASSERT(coordinator.HasComponent<TestComponent>(e1));
    ASSERT(coordinator.HasComponent<AnotherComponent>(e1));

    coordinator.DestroyEntity(e1);

    ASSERT(!coordinator.IsEntityAlive(e1));

    coordinator.Shutdown();
}

// ============================================================================
// Transform Component Integration Tests
// ============================================================================

TEST(ECSCoordinator_TransformComponent) {
    ECSCoordinator coordinator;
    coordinator.Init();

    Entity e1 = coordinator.CreateEntity();

    Transform transform;
    transform.localPosition = Vec3(10.0f, 20.0f, 30.0f);
    transform.isDirty = true;

    coordinator.AddComponent(e1, transform);

    ASSERT(coordinator.HasComponent<Transform>(e1));

    Transform& retrieved = coordinator.GetComponent<Transform>(e1);
    ASSERT(Vec3Equal(retrieved.localPosition, Vec3(10.0f, 20.0f, 30.0f)));

    coordinator.Shutdown();
}

TEST(ECSCoordinator_TransformSystem_Update) {
    ECSCoordinator coordinator;
    coordinator.Init();

    Entity e1 = coordinator.CreateEntity();

    Transform transform;
    transform.localPosition = Vec3(5.0f, 10.0f, 15.0f);
    transform.isDirty = true;

    coordinator.AddComponent(e1, transform);

    // Call update
    coordinator.Update(0.016f);

    // Verify transform system updated the world matrix
    Transform& updated = coordinator.GetComponent<Transform>(e1);
    Mat4 expected = Translate(Mat4(1.0f), Vec3(5.0f, 10.0f, 15.0f));

    ASSERT(Mat4Equal(updated.worldMatrix, expected));
    ASSERT(updated.isDirty == false);

    coordinator.Shutdown();
}

TEST(ECSCoordinator_TransformSystem_MultipleEntities) {
    ECSCoordinator coordinator;
    coordinator.Init();

    Entity e1 = coordinator.CreateEntity();
    Entity e2 = coordinator.CreateEntity();
    Entity e3 = coordinator.CreateEntity();

    Transform t1;
    t1.localPosition = Vec3(10.0f, 0.0f, 0.0f);
    t1.isDirty = true;

    Transform t2;
    t2.localPosition = Vec3(0.0f, 20.0f, 0.0f);
    t2.isDirty = true;

    Transform t3;
    t3.localPosition = Vec3(0.0f, 0.0f, 30.0f);
    t3.isDirty = true;

    coordinator.AddComponent(e1, t1);
    coordinator.AddComponent(e2, t2);
    coordinator.AddComponent(e3, t3);

    // Update all
    coordinator.Update(0.016f);

    Transform& updated1 = coordinator.GetComponent<Transform>(e1);
    Transform& updated2 = coordinator.GetComponent<Transform>(e2);
    Transform& updated3 = coordinator.GetComponent<Transform>(e3);

    Mat4 expected1 = Translate(Mat4(1.0f), Vec3(10.0f, 0.0f, 0.0f));
    Mat4 expected2 = Translate(Mat4(1.0f), Vec3(0.0f, 20.0f, 0.0f));
    Mat4 expected3 = Translate(Mat4(1.0f), Vec3(0.0f, 0.0f, 30.0f));

    ASSERT(Mat4Equal(updated1.worldMatrix, expected1));
    ASSERT(Mat4Equal(updated2.worldMatrix, expected2));
    ASSERT(Mat4Equal(updated3.worldMatrix, expected3));

    ASSERT(updated1.isDirty == false);
    ASSERT(updated2.isDirty == false);
    ASSERT(updated3.isDirty == false);

    coordinator.Shutdown();
}

TEST(ECSCoordinator_TransformSystem_DirtyFlagOptimization) {
    ECSCoordinator coordinator;
    coordinator.Init();

    Entity e1 = coordinator.CreateEntity();

    Transform transform;
    transform.localPosition = Vec3(5.0f, 5.0f, 5.0f);
    transform.isDirty = true;

    coordinator.AddComponent(e1, transform);

    // First update
    coordinator.Update(0.016f);

    Transform& updated = coordinator.GetComponent<Transform>(e1);
    ASSERT(updated.isDirty == false);

    Mat4 originalWorld = updated.worldMatrix;

    // Modify without marking dirty
    updated.localPosition = Vec3(100.0f, 100.0f, 100.0f);

    // Second update - should NOT recompute
    coordinator.Update(0.016f);

    // World matrix should be unchanged
    ASSERT(Mat4Equal(updated.worldMatrix, originalWorld));

    coordinator.Shutdown();
}

TEST(ECSCoordinator_TransformSystem_DirtyFlagUpdate) {
    ECSCoordinator coordinator;
    coordinator.Init();

    Entity e1 = coordinator.CreateEntity();

    Transform transform;
    transform.localPosition = Vec3(5.0f, 5.0f, 5.0f);
    transform.isDirty = true;

    coordinator.AddComponent(e1, transform);

    // First update
    coordinator.Update(0.016f);

    Transform& updated = coordinator.GetComponent<Transform>(e1);
    ASSERT(updated.isDirty == false);

    // Modify and mark dirty
    updated.localPosition = Vec3(100.0f, 100.0f, 100.0f);
    updated.MarkDirty();
    ASSERT(updated.isDirty == true);

    // Second update - should recompute
    coordinator.Update(0.016f);

    Mat4 expected = Translate(Mat4(1.0f), Vec3(100.0f, 100.0f, 100.0f));
    ASSERT(Mat4Equal(updated.worldMatrix, expected));
    ASSERT(updated.isDirty == false);

    coordinator.Shutdown();
}

// ============================================================================
// Full Integration Tests
// ============================================================================

TEST(ECSCoordinator_FullLifecycle_SingleEntity) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<TestComponent>();

    // Create entity
    Entity e1 = coordinator.CreateEntity();
    ASSERT(coordinator.IsEntityAlive(e1));

    // Add components
    Transform transform;
    transform.localPosition = Vec3(10.0f, 20.0f, 30.0f);
    transform.isDirty = true;

    TestComponent comp;
    comp.value = Vec3(1.0f, 2.0f, 3.0f);

    coordinator.AddComponent(e1, transform);
    coordinator.AddComponent(e1, comp);

    ASSERT(coordinator.HasComponent<Transform>(e1));
    ASSERT(coordinator.HasComponent<TestComponent>(e1));

    // Update systems
    coordinator.Update(0.016f);

    // Verify transform updated
    Transform& updatedTransform = coordinator.GetComponent<Transform>(e1);
    ASSERT(updatedTransform.isDirty == false);

    // Destroy entity
    coordinator.DestroyEntity(e1);
    ASSERT(!coordinator.IsEntityAlive(e1));

    coordinator.Shutdown();
}

TEST(ECSCoordinator_FullLifecycle_MultipleEntities) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<TestComponent>();

    // Create entities
    Entity e1 = coordinator.CreateEntity();
    Entity e2 = coordinator.CreateEntity();
    Entity e3 = coordinator.CreateEntity();

    // Add transforms to all
    Transform t1;
    t1.localPosition = Vec3(10.0f, 0.0f, 0.0f);
    t1.isDirty = true;

    Transform t2;
    t2.localPosition = Vec3(0.0f, 20.0f, 0.0f);
    t2.isDirty = true;

    Transform t3;
    t3.localPosition = Vec3(0.0f, 0.0f, 30.0f);
    t3.isDirty = true;

    coordinator.AddComponent(e1, t1);
    coordinator.AddComponent(e2, t2);
    coordinator.AddComponent(e3, t3);

    // Add test component to e1 and e3 only
    TestComponent comp1;
    comp1.value = Vec3(1.0f, 1.0f, 1.0f);

    TestComponent comp3;
    comp3.value = Vec3(3.0f, 3.0f, 3.0f);

    coordinator.AddComponent(e1, comp1);
    coordinator.AddComponent(e3, comp3);

    ASSERT(coordinator.HasComponent<TestComponent>(e1));
    ASSERT(!coordinator.HasComponent<TestComponent>(e2));
    ASSERT(coordinator.HasComponent<TestComponent>(e3));

    // Update all systems
    coordinator.Update(0.016f);

    // Verify all transforms updated
    ASSERT(coordinator.GetComponent<Transform>(e1).isDirty == false);
    ASSERT(coordinator.GetComponent<Transform>(e2).isDirty == false);
    ASSERT(coordinator.GetComponent<Transform>(e3).isDirty == false);

    // Destroy e2
    coordinator.DestroyEntity(e2);

    ASSERT(coordinator.IsEntityAlive(e1));
    ASSERT(!coordinator.IsEntityAlive(e2));
    ASSERT(coordinator.IsEntityAlive(e3));

    coordinator.Shutdown();
}

TEST(ECSCoordinator_LargeScale) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<TestComponent>();

    const int numEntities = 100;

    // Create many entities with transforms
    for (int i = 0; i < numEntities; i++) {
        Entity e = coordinator.CreateEntity();

        Transform t;
        t.localPosition = Vec3(static_cast<f32>(i), 0.0f, 0.0f);
        t.isDirty = true;

        coordinator.AddComponent(e, t);

        if (i % 2 == 0) {
            TestComponent comp;
            comp.value = Vec3(static_cast<f32>(i), static_cast<f32>(i), static_cast<f32>(i));
            coordinator.AddComponent(e, comp);
        }
    }

    ASSERT(coordinator.GetEntityCount() == numEntities);

    // Update all
    coordinator.Update(0.016f);

    coordinator.Shutdown();
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    std::cout << "=== ECS Coordinator Unit Tests ===" << std::endl;
    std::cout << std::endl;

    std::cout << "--- Initialization Tests ---" << std::endl;
    ECSCoordinator_InitShutdown_runner();
    ECSCoordinator_MultipleInitShutdown_runner();

    std::cout << std::endl;
    std::cout << "--- Entity API Tests ---" << std::endl;
    ECSCoordinator_CreateEntity_runner();
    ECSCoordinator_CreateMultipleEntities_runner();
    ECSCoordinator_DestroyEntity_runner();
    ECSCoordinator_DestroyMultipleEntities_runner();

    std::cout << std::endl;
    std::cout << "--- Component API Tests ---" << std::endl;
    ECSCoordinator_RegisterComponent_runner();
    ECSCoordinator_AddComponent_runner();
    ECSCoordinator_GetComponent_runner();
    ECSCoordinator_GetComponent_Const_runner();
    ECSCoordinator_ModifyComponent_runner();
    ECSCoordinator_RemoveComponent_runner();
    ECSCoordinator_HasComponent_runner();
    ECSCoordinator_MultipleComponentTypes_runner();

    std::cout << std::endl;
    std::cout << "--- Entity Destruction Tests ---" << std::endl;
    ECSCoordinator_DestroyEntity_RemovesComponents_runner();
    ECSCoordinator_DestroyEntity_MultipleComponents_runner();

    std::cout << std::endl;
    std::cout << "--- Transform System Integration Tests ---" << std::endl;
    ECSCoordinator_TransformComponent_runner();
    ECSCoordinator_TransformSystem_Update_runner();
    ECSCoordinator_TransformSystem_MultipleEntities_runner();
    ECSCoordinator_TransformSystem_DirtyFlagOptimization_runner();
    ECSCoordinator_TransformSystem_DirtyFlagUpdate_runner();

    std::cout << std::endl;
    std::cout << "--- Full Integration Tests ---" << std::endl;
    ECSCoordinator_FullLifecycle_SingleEntity_runner();
    ECSCoordinator_FullLifecycle_MultipleEntities_runner();
    ECSCoordinator_LargeScale_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
