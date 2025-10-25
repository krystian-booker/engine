#include "ecs/entity_manager.h"
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

// ============================================================================
// Entity Tests
// ============================================================================

TEST(Entity_InvalidConstant) {
    Entity invalid = Entity::Invalid;
    ASSERT(!invalid.IsValid());
}

TEST(Entity_EqualityOperators) {
    Entity e1 = {0, 0};
    Entity e2 = {0, 0};
    Entity e3 = {1, 0};
    Entity e4 = {0, 1};

    ASSERT(e1 == e2);
    ASSERT(e1 != e3);
    ASSERT(e1 != e4);
    ASSERT(e3 != e4);
}

TEST(Entity_IsValid) {
    Entity valid = {0, 0};
    Entity invalid = Entity::Invalid;

    ASSERT(valid.IsValid());
    ASSERT(!invalid.IsValid());
}

// ============================================================================
// EntityManager Tests
// ============================================================================

TEST(EntityManager_BasicCreation) {
    EntityManager em;

    Entity e1 = em.CreateEntity();
    ASSERT(e1.IsValid());
    ASSERT(e1.index == 0);
    ASSERT(e1.generation == 0);
    ASSERT(em.IsAlive(e1));
    ASSERT(em.GetEntityCount() == 1);
    ASSERT(em.GetCapacity() == 1);
}

TEST(EntityManager_MultipleCreation) {
    EntityManager em;

    Entity e1 = em.CreateEntity();
    Entity e2 = em.CreateEntity();
    Entity e3 = em.CreateEntity();

    ASSERT(e1.index == 0);
    ASSERT(e2.index == 1);
    ASSERT(e3.index == 2);

    ASSERT(e1.generation == 0);
    ASSERT(e2.generation == 0);
    ASSERT(e3.generation == 0);

    ASSERT(em.IsAlive(e1));
    ASSERT(em.IsAlive(e2));
    ASSERT(em.IsAlive(e3));

    ASSERT(em.GetEntityCount() == 3);
    ASSERT(em.GetCapacity() == 3);
}

TEST(EntityManager_DestroyEntity) {
    EntityManager em;

    Entity e1 = em.CreateEntity();
    Entity e2 = em.CreateEntity();

    ASSERT(em.GetEntityCount() == 2);

    em.DestroyEntity(e1);

    ASSERT(!em.IsAlive(e1));
    ASSERT(em.IsAlive(e2));
    ASSERT(em.GetEntityCount() == 1);
}

TEST(EntityManager_GenerationIncrement) {
    EntityManager em;

    Entity e1 = em.CreateEntity();  // index=0, gen=0
    ASSERT(e1.index == 0);
    ASSERT(e1.generation == 0);

    em.DestroyEntity(e1);
    ASSERT(!em.IsAlive(e1));

    // Reuse the slot - should have incremented generation
    Entity e2 = em.CreateEntity();  // index=0, gen=1
    ASSERT(e2.index == 0);
    ASSERT(e2.generation == 1);
    ASSERT(em.IsAlive(e2));
    ASSERT(!em.IsAlive(e1));  // Old handle is invalid
}

TEST(EntityManager_FreeListRecycling) {
    EntityManager em;

    Entity e1 = em.CreateEntity();  // index=0, gen=0
    Entity e2 = em.CreateEntity();  // index=1, gen=0

    em.DestroyEntity(e1);

    Entity e3 = em.CreateEntity();  // index=0, gen=1 (reused slot!)

    ASSERT(e3.index == 0);
    ASSERT(e3.generation == 1);
    ASSERT(em.IsAlive(e2));
    ASSERT(!em.IsAlive(e1));  // Old handle is invalid
    ASSERT(em.IsAlive(e3));
    ASSERT(em.GetEntityCount() == 2);
    ASSERT(em.GetCapacity() == 2);  // Didn't grow
}

