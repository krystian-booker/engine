#include "ecs/scene_serializer.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/components/transform.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <filesystem>

// Helper function to compare floats with tolerance
bool FloatEquals(f32 a, f32 b, f32 epsilon = 0.0001f) {
    return std::abs(a - b) < epsilon;
}

// Helper function to compare Vec3
bool Vec3Equals(const Vec3& a, const Vec3& b) {
    return FloatEquals(a.x, b.x) && FloatEquals(a.y, b.y) && FloatEquals(a.z, b.z);
}

// Helper function to compare Quat
bool QuatEquals(const Quat& a, const Quat& b) {
    return FloatEquals(a.w, b.w) && FloatEquals(a.x, b.x) &&
           FloatEquals(a.y, b.y) && FloatEquals(a.z, b.z);
}

void TestBasicSaveLoad() {
    std::cout << "\n=== Test 1: Basic Save/Load ===" << std::endl;

    ECSCoordinator ecs;
    ecs.Init();

    // Create entity with transform
    Entity entity = ecs.CreateEntity();
    Transform transform;
    transform.localPosition = Vec3(1.0f, 2.0f, 3.0f);
    transform.localRotation = Quat(0.707f, 0.707f, 0.0f, 0.0f);
    transform.localScale = Vec3(2.0f, 2.0f, 2.0f);
    ecs.AddComponent(entity, transform);

    // Save scene
    SceneSerializer serializer(&ecs);
    bool saved = serializer.SaveScene("test_basic.json");
    assert(saved && "Failed to save scene");

    // Clear scene
    ecs.DestroyEntity(entity);
    assert(ecs.GetEntityCount() == 0 && "Entity not destroyed");

    // Load scene
    bool loaded = serializer.LoadScene("test_basic.json");
    assert(loaded && "Failed to load scene");
    assert(ecs.GetEntityCount() == 1 && "Entity not created");

    // Verify loaded data
    auto entities = ecs.QueryEntities<Transform>();
    assert(entities.size() == 1 && "Should have 1 entity with Transform");

    Entity loadedEntity = entities[0];
    const Transform& loadedTransform = ecs.GetComponent<Transform>(loadedEntity);

    assert(Vec3Equals(loadedTransform.localPosition, Vec3(1.0f, 2.0f, 3.0f)) && "Position mismatch");
    assert(QuatEquals(loadedTransform.localRotation, Quat(0.707f, 0.707f, 0.0f, 0.0f)) && "Rotation mismatch");
    assert(Vec3Equals(loadedTransform.localScale, Vec3(2.0f, 2.0f, 2.0f)) && "Scale mismatch");

    std::cout << "✓ Basic save/load test passed" << std::endl;

    // Cleanup
    std::filesystem::remove("test_basic.json");
    ecs.Shutdown();
}

