#include "ecs/components/transform.h"
#include "ecs/systems/transform_system.h"
#include "ecs/component_registry.h"
#include "ecs/entity_manager.h"
#include "ecs/hierarchy_manager.h"
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

// ============================================================================
// Transform Component Tests
// ============================================================================

TEST(Transform_DefaultInitialization) {
    Transform t;

    ASSERT(Vec3Equal(t.localPosition, Vec3(0, 0, 0)));
    ASSERT(FloatEqual(t.localRotation.w, 1.0f));
    ASSERT(FloatEqual(t.localRotation.x, 0.0f));
    ASSERT(FloatEqual(t.localRotation.y, 0.0f));
    ASSERT(FloatEqual(t.localRotation.z, 0.0f));
    ASSERT(Vec3Equal(t.localScale, Vec3(1, 1, 1)));
    ASSERT(t.parent == Entity::Invalid);
    ASSERT(!t.parent.IsValid());
    ASSERT(t.isDirty == true);
}

TEST(Transform_GetLocalMatrix_Identity) {
    Transform t;
    Mat4 localMatrix = t.GetLocalMatrix();

    // Identity matrix should result from default transform
    Mat4 identity(1.0f);
    ASSERT(Mat4Equal(localMatrix, identity));
}

TEST(Transform_GetLocalMatrix_Translation) {
    Transform t;
    t.localPosition = Vec3(10, 20, 30);

    Mat4 localMatrix = t.GetLocalMatrix();
    Mat4 expected = Translate(Mat4(1.0f), Vec3(10, 20, 30));

    ASSERT(Mat4Equal(localMatrix, expected));
}

TEST(Transform_GetLocalMatrix_Scale) {
    Transform t;
    t.localScale = Vec3(2, 3, 4);

    Mat4 localMatrix = t.GetLocalMatrix();
    Mat4 expected = Scale(Mat4(1.0f), Vec3(2, 3, 4));

    ASSERT(Mat4Equal(localMatrix, expected));
}

TEST(Transform_GetLocalMatrix_Rotation) {
    Transform t;
    t.localRotation = QuatFromAxisAngle(Vec3(0, 1, 0), Radians(90.0f));

    Mat4 localMatrix = t.GetLocalMatrix();
    Mat4 expected = QuatToMat4(QuatFromAxisAngle(Vec3(0, 1, 0), Radians(90.0f)));

    ASSERT(Mat4Equal(localMatrix, expected));
}

TEST(Transform_GetLocalMatrix_Combined) {
    Transform t;
    t.localPosition = Vec3(5, 10, 15);
    t.localRotation = QuatFromAxisAngle(Vec3(0, 0, 1), Radians(45.0f));
    t.localScale = Vec3(2, 2, 2);

    Mat4 localMatrix = t.GetLocalMatrix();

    // Manually compute expected TRS
    Mat4 translation = Translate(Mat4(1.0f), Vec3(5, 10, 15));
    Mat4 rotation = QuatToMat4(QuatFromAxisAngle(Vec3(0, 0, 1), Radians(45.0f)));
    Mat4 scale = Scale(Mat4(1.0f), Vec3(2, 2, 2));
    Mat4 expected = translation * rotation * scale;

    ASSERT(Mat4Equal(localMatrix, expected));
}

TEST(Transform_MarkDirty) {
    Transform t;
    t.isDirty = false;

    t.MarkDirty();

    ASSERT(t.isDirty == true);
}

TEST(Transform_ParentReference) {
    Transform t;
    Entity parent = {5, 2};

    t.parent = parent;

    ASSERT(t.parent == parent);
    ASSERT(t.parent.IsValid());
    ASSERT(t.parent.index == 5);
    ASSERT(t.parent.generation == 2);
}

// ============================================================================
// TransformSystem Tests
// ============================================================================

TEST(TransformSystem_BasicSetup) {
    ComponentRegistry registry;
    registry.RegisterComponent<Transform>();
    HierarchyManager hierarchy;

    TransformSystem system(&registry, &hierarchy);

    // Should not crash with empty component array
    system.Update(0.016f);
}

TEST(TransformSystem_RootEntity_UpdateWorldMatrix) {
    ComponentRegistry registry;
    registry.RegisterComponent<Transform>();
    HierarchyManager hierarchy;
    TransformSystem system(&registry, &hierarchy);

    EntityManager em;
    Entity entity = em.CreateEntity();

    auto transforms = registry.GetComponentArray<Transform>();
    Transform t;
    t.localPosition = Vec3(10, 20, 30);
    t.isDirty = true;
    transforms->Add(entity, t);

    // Update system
    system.Update(0.016f);

    // World matrix should equal local matrix for root entity
    Transform& updated = transforms->Get(entity);
    Mat4 expected = Translate(Mat4(1.0f), Vec3(10, 20, 30));

    ASSERT(Mat4Equal(updated.worldMatrix, expected));
    ASSERT(updated.isDirty == false);
}

