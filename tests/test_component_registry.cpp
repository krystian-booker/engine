#include "ecs/component_registry.h"
#include "ecs/entity_manager.h"
#include "core/math.h"
#include <iostream>

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

// Test component types
struct Position {
    Vec3 value;
};

struct Velocity {
    Vec3 value;
};

struct Health {
    f32 current;
    f32 max;
};

struct Transform {
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
};

// ============================================================================
// ComponentRegistry Basic Tests
// ============================================================================

TEST(ComponentRegistry_RegisterSingleComponent) {
    ComponentRegistry registry;

    // Should not throw or assert in release
    registry.RegisterComponent<Position>();

    // Should be able to get the array
    auto array = registry.GetComponentArray<Position>();
    ASSERT(array != nullptr);
}

TEST(ComponentRegistry_RegisterMultipleComponents) {
    ComponentRegistry registry;

    registry.RegisterComponent<Position>();
    registry.RegisterComponent<Velocity>();
    registry.RegisterComponent<Health>();

    auto posArray = registry.GetComponentArray<Position>();
    auto velArray = registry.GetComponentArray<Velocity>();
    auto hpArray = registry.GetComponentArray<Health>();

    ASSERT(posArray != nullptr);
    ASSERT(velArray != nullptr);
    ASSERT(hpArray != nullptr);

    // Arrays should be different (check addresses)
    ASSERT(static_cast<void*>(posArray.get()) != static_cast<void*>(velArray.get()));
}

TEST(ComponentRegistry_GetRegisteredArray) {
    ComponentRegistry registry;
    registry.RegisterComponent<Position>();

    auto array = registry.GetComponentArray<Position>();
    ASSERT(array != nullptr);
    ASSERT(array->Size() == 0);  // Should be empty initially
}

TEST(ComponentRegistry_ArrayFunctionality) {
    ComponentRegistry registry;
    registry.RegisterComponent<Position>();

    auto array = registry.GetComponentArray<Position>();
    Entity e1 = {0, 0};

    // Test basic add/get through registry
    array->Add(e1, Position{{1.0f, 2.0f, 3.0f}});

    ASSERT(array->Has(e1));
    ASSERT(array->Size() == 1);
    ASSERT(array->Get(e1).value.x == 1.0f);
}

// ============================================================================
// ComponentRegistry Entity Destruction Tests
// ============================================================================

TEST(ComponentRegistry_OnEntityDestroyed_SingleComponent) {
    ComponentRegistry registry;
    registry.RegisterComponent<Position>();

    auto posArray = registry.GetComponentArray<Position>();
    Entity e1 = {0, 0};

    posArray->Add(e1, Position{{1.0f, 2.0f, 3.0f}});
    ASSERT(posArray->Has(e1));

    // Destroy entity
    registry.OnEntityDestroyed(e1);

    ASSERT(!posArray->Has(e1));
    ASSERT(posArray->Size() == 0);
}

TEST(ComponentRegistry_OnEntityDestroyed_MultipleComponents) {
    ComponentRegistry registry;
    registry.RegisterComponent<Position>();
    registry.RegisterComponent<Velocity>();
    registry.RegisterComponent<Health>();

    auto posArray = registry.GetComponentArray<Position>();
    auto velArray = registry.GetComponentArray<Velocity>();
    auto hpArray = registry.GetComponentArray<Health>();

    Entity e1 = {0, 0};

    // Add entity to all arrays
    posArray->Add(e1, Position{{1.0f, 2.0f, 3.0f}});
    velArray->Add(e1, Velocity{{0.5f, 0.5f, 0.5f}});
    hpArray->Add(e1, Health{100.0f, 100.0f});

    ASSERT(posArray->Has(e1));
    ASSERT(velArray->Has(e1));
    ASSERT(hpArray->Has(e1));

    // Destroy entity - should remove from all arrays
    registry.OnEntityDestroyed(e1);

    ASSERT(!posArray->Has(e1));
    ASSERT(!velArray->Has(e1));
    ASSERT(!hpArray->Has(e1));
    ASSERT(posArray->Size() == 0);
    ASSERT(velArray->Size() == 0);
    ASSERT(hpArray->Size() == 0);
}