void TestHierarchy() {
    std::cout << "\n=== Test 2: Hierarchy Preservation ===" << std::endl;

    ECSCoordinator ecs;
    ecs.Init();

    // Create parent
    Entity parent = ecs.CreateEntity();
    Transform parentTransform;
    parentTransform.localPosition = Vec3(10.0f, 5.0f, 0.0f);
    ecs.AddComponent(parent, parentTransform);

    // Create child
    Entity child = ecs.CreateEntity();
    Transform childTransform;
    childTransform.localPosition = Vec3(2.0f, 0.0f, 0.0f);
    ecs.AddComponent(child, childTransform);

    ecs.SetParent(child, parent);

    // Save scene
    SceneSerializer serializer(&ecs);
    bool saved = serializer.SaveScene("test_hierarchy.json");
    assert(saved && "Failed to save scene");

    // Clear scene
    ecs.DestroyEntity(parent);
    ecs.DestroyEntity(child);
    assert(ecs.GetEntityCount() == 0 && "Entities not destroyed");

    // Load scene
    bool loaded = serializer.LoadScene("test_hierarchy.json");
    assert(loaded && "Failed to load scene");
    assert(ecs.GetEntityCount() == 2 && "Should have 2 entities");

    // Find parent and child by position
    auto entities = ecs.QueryEntities<Transform>();
    Entity loadedParent = Entity::Invalid;
    Entity loadedChild = Entity::Invalid;

    for (Entity e : entities) {
        const Transform& t = ecs.GetComponent<Transform>(e);
        if (Vec3Equals(t.localPosition, Vec3(10.0f, 5.0f, 0.0f))) {
            loadedParent = e;
        } else if (Vec3Equals(t.localPosition, Vec3(2.0f, 0.0f, 0.0f))) {
            loadedChild = e;
        }
    }

    assert(loadedParent.IsValid() && "Parent not found");
    assert(loadedChild.IsValid() && "Child not found");

    // Verify hierarchy
    Entity childParent = ecs.GetParent(loadedChild);
    assert(childParent == loadedParent && "Hierarchy not preserved");

    const std::vector<Entity>& children = ecs.GetChildren(loadedParent);
    assert(children.size() == 1 && "Parent should have 1 child");
    assert(children[0] == loadedChild && "Child not linked to parent");

    std::cout << "✓ Hierarchy preservation test passed" << std::endl;

    // Cleanup
    std::filesystem::remove("test_hierarchy.json");
    ecs.Shutdown();
}

void TestComplexScene() {
    std::cout << "\n=== Test 3: Complex Scene with Multiple Entities ===" << std::endl;

    ECSCoordinator ecs;
    ecs.Init();

    // Create root
    Entity root = ecs.CreateEntity();
    Transform rootTransform;
    rootTransform.localPosition = Vec3(0.0f, 0.0f, 0.0f);
    ecs.AddComponent(root, rootTransform);

    // Create child1
    Entity child1 = ecs.CreateEntity();
    Transform child1Transform;
    child1Transform.localPosition = Vec3(5.0f, 0.0f, 0.0f);
    ecs.AddComponent(child1, child1Transform);
    ecs.SetParent(child1, root);

    // Create child2
    Entity child2 = ecs.CreateEntity();
    Transform child2Transform;
    child2Transform.localPosition = Vec3(-5.0f, 0.0f, 0.0f);
    ecs.AddComponent(child2, child2Transform);
    ecs.SetParent(child2, root);

    // Create grandchild
    Entity grandchild = ecs.CreateEntity();
    Transform grandchildTransform;
    grandchildTransform.localPosition = Vec3(0.0f, 3.0f, 0.0f);
    ecs.AddComponent(grandchild, grandchildTransform);
    ecs.SetParent(grandchild, child1);

    // Save scene
    SceneSerializer serializer(&ecs);
    bool saved = serializer.SaveScene("test_complex.json");
    assert(saved && "Failed to save scene");

    u32 originalCount = ecs.GetEntityCount();

    // Clear scene
    ecs.DestroyEntity(root);
    ecs.DestroyEntity(child1);
    ecs.DestroyEntity(child2);
    ecs.DestroyEntity(grandchild);
    assert(ecs.GetEntityCount() == 0 && "Entities not destroyed");

    // Load scene
    bool loaded = serializer.LoadScene("test_complex.json");
    assert(loaded && "Failed to load scene");
    assert(ecs.GetEntityCount() == originalCount && "Entity count mismatch");

    // Verify all entities loaded
    auto entities = ecs.QueryEntities<Transform>();
    assert(entities.size() == 4 && "Should have 4 entities");

    // Verify hierarchy structure
    std::vector<Entity> rootEntities = ecs.GetRootEntities();
    assert(rootEntities.size() == 1 && "Should have 1 root entity");

    Entity loadedRoot = rootEntities[0];
    const std::vector<Entity>& rootChildren = ecs.GetChildren(loadedRoot);
    assert(rootChildren.size() == 2 && "Root should have 2 children");

    // Find which child has a grandchild
    Entity childWithGrandchild = Entity::Invalid;
    for (Entity child : rootChildren) {
        if (ecs.HasChildren(child)) {
            childWithGrandchild = child;
            break;
        }
    }

    assert(childWithGrandchild.IsValid() && "Should find child with grandchild");

    const std::vector<Entity>& grandchildren = ecs.GetChildren(childWithGrandchild);
    assert(grandchildren.size() == 1 && "Child should have 1 grandchild");

    std::cout << "✓ Complex scene test passed" << std::endl;

    // Cleanup
    std::filesystem::remove("test_complex.json");
    ecs.Shutdown();
}