TEST(TransformSystem_HierarchyAlwaysUpdates) {
    ComponentRegistry registry;
    registry.RegisterComponent<Transform>();
    HierarchyManager hierarchy;
    TransformSystem system(&registry, &hierarchy);

    EntityManager em;
    Entity entity = em.CreateEntity();

    auto transforms = registry.GetComponentArray<Transform>();
    Transform t;
    t.localPosition = Vec3(5, 5, 5);
    t.isDirty = true;
    transforms->Add(entity, t);

    // First update
    system.Update(0.016f);
    Transform& updated = transforms->Get(entity);
    ASSERT(updated.isDirty == false);

    // Modify transform position
    updated.localPosition = Vec3(100, 100, 100);

    // Second update - hierarchy system always updates transforms
    system.Update(0.016f);

    // World matrix should be updated with new position
    Mat4 expected = Translate(Mat4(1.0f), Vec3(100, 100, 100));
    ASSERT(Mat4Equal(updated.worldMatrix, expected));
}

TEST(TransformSystem_DirtyFlagUpdate) {
    ComponentRegistry registry;
    registry.RegisterComponent<Transform>();
    HierarchyManager hierarchy;
    TransformSystem system(&registry, &hierarchy);

    EntityManager em;
    Entity entity = em.CreateEntity();

    auto transforms = registry.GetComponentArray<Transform>();
    Transform t;
    t.localPosition = Vec3(5, 5, 5);
    t.isDirty = true;
    transforms->Add(entity, t);

    // First update
    system.Update(0.016f);
    Transform& updated = transforms->Get(entity);
    ASSERT(updated.isDirty == false);

    // Modify and mark dirty
    updated.localPosition = Vec3(100, 100, 100);
    updated.MarkDirty();
    ASSERT(updated.isDirty == true);

    // Second update - should recompute
    system.Update(0.016f);

    Mat4 expected = Translate(Mat4(1.0f), Vec3(100, 100, 100));
    ASSERT(Mat4Equal(updated.worldMatrix, expected));
    ASSERT(updated.isDirty == false);
}

TEST(TransformSystem_MultipleEntities) {
    ComponentRegistry registry;
    registry.RegisterComponent<Transform>();
    HierarchyManager hierarchy;
    TransformSystem system(&registry, &hierarchy);

    EntityManager em;
    Entity e1 = em.CreateEntity();
    Entity e2 = em.CreateEntity();
    Entity e3 = em.CreateEntity();

    auto transforms = registry.GetComponentArray<Transform>();

    Transform t1;
    t1.localPosition = Vec3(10, 0, 0);
    t1.isDirty = true;
    transforms->Add(e1, t1);

    Transform t2;
    t2.localPosition = Vec3(0, 20, 0);
    t2.isDirty = true;
    transforms->Add(e2, t2);

    Transform t3;
    t3.localPosition = Vec3(0, 0, 30);
    t3.isDirty = true;
    transforms->Add(e3, t3);

    // Update all
    system.Update(0.016f);

    // Verify each entity has correct world matrix
    Transform& updated1 = transforms->Get(e1);
    Transform& updated2 = transforms->Get(e2);
    Transform& updated3 = transforms->Get(e3);

    Mat4 expected1 = Translate(Mat4(1.0f), Vec3(10, 0, 0));
    Mat4 expected2 = Translate(Mat4(1.0f), Vec3(0, 20, 0));
    Mat4 expected3 = Translate(Mat4(1.0f), Vec3(0, 0, 30));

    ASSERT(Mat4Equal(updated1.worldMatrix, expected1));
    ASSERT(Mat4Equal(updated2.worldMatrix, expected2));
    ASSERT(Mat4Equal(updated3.worldMatrix, expected3));

    ASSERT(updated1.isDirty == false);
    ASSERT(updated2.isDirty == false);
    ASSERT(updated3.isDirty == false);
}