TEST(ComponentRegistry_OnEntityDestroyed_PartialComponents) {
    ComponentRegistry registry;
    registry.RegisterComponent<Position>();
    registry.RegisterComponent<Velocity>();

    auto posArray = registry.GetComponentArray<Position>();
    auto velArray = registry.GetComponentArray<Velocity>();

    Entity e1 = {0, 0};

    // Entity only has Position, not Velocity
    posArray->Add(e1, Position{{1.0f, 2.0f, 3.0f}});

    ASSERT(posArray->Has(e1));
    ASSERT(!velArray->Has(e1));

    // Should safely handle entity not having all components
    registry.OnEntityDestroyed(e1);

    ASSERT(!posArray->Has(e1));
    ASSERT(!velArray->Has(e1));
}

TEST(ComponentRegistry_OnEntityDestroyed_MultipleEntities) {
    ComponentRegistry registry;
    registry.RegisterComponent<Position>();
    registry.RegisterComponent<Velocity>();

    auto posArray = registry.GetComponentArray<Position>();
    auto velArray = registry.GetComponentArray<Velocity>();

    Entity e1 = {0, 0};
    Entity e2 = {1, 0};
    Entity e3 = {2, 0};

    // Add multiple entities
    posArray->Add(e1, Position{{1.0f, 0.0f, 0.0f}});
    posArray->Add(e2, Position{{2.0f, 0.0f, 0.0f}});
    posArray->Add(e3, Position{{3.0f, 0.0f, 0.0f}});

    velArray->Add(e1, Velocity{{0.1f, 0.0f, 0.0f}});
    velArray->Add(e3, Velocity{{0.3f, 0.0f, 0.0f}});

    ASSERT(posArray->Size() == 3);
    ASSERT(velArray->Size() == 2);

    // Destroy e2 (only has position)
    registry.OnEntityDestroyed(e2);

    ASSERT(!posArray->Has(e2));
    ASSERT(posArray->Has(e1));
    ASSERT(posArray->Has(e3));
    ASSERT(posArray->Size() == 2);
    ASSERT(velArray->Size() == 2);

    // Destroy e1 (has both)
    registry.OnEntityDestroyed(e1);

    ASSERT(!posArray->Has(e1));
    ASSERT(!velArray->Has(e1));
    ASSERT(posArray->Size() == 1);
    ASSERT(velArray->Size() == 1);
}

TEST(ComponentRegistry_OnEntityDestroyed_NoComponents) {
    ComponentRegistry registry;
    registry.RegisterComponent<Position>();

    Entity e1 = {0, 0};

    // Should safely handle entity with no components
    registry.OnEntityDestroyed(e1);

    // Should not crash
    auto posArray = registry.GetComponentArray<Position>();
    ASSERT(!posArray->Has(e1));
}

// ============================================================================
// ComponentRegistry Integration Tests
// ============================================================================

TEST(ComponentRegistry_WithEntityManager) {
    ComponentRegistry registry;
    EntityManager em;

    registry.RegisterComponent<Position>();
    registry.RegisterComponent<Velocity>();

    auto posArray = registry.GetComponentArray<Position>();
    auto velArray = registry.GetComponentArray<Velocity>();

    // Create entities
    Entity e1 = em.CreateEntity();
    Entity e2 = em.CreateEntity();

    posArray->Add(e1, Position{{1.0f, 2.0f, 3.0f}});
    velArray->Add(e1, Velocity{{0.1f, 0.2f, 0.3f}});

    posArray->Add(e2, Position{{4.0f, 5.0f, 6.0f}});

    ASSERT(em.IsAlive(e1));
    ASSERT(em.IsAlive(e2));

    // Destroy entity and cleanup components
    em.DestroyEntity(e1);
    registry.OnEntityDestroyed(e1);

    ASSERT(!em.IsAlive(e1));
    ASSERT(!posArray->Has(e1));
    ASSERT(!velArray->Has(e1));
    ASSERT(posArray->Has(e2));
}

TEST(ComponentRegistry_TypeSafety) {
    ComponentRegistry registry;

    registry.RegisterComponent<Position>();
    registry.RegisterComponent<Velocity>();

    auto posArray1 = registry.GetComponentArray<Position>();
    auto posArray2 = registry.GetComponentArray<Position>();
    auto velArray = registry.GetComponentArray<Velocity>();

    // Same type should return same array (check addresses)
    ASSERT(posArray1.get() == posArray2.get());

    // Different types should return different arrays
    ASSERT(static_cast<void*>(posArray1.get()) != static_cast<void*>(velArray.get()));
}