void TestEmptyScene() {
    std::cout << "\n=== Test 4: Empty Scene ===" << std::endl;

    ECSCoordinator ecs;
    ecs.Init();

    // Save empty scene
    SceneSerializer serializer(&ecs);
    bool saved = serializer.SaveScene("test_empty.json");
    assert(saved && "Failed to save empty scene");

    // Load empty scene
    bool loaded = serializer.LoadScene("test_empty.json");
    assert(loaded && "Failed to load empty scene");
    assert(ecs.GetEntityCount() == 0 && "Empty scene should have no entities");

    std::cout << "✓ Empty scene test passed" << std::endl;

    // Cleanup
    std::filesystem::remove("test_empty.json");
    ecs.Shutdown();
}

void TestFileErrors() {
    std::cout << "\n=== Test 5: File Error Handling ===" << std::endl;

    ECSCoordinator ecs;
    ecs.Init();

    SceneSerializer serializer(&ecs);

    // Test loading non-existent file
    bool loaded = serializer.LoadScene("nonexistent_file.json");
    assert(!loaded && "Should fail to load non-existent file");

    // Test loading invalid path (directory that doesn't exist)
    bool loadedInvalid = serializer.LoadScene("invalid_dir/test.json");
    assert(!loadedInvalid && "Should fail to load from invalid path");

    std::cout << "✓ File error handling test passed" << std::endl;

    ecs.Shutdown();
}

void TestMultipleSaveLoad() {
    std::cout << "\n=== Test 6: Multiple Save/Load (Idempotence) ===" << std::endl;

    ECSCoordinator ecs;
    ecs.Init();

    // Create test entity
    Entity entity = ecs.CreateEntity();
    Transform transform;
    transform.localPosition = Vec3(7.0f, 8.0f, 9.0f);
    ecs.AddComponent(entity, transform);

    SceneSerializer serializer(&ecs);

    // Save, load, save again, load again
    serializer.SaveScene("test_idempotent.json");
    ecs.DestroyEntity(entity);

    serializer.LoadScene("test_idempotent.json");
    auto entities1 = ecs.QueryEntities<Transform>();
    const Transform& t1 = ecs.GetComponent<Transform>(entities1[0]);

    serializer.SaveScene("test_idempotent2.json");
    ecs.DestroyEntity(entities1[0]);

    serializer.LoadScene("test_idempotent2.json");
    auto entities2 = ecs.QueryEntities<Transform>();
    const Transform& t2 = ecs.GetComponent<Transform>(entities2[0]);

    // Verify data matches after multiple save/load cycles
    assert(Vec3Equals(t1.localPosition, t2.localPosition) && "Position mismatch after multiple cycles");
    assert(QuatEquals(t1.localRotation, t2.localRotation) && "Rotation mismatch after multiple cycles");
    assert(Vec3Equals(t1.localScale, t2.localScale) && "Scale mismatch after multiple cycles");

    std::cout << "✓ Multiple save/load test passed" << std::endl;

    // Cleanup
    std::filesystem::remove("test_idempotent.json");
    std::filesystem::remove("test_idempotent2.json");
    ecs.Shutdown();
}

int main() {
    std::cout << "Running Scene Serializer Tests..." << std::endl;

    try {
        TestBasicSaveLoad();
        TestHierarchy();
        TestComplexScene();
        TestEmptyScene();
        TestFileErrors();
        TestMultipleSaveLoad();

        std::cout << "\n====================================" << std::endl;
        std::cout << "All tests passed! ✓" << std::endl;
        std::cout << "====================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