TEST(TransformSystem_ParentChildHierarchy) {
    ComponentRegistry registry;
    registry.RegisterComponent<Transform>();
    HierarchyManager hierarchy;
    TransformSystem system(&registry, &hierarchy);

    EntityManager em;
    Entity parent = em.CreateEntity();
    Entity child = em.CreateEntity();

    auto transforms = registry.GetComponentArray<Transform>();

    // Parent at (10, 0, 0)
    Transform parentTransform;
    parentTransform.localPosition = Vec3(10, 0, 0);
    parentTransform.isDirty = true;
    transforms->Add(parent, parentTransform);

    // Child at (5, 0, 0) relative to parent
    Transform childTransform;
    childTransform.localPosition = Vec3(5, 0, 0);
    childTransform.isDirty = true;
    transforms->Add(child, childTransform);

    // Set up hierarchy
    hierarchy.SetParent(child, parent);

    // Update system
    system.Update(0.016f);

    // Child world position should be (15, 0, 0)
    Transform& updatedParent = transforms->Get(parent);
    Transform& updatedChild = transforms->Get(child);

    Mat4 expectedParent = Translate(Mat4(1.0f), Vec3(10, 0, 0));
    Mat4 expectedChild = Translate(Mat4(1.0f), Vec3(15, 0, 0));

    ASSERT(Mat4Equal(updatedParent.worldMatrix, expectedParent));
    ASSERT(Mat4Equal(updatedChild.worldMatrix, expectedChild));
    ASSERT(updatedParent.isDirty == false);
    ASSERT(updatedChild.isDirty == false);
}

TEST(TransformSystem_DeepHierarchy) {
    ComponentRegistry registry;
    registry.RegisterComponent<Transform>();
    HierarchyManager hierarchy;
    TransformSystem system(&registry, &hierarchy);

    EntityManager em;
    Entity root = em.CreateEntity();
    Entity child1 = em.CreateEntity();
    Entity child2 = em.CreateEntity();

    auto transforms = registry.GetComponentArray<Transform>();

    // Root at (10, 0, 0)
    Transform rootTransform;
    rootTransform.localPosition = Vec3(10, 0, 0);
    transforms->Add(root, rootTransform);

    // Child1 at (5, 0, 0) relative to root
    Transform child1Transform;
    child1Transform.localPosition = Vec3(5, 0, 0);
    transforms->Add(child1, child1Transform);

    // Child2 at (3, 0, 0) relative to child1
    Transform child2Transform;
    child2Transform.localPosition = Vec3(3, 0, 0);
    transforms->Add(child2, child2Transform);

    // Set up hierarchy: root -> child1 -> child2
    hierarchy.SetParent(child1, root);
    hierarchy.SetParent(child2, child1);

    // Update system
    system.Update(0.016f);

    // Verify world positions:
    // root: (10, 0, 0)
    // child1: (15, 0, 0)
    // child2: (18, 0, 0)
    Transform& updatedRoot = transforms->Get(root);
    Transform& updatedChild1 = transforms->Get(child1);
    Transform& updatedChild2 = transforms->Get(child2);

    Mat4 expectedRoot = Translate(Mat4(1.0f), Vec3(10, 0, 0));
    Mat4 expectedChild1 = Translate(Mat4(1.0f), Vec3(15, 0, 0));
    Mat4 expectedChild2 = Translate(Mat4(1.0f), Vec3(18, 0, 0));

    ASSERT(Mat4Equal(updatedRoot.worldMatrix, expectedRoot));
    ASSERT(Mat4Equal(updatedChild1.worldMatrix, expectedChild1));
    ASSERT(Mat4Equal(updatedChild2.worldMatrix, expectedChild2));
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    std::cout << "=== Transform System Unit Tests ===" << std::endl;
    std::cout << std::endl;

    std::cout << "--- Transform Component Tests ---" << std::endl;
    Transform_DefaultInitialization_runner();
    Transform_GetLocalMatrix_Identity_runner();
    Transform_GetLocalMatrix_Translation_runner();
    Transform_GetLocalMatrix_Scale_runner();
    Transform_GetLocalMatrix_Rotation_runner();
    Transform_GetLocalMatrix_Combined_runner();
    Transform_MarkDirty_runner();
    Transform_ParentReference_runner();

    std::cout << std::endl;
    std::cout << "--- TransformSystem Tests ---" << std::endl;
    TransformSystem_BasicSetup_runner();
    TransformSystem_RootEntity_UpdateWorldMatrix_runner();
    TransformSystem_HierarchyAlwaysUpdates_runner();
    TransformSystem_DirtyFlagUpdate_runner();
    TransformSystem_MultipleEntities_runner();
    TransformSystem_ParentChildHierarchy_runner();
    TransformSystem_DeepHierarchy_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
