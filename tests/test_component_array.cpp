#include "ecs/component_array.h"
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

// ============================================================================
// ComponentArray Basic Tests
// ============================================================================

TEST(ComponentArray_BasicAddAndGet) {
    ComponentArray<Position> positions;
    Entity e1 = {0, 0};

    positions.Add(e1, Position{{1.0f, 2.0f, 3.0f}});

    ASSERT(positions.Has(e1));
    ASSERT(positions.Size() == 1);

    Position& pos = positions.Get(e1);
    ASSERT(pos.value.x == 1.0f);
    ASSERT(pos.value.y == 2.0f);
    ASSERT(pos.value.z == 3.0f);
}

TEST(ComponentArray_MultipleEntities) {
    ComponentArray<Position> positions;
    Entity e1 = {0, 0};
    Entity e2 = {1, 0};
    Entity e3 = {2, 0};

    positions.Add(e1, Position{{1.0f, 0.0f, 0.0f}});
    positions.Add(e2, Position{{0.0f, 2.0f, 0.0f}});
    positions.Add(e3, Position{{0.0f, 0.0f, 3.0f}});

    ASSERT(positions.Size() == 3);
    ASSERT(positions.Has(e1));
    ASSERT(positions.Has(e2));
    ASSERT(positions.Has(e3));

    ASSERT(positions.Get(e1).value.x == 1.0f);
    ASSERT(positions.Get(e2).value.y == 2.0f);
    ASSERT(positions.Get(e3).value.z == 3.0f);
}

TEST(ComponentArray_ModifyComponent) {
    ComponentArray<Position> positions;
    Entity e1 = {0, 0};

    positions.Add(e1, Position{{1.0f, 2.0f, 3.0f}});

    Position& pos = positions.Get(e1);
    pos.value.x += 10.0f;

    ASSERT(positions.Get(e1).value.x == 11.0f);
    ASSERT(positions.Get(e1).value.y == 2.0f);
    ASSERT(positions.Get(e1).value.z == 3.0f);
}

TEST(ComponentArray_HasComponent) {
    ComponentArray<Position> positions;
    Entity e1 = {0, 0};
    Entity e2 = {1, 0};

    positions.Add(e1, Position{{1.0f, 2.0f, 3.0f}});

    ASSERT(positions.Has(e1));
    ASSERT(!positions.Has(e2));
}

// ============================================================================
// ComponentArray Removal Tests
// ============================================================================

TEST(ComponentArray_BasicRemove) {
    ComponentArray<Position> positions;
    Entity e1 = {0, 0};

    positions.Add(e1, Position{{1.0f, 2.0f, 3.0f}});
    ASSERT(positions.Has(e1));
    ASSERT(positions.Size() == 1);

    positions.Remove(e1);
    ASSERT(!positions.Has(e1));
    ASSERT(positions.Size() == 0);
}

TEST(ComponentArray_RemoveMiddleElement) {
    ComponentArray<Position> positions;
    Entity e1 = {0, 0};
    Entity e2 = {1, 0};
    Entity e3 = {2, 0};

    positions.Add(e1, Position{{1.0f, 0.0f, 0.0f}});
    positions.Add(e2, Position{{2.0f, 0.0f, 0.0f}});
    positions.Add(e3, Position{{3.0f, 0.0f, 0.0f}});

    ASSERT(positions.Size() == 3);

    // Remove middle element
    positions.Remove(e2);

    ASSERT(positions.Size() == 2);
    ASSERT(positions.Has(e1));
    ASSERT(!positions.Has(e2));
    ASSERT(positions.Has(e3));

    // Verify remaining components still correct
    ASSERT(positions.Get(e1).value.x == 1.0f);
    ASSERT(positions.Get(e3).value.x == 3.0f);
}

TEST(ComponentArray_SwapAndPopCorrectness) {
    ComponentArray<Position> positions;
    Entity e1 = {0, 0};
    Entity e2 = {1, 0};
    Entity e3 = {2, 0};

    positions.Add(e1, Position{{1.0f, 0.0f, 0.0f}});
    positions.Add(e2, Position{{2.0f, 0.0f, 0.0f}});
    positions.Add(e3, Position{{3.0f, 0.0f, 0.0f}});

    // Remove first element - should swap with last (e3)
    positions.Remove(e1);

    ASSERT(positions.Size() == 2);
    ASSERT(!positions.Has(e1));
    ASSERT(positions.Has(e2));
    ASSERT(positions.Has(e3));

    // Verify e3's component is still accessible and correct
    ASSERT(positions.Get(e3).value.x == 3.0f);
    ASSERT(positions.Get(e2).value.x == 2.0f);
}

TEST(ComponentArray_RemoveAll) {
    ComponentArray<Position> positions;
    Entity entities[5] = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}};

    for (int i = 0; i < 5; i++) {
        positions.Add(entities[i], Position{{static_cast<f32>(i), 0.0f, 0.0f}});
    }

    ASSERT(positions.Size() == 5);

    // Remove all
    for (int i = 0; i < 5; i++) {
        positions.Remove(entities[i]);
    }

    ASSERT(positions.Size() == 0);
    for (int i = 0; i < 5; i++) {
        ASSERT(!positions.Has(entities[i]));
    }
}