TEST(ComponentRegistry_LargeScale) {
    ComponentRegistry registry;

    registry.RegisterComponent<Position>();
    registry.RegisterComponent<Velocity>();
    registry.RegisterComponent<Health>();

    auto posArray = registry.GetComponentArray<Position>();
    auto velArray = registry.GetComponentArray<Velocity>();
    auto hpArray = registry.GetComponentArray<Health>();

    const int numEntities = 1000;

    // Add components to many entities
    for (int i = 0; i < numEntities; i++) {
        Entity e = {static_cast<u32>(i), 0};
        posArray->Add(e, Position{{static_cast<f32>(i), 0.0f, 0.0f}});

        if (i % 2 == 0) {
            velArray->Add(e, Velocity{{0.1f, 0.0f, 0.0f}});
        }

        if (i % 3 == 0) {
            hpArray->Add(e, Health{100.0f, 100.0f});
        }
    }

    ASSERT(posArray->Size() == numEntities);
    ASSERT(velArray->Size() == numEntities / 2);
    // 0, 3, 6, 9, ... 999 = 334 entities (not 333)
    ASSERT(hpArray->Size() == 334);

    // Destroy entities
    for (int i = 0; i < numEntities / 2; i++) {
        Entity e = {static_cast<u32>(i), 0};
        registry.OnEntityDestroyed(e);
    }

    ASSERT(posArray->Size() == numEntities / 2);
}

TEST(ComponentRegistry_MultipleComponentTypes) {
    ComponentRegistry registry;

    registry.RegisterComponent<Position>();
    registry.RegisterComponent<Velocity>();
    registry.RegisterComponent<Health>();
    registry.RegisterComponent<Transform>();

    auto posArray = registry.GetComponentArray<Position>();
    auto velArray = registry.GetComponentArray<Velocity>();
    auto hpArray = registry.GetComponentArray<Health>();
    auto transArray = registry.GetComponentArray<Transform>();

    ASSERT(posArray != nullptr);
    ASSERT(velArray != nullptr);
    ASSERT(hpArray != nullptr);
    ASSERT(transArray != nullptr);

    // All arrays should be different (check addresses)
    ASSERT(static_cast<void*>(posArray.get()) != static_cast<void*>(velArray.get()));
    ASSERT(static_cast<void*>(posArray.get()) != static_cast<void*>(hpArray.get()));
    ASSERT(static_cast<void*>(posArray.get()) != static_cast<void*>(transArray.get()));
    ASSERT(static_cast<void*>(velArray.get()) != static_cast<void*>(hpArray.get()));
    ASSERT(static_cast<void*>(velArray.get()) != static_cast<void*>(transArray.get()));
    ASSERT(static_cast<void*>(hpArray.get()) != static_cast<void*>(transArray.get()));
}

TEST(ComponentRegistry_EntityRemovedInterface) {
    ComponentRegistry registry;
    registry.RegisterComponent<Position>();

    auto posArray = registry.GetComponentArray<Position>();
    Entity e1 = {0, 0};

    posArray->Add(e1, Position{{1.0f, 2.0f, 3.0f}});
    ASSERT(posArray->Has(e1));

    // Test IComponentArray interface directly
    IComponentArray* baseArray = posArray.get();
    baseArray->EntityRemoved(e1);

    ASSERT(!posArray->Has(e1));
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    std::cout << "=== ComponentRegistry Unit Tests ===" << std::endl;
    std::cout << std::endl;

    std::cout << "--- Basic Tests ---" << std::endl;
    ComponentRegistry_RegisterSingleComponent_runner();
    ComponentRegistry_RegisterMultipleComponents_runner();
    ComponentRegistry_GetRegisteredArray_runner();
    ComponentRegistry_ArrayFunctionality_runner();

    std::cout << std::endl;
    std::cout << "--- Entity Destruction Tests ---" << std::endl;
    ComponentRegistry_OnEntityDestroyed_SingleComponent_runner();
    ComponentRegistry_OnEntityDestroyed_MultipleComponents_runner();
    ComponentRegistry_OnEntityDestroyed_PartialComponents_runner();
    ComponentRegistry_OnEntityDestroyed_MultipleEntities_runner();
    ComponentRegistry_OnEntityDestroyed_NoComponents_runner();

    std::cout << std::endl;
    std::cout << "--- Integration Tests ---" << std::endl;
    ComponentRegistry_WithEntityManager_runner();
    ComponentRegistry_TypeSafety_runner();
    ComponentRegistry_LargeScale_runner();
    ComponentRegistry_MultipleComponentTypes_runner();
    ComponentRegistry_EntityRemovedInterface_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