TEST(EntityManager_MultipleDestroyAndRecycle) {
    EntityManager em;

    // Create 5 entities
    Entity entities[5];
    for (int i = 0; i < 5; i++) {
        entities[i] = em.CreateEntity();
        ASSERT(entities[i].index == static_cast<u32>(i));
        ASSERT(entities[i].generation == 0);
    }

    ASSERT(em.GetEntityCount() == 5);

    // Destroy entities 1 and 3
    em.DestroyEntity(entities[1]);
    em.DestroyEntity(entities[3]);

    ASSERT(em.GetEntityCount() == 3);
    ASSERT(!em.IsAlive(entities[1]));
    ASSERT(!em.IsAlive(entities[3]));

    // Create two new entities - should reuse slots 3 and 1 (LIFO order from queue)
    Entity new1 = em.CreateEntity();
    Entity new2 = em.CreateEntity();

    ASSERT(em.GetEntityCount() == 5);
    ASSERT(em.IsAlive(new1));
    ASSERT(em.IsAlive(new2));

    // Verify old handles still invalid
    ASSERT(!em.IsAlive(entities[1]));
    ASSERT(!em.IsAlive(entities[3]));

    // Other entities still alive
    ASSERT(em.IsAlive(entities[0]));
    ASSERT(em.IsAlive(entities[2]));
    ASSERT(em.IsAlive(entities[4]));
}

TEST(EntityManager_StaleHandlePrevention) {
    EntityManager em;

    Entity e1 = em.CreateEntity();
    Entity staleHandle = e1;  // Save the handle

    em.DestroyEntity(e1);
    ASSERT(!em.IsAlive(staleHandle));

    Entity e2 = em.CreateEntity();  // Reuses same slot
    ASSERT(e2.index == staleHandle.index);
    ASSERT(e2.generation != staleHandle.generation);

    // Stale handle should NOT be considered alive
    ASSERT(!em.IsAlive(staleHandle));
    ASSERT(em.IsAlive(e2));
}

TEST(EntityManager_LargeAllocation) {
    EntityManager em;

    const int numEntities = 1000;
    Entity entities[1000];

    // Create many entities
    for (int i = 0; i < numEntities; i++) {
        entities[i] = em.CreateEntity();
        ASSERT(entities[i].IsValid());
    }

    ASSERT(em.GetEntityCount() == numEntities);
    ASSERT(em.GetCapacity() == numEntities);

    // Verify all alive
    for (int i = 0; i < numEntities; i++) {
        ASSERT(em.IsAlive(entities[i]));
    }

    // Destroy half
    for (int i = 0; i < numEntities / 2; i++) {
        em.DestroyEntity(entities[i * 2]);
    }

    ASSERT(em.GetEntityCount() == numEntities / 2);

    // Verify correct ones are alive
    for (int i = 0; i < numEntities / 2; i++) {
        ASSERT(!em.IsAlive(entities[i * 2]));
        ASSERT(em.IsAlive(entities[i * 2 + 1]));
    }
}

TEST(EntityManager_RepeatedCycling) {
    EntityManager em;

    Entity handle = em.CreateEntity();
    u32 originalIndex = handle.index;

    // Cycle through multiple generations on the same slot
    for (int i = 0; i < 10; i++) {
        Entity currentHandle = handle;
        ASSERT(currentHandle.generation == static_cast<u32>(i));

        em.DestroyEntity(currentHandle);
        ASSERT(!em.IsAlive(currentHandle));

        handle = em.CreateEntity();
        ASSERT(handle.index == originalIndex);
        ASSERT(handle.generation == static_cast<u32>(i + 1));
    }
}

TEST(EntityManager_InvalidEntityNotAlive) {
    EntityManager em;

    Entity invalid = Entity::Invalid;
    ASSERT(!em.IsAlive(invalid));
}

TEST(EntityManager_OutOfBoundsNotAlive) {
    EntityManager em;

    em.CreateEntity();  // Create one entity

    Entity outOfBounds = {100, 0};  // Index way out of range
    ASSERT(!em.IsAlive(outOfBounds));
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    std::cout << "=== Entity System Unit Tests ===" << std::endl;
    std::cout << std::endl;

    std::cout << "--- Entity Tests ---" << std::endl;
    Entity_InvalidConstant_runner();
    Entity_EqualityOperators_runner();
    Entity_IsValid_runner();

    std::cout << std::endl;
    std::cout << "--- EntityManager Tests ---" << std::endl;
    EntityManager_BasicCreation_runner();
    EntityManager_MultipleCreation_runner();
    EntityManager_DestroyEntity_runner();
    EntityManager_GenerationIncrement_runner();
    EntityManager_FreeListRecycling_runner();
    EntityManager_MultipleDestroyAndRecycle_runner();
    EntityManager_StaleHandlePrevention_runner();
    EntityManager_LargeAllocation_runner();
    EntityManager_RepeatedCycling_runner();
    EntityManager_InvalidEntityNotAlive_runner();
    EntityManager_OutOfBoundsNotAlive_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