// ============================================================================
// ComponentArray Iteration Tests
// ============================================================================

TEST(ComponentArray_DenseIteration) {
    ComponentArray<Position> positions;
    Entity e1 = {0, 0};
    Entity e2 = {1, 0};
    Entity e3 = {2, 0};

    positions.Add(e1, Position{{1.0f, 0.0f, 0.0f}});
    positions.Add(e2, Position{{2.0f, 0.0f, 0.0f}});
    positions.Add(e3, Position{{3.0f, 0.0f, 0.0f}});

    // Iterate using range-based for
    f32 sum = 0.0f;
    for (const Position& pos : positions) {
        sum += pos.value.x;
    }

    ASSERT(sum == 6.0f);  // 1 + 2 + 3
}

TEST(ComponentArray_DataPointer) {
    ComponentArray<Position> positions;
    Entity e1 = {0, 0};
    Entity e2 = {1, 0};

    positions.Add(e1, Position{{1.0f, 2.0f, 3.0f}});
    positions.Add(e2, Position{{4.0f, 5.0f, 6.0f}});

    const Position* data = positions.Data();
    ASSERT(data != nullptr);
    ASSERT(data[0].value.x == 1.0f);
    ASSERT(data[1].value.x == 4.0f);
}

TEST(ComponentArray_GetEntity) {
    ComponentArray<Position> positions;
    Entity e1 = {0, 0};
    Entity e2 = {5, 2};  // Non-sequential index

    positions.Add(e1, Position{{1.0f, 0.0f, 0.0f}});
    positions.Add(e2, Position{{2.0f, 0.0f, 0.0f}});

    ASSERT(positions.GetEntity(0) == e1);
    ASSERT(positions.GetEntity(1) == e2);
}

TEST(ComponentArray_IterationAfterRemoval) {
    ComponentArray<Position> positions;
    Entity e1 = {0, 0};
    Entity e2 = {1, 0};
    Entity e3 = {2, 0};

    positions.Add(e1, Position{{1.0f, 0.0f, 0.0f}});
    positions.Add(e2, Position{{2.0f, 0.0f, 0.0f}});
    positions.Add(e3, Position{{3.0f, 0.0f, 0.0f}});

    positions.Remove(e2);

    // Should still iterate correctly
    ASSERT(positions.Size() == 2);

    f32 sum = 0.0f;
    for (const Position& pos : positions) {
        sum += pos.value.x;
    }

    ASSERT(sum == 4.0f);  // 1 + 3 (e2 removed)
}

// ============================================================================
// ComponentArray Sparse Array Growth Tests
// ============================================================================

TEST(ComponentArray_SparseArrayGrowth) {
    ComponentArray<Position> positions(10);  // Start with small capacity

    Entity e1 = {100, 0};  // Way beyond initial capacity

    positions.Add(e1, Position{{1.0f, 2.0f, 3.0f}});

    ASSERT(positions.Has(e1));
    ASSERT(positions.Get(e1).value.x == 1.0f);
}

TEST(ComponentArray_NonSequentialEntities) {
    ComponentArray<Position> positions;
    Entity e1 = {5, 0};
    Entity e2 = {100, 0};
    Entity e3 = {3, 0};

    positions.Add(e1, Position{{1.0f, 0.0f, 0.0f}});
    positions.Add(e2, Position{{2.0f, 0.0f, 0.0f}});
    positions.Add(e3, Position{{3.0f, 0.0f, 0.0f}});

    ASSERT(positions.Size() == 3);
    ASSERT(positions.Has(e1));
    ASSERT(positions.Has(e2));
    ASSERT(positions.Has(e3));

    ASSERT(positions.Get(e1).value.x == 1.0f);
    ASSERT(positions.Get(e2).value.x == 2.0f);
    ASSERT(positions.Get(e3).value.x == 3.0f);
}

// ============================================================================
// ComponentArray Integration with EntityManager
// ============================================================================

TEST(ComponentArray_WithEntityManager) {
    EntityManager em;
    ComponentArray<Position> positions;

    Entity e1 = em.CreateEntity();
    Entity e2 = em.CreateEntity();
    Entity e3 = em.CreateEntity();

    positions.Add(e1, Position{{1.0f, 2.0f, 3.0f}});
    positions.Add(e2, Position{{4.0f, 5.0f, 6.0f}});
    positions.Add(e3, Position{{7.0f, 8.0f, 9.0f}});

    ASSERT(positions.Size() == 3);
    ASSERT(em.IsAlive(e1));
    ASSERT(positions.Has(e1));

    // Destroy entity
    em.DestroyEntity(e2);
    positions.Remove(e2);

    ASSERT(!em.IsAlive(e2));
    ASSERT(!positions.Has(e2));
    ASSERT(positions.Size() == 2);
}

TEST(ComponentArray_GenerationHandling) {
    EntityManager em;
    ComponentArray<Position> positions;

    Entity e1 = em.CreateEntity();  // index=0, gen=0
    positions.Add(e1, Position{{1.0f, 2.0f, 3.0f}});

    em.DestroyEntity(e1);
    positions.Remove(e1);

    Entity e2 = em.CreateEntity();  // index=0, gen=1
    ASSERT(e2.index == e1.index);
    ASSERT(e2.generation != e1.generation);

    // Old entity handle should not have component
    ASSERT(!positions.Has(e1));

    // New entity can get component
    positions.Add(e2, Position{{10.0f, 20.0f, 30.0f}});
    ASSERT(positions.Has(e2));
    ASSERT(positions.Get(e2).value.x == 10.0f);
}

// ============================================================================
// ComponentArray Multiple Component Types
// ============================================================================

TEST(ComponentArray_MultipleComponentTypes) {
    ComponentArray<Position> positions;
    ComponentArray<Velocity> velocities;
    ComponentArray<Health> healths;

    Entity e1 = {0, 0};

    positions.Add(e1, Position{{1.0f, 2.0f, 3.0f}});
    velocities.Add(e1, Velocity{{0.5f, 0.5f, 0.5f}});
    healths.Add(e1, Health{100.0f, 100.0f});

    ASSERT(positions.Has(e1));
    ASSERT(velocities.Has(e1));
    ASSERT(healths.Has(e1));

    ASSERT(positions.Get(e1).value.x == 1.0f);
    ASSERT(velocities.Get(e1).value.x == 0.5f);
    ASSERT(healths.Get(e1).current == 100.0f);
}

TEST(ComponentArray_PartialComponents) {
    ComponentArray<Position> positions;
    ComponentArray<Velocity> velocities;

    Entity e1 = {0, 0};
    Entity e2 = {1, 0};

    // e1 has both, e2 only has position
    positions.Add(e1, Position{{1.0f, 0.0f, 0.0f}});
    velocities.Add(e1, Velocity{{0.5f, 0.0f, 0.0f}});

    positions.Add(e2, Position{{2.0f, 0.0f, 0.0f}});

    ASSERT(positions.Has(e1));
    ASSERT(velocities.Has(e1));
    ASSERT(positions.Has(e2));
    ASSERT(!velocities.Has(e2));
}

// ============================================================================
// ComponentArray Large Scale Tests
// ============================================================================

TEST(ComponentArray_LargeScale) {
    ComponentArray<Position> positions;
    const int numEntities = 1000;

    // Create many entities
    for (int i = 0; i < numEntities; i++) {
        Entity e = {static_cast<u32>(i), 0};
        positions.Add(e, Position{{static_cast<f32>(i), 0.0f, 0.0f}});
    }

    ASSERT(positions.Size() == numEntities);

    // Verify all present
    for (int i = 0; i < numEntities; i++) {
        Entity e = {static_cast<u32>(i), 0};
        ASSERT(positions.Has(e));
        ASSERT(positions.Get(e).value.x == static_cast<f32>(i));
    }

    // Remove every other
    for (int i = 0; i < numEntities / 2; i++) {
        Entity e = {static_cast<u32>(i * 2), 0};
        positions.Remove(e);
    }

    ASSERT(positions.Size() == numEntities / 2);

    // Verify correct ones remain
    for (int i = 0; i < numEntities / 2; i++) {
        Entity even = {static_cast<u32>(i * 2), 0};
        Entity odd = {static_cast<u32>(i * 2 + 1), 0};

        ASSERT(!positions.Has(even));
        ASSERT(positions.Has(odd));
    }
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    std::cout << "=== ComponentArray Unit Tests ===" << std::endl;
    std::cout << std::endl;

    std::cout << "--- Basic Tests ---" << std::endl;
    ComponentArray_BasicAddAndGet_runner();
    ComponentArray_MultipleEntities_runner();
    ComponentArray_ModifyComponent_runner();
    ComponentArray_HasComponent_runner();

    std::cout << std::endl;
    std::cout << "--- Removal Tests ---" << std::endl;
    ComponentArray_BasicRemove_runner();
    ComponentArray_RemoveMiddleElement_runner();
    ComponentArray_SwapAndPopCorrectness_runner();
    ComponentArray_RemoveAll_runner();

    std::cout << std::endl;
    std::cout << "--- Iteration Tests ---" << std::endl;
    ComponentArray_DenseIteration_runner();
    ComponentArray_DataPointer_runner();
    ComponentArray_GetEntity_runner();
    ComponentArray_IterationAfterRemoval_runner();

    std::cout << std::endl;
    std::cout << "--- Sparse Array Growth Tests ---" << std::endl;
    ComponentArray_SparseArrayGrowth_runner();
    ComponentArray_NonSequentialEntities_runner();

    std::cout << std::endl;
    std::cout << "--- EntityManager Integration Tests ---" << std::endl;
    ComponentArray_WithEntityManager_runner();
    ComponentArray_GenerationHandling_runner();

    std::cout << std::endl;
    std::cout << "--- Multiple Component Types Tests ---" << std::endl;
    ComponentArray_MultipleComponentTypes_runner();
    ComponentArray_PartialComponents_runner();

    std::cout << std::endl;
    std::cout << "--- Large Scale Tests ---" << std::endl;
    ComponentArray_LargeScale_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
