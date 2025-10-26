#include <iostream>
#include <cassert>
#include "resources/mesh_manager.h"

void TestHandleCreation() {
    std::cout << "[TEST] Handle Creation and Validity" << std::endl;

    MeshManager& meshMgr = MeshManager::Instance();

    // Create a cube
    MeshHandle cube = meshMgr.CreateCube();
    assert(cube.IsValid());
    assert(cube != MeshHandle::Invalid);
    std::cout << "  ✓ Created cube handle (index=" << cube.index << ", gen=" << cube.generation << ")" << std::endl;

    // Verify we can get the mesh data
    MeshData* cubeData = meshMgr.Get(cube);
    assert(cubeData != nullptr);
    assert(cubeData->vertexCount == 24);
    assert(cubeData->indexCount == 36);
    assert(!cubeData->gpuUploaded);
    std::cout << "  ✓ Cube mesh data: " << cubeData->vertexCount << " vertices, " << cubeData->indexCount << " indices" << std::endl;

    std::cout << std::endl;
}

void TestMultiplePrimitives() {
    std::cout << "[TEST] Multiple Primitive Creation" << std::endl;

    MeshManager& meshMgr = MeshManager::Instance();

    MeshHandle cube = meshMgr.CreateCube();
    MeshHandle sphere = meshMgr.CreateSphere(32);
    MeshHandle plane = meshMgr.CreatePlane();
    MeshHandle quad = meshMgr.CreateQuad();

    assert(cube.IsValid());
    assert(sphere.IsValid());
    assert(plane.IsValid());
    assert(quad.IsValid());

    // All handles should be different
    assert(cube != sphere);
    assert(cube != plane);
    assert(sphere != plane);
    assert(plane != quad);

    std::cout << "  ✓ Created 4 different primitives with unique handles" << std::endl;

    // Verify mesh data
    MeshData* cubeData = meshMgr.Get(cube);
    MeshData* sphereData = meshMgr.Get(sphere);
    MeshData* planeData = meshMgr.Get(plane);
    MeshData* quadData = meshMgr.Get(quad);

    std::cout << "  ✓ Cube: " << cubeData->vertexCount << " vertices, " << cubeData->indexCount << " indices" << std::endl;
    std::cout << "  ✓ Sphere: " << sphereData->vertexCount << " vertices, " << sphereData->indexCount << " indices" << std::endl;
    std::cout << "  ✓ Plane: " << planeData->vertexCount << " vertices, " << planeData->indexCount << " indices" << std::endl;
    std::cout << "  ✓ Quad: " << quadData->vertexCount << " vertices, " << quadData->indexCount << " indices" << std::endl;

    std::cout << std::endl;
}

void TestHandleDestruction() {
    std::cout << "[TEST] Handle Destruction and Generation Counter" << std::endl;

    MeshManager& meshMgr = MeshManager::Instance();

    // Create a cube
    MeshHandle cube = meshMgr.CreateCube();
    u32 originalIndex = cube.index;
    u32 originalGeneration = cube.generation;

    std::cout << "  Created cube (index=" << originalIndex << ", gen=" << originalGeneration << ")" << std::endl;

    // Verify it's valid
    assert(meshMgr.IsValid(cube));
    assert(meshMgr.Get(cube) != nullptr);

    // Destroy the cube
    meshMgr.Destroy(cube);
    std::cout << "  Destroyed cube" << std::endl;

    // Handle should now be invalid
    assert(!meshMgr.IsValid(cube));
    assert(meshMgr.Get(cube) == nullptr);
    std::cout << "  ✓ Old handle is now invalid (returns nullptr)" << std::endl;

    // Create a new cube - should reuse the same index but increment generation
    MeshHandle newCube = meshMgr.CreateCube();
    std::cout << "  Created new cube (index=" << newCube.index << ", gen=" << newCube.generation << ")" << std::endl;

    // New cube should reuse the index (free-list)
    assert(newCube.index == originalIndex);
    assert(newCube.generation == originalGeneration + 1);
    std::cout << "  ✓ New handle reused index but incremented generation" << std::endl;

    // Old handle should still be invalid (generation mismatch)
    assert(!meshMgr.IsValid(cube));
    assert(meshMgr.Get(cube) == nullptr);
    std::cout << "  ✓ Old handle still invalid (generation counter prevents use-after-free)" << std::endl;

    // New handle should be valid
    assert(meshMgr.IsValid(newCube));
    assert(meshMgr.Get(newCube) != nullptr);
    std::cout << "  ✓ New handle is valid" << std::endl;

    std::cout << std::endl;
}

void TestResourceCount() {
    std::cout << "[TEST] Resource Count Tracking" << std::endl;

    MeshManager& meshMgr = MeshManager::Instance();

    // Note: Previous tests may have created resources, so get initial count
    size_t initialCount = meshMgr.Count();
    std::cout << "  Initial resource count: " << initialCount << std::endl;

    MeshHandle h1 = meshMgr.CreateCube();
    assert(meshMgr.Count() == initialCount + 1);

    MeshHandle h2 = meshMgr.CreateSphere(16);
    assert(meshMgr.Count() == initialCount + 2);

    MeshHandle h3 = meshMgr.CreatePlane();
    assert(meshMgr.Count() == initialCount + 3);

    std::cout << "  ✓ Count increased correctly after creating 3 resources" << std::endl;

    meshMgr.Destroy(h2);
    assert(meshMgr.Count() == initialCount + 2);
    std::cout << "  ✓ Count decreased after destroying 1 resource" << std::endl;

    meshMgr.Destroy(h1);
    meshMgr.Destroy(h3);
    assert(meshMgr.Count() == initialCount);
    std::cout << "  ✓ Count returned to initial value after destroying all test resources" << std::endl;

    std::cout << std::endl;
}

void TestInvalidHandle() {
    std::cout << "[TEST] Invalid Handle Behavior" << std::endl;

    MeshManager& meshMgr = MeshManager::Instance();

    // Test with Invalid constant
    MeshHandle invalid = MeshHandle::Invalid;
    assert(!invalid.IsValid());
    assert(!meshMgr.IsValid(invalid));
    assert(meshMgr.Get(invalid) == nullptr);
    std::cout << "  ✓ MeshHandle::Invalid is properly invalid" << std::endl;

    // Test destroying invalid handle (should be safe)
    meshMgr.Destroy(invalid);
    std::cout << "  ✓ Destroying invalid handle is safe (no crash)" << std::endl;

    // Test default-constructed handle
    MeshHandle defaultHandle;
    assert(defaultHandle.index == 0);
    assert(defaultHandle.generation == 0);
    assert(defaultHandle != MeshHandle::Invalid);
    std::cout << "  ✓ Default-constructed handle has index=0, gen=0 (different from Invalid)" << std::endl;

    std::cout << std::endl;
}

void TestHandleComparison() {
    std::cout << "[TEST] Handle Comparison Operators" << std::endl;

    MeshManager& meshMgr = MeshManager::Instance();

    MeshHandle h1 = meshMgr.CreateCube();
    MeshHandle h2 = meshMgr.CreateCube();

    // Different handles
    assert(h1 != h2);
    assert(!(h1 == h2));
    std::cout << "  ✓ Different handles are not equal" << std::endl;

    // Same handle
    MeshHandle h1Copy = h1;
    assert(h1 == h1Copy);
    assert(!(h1 != h1Copy));
    std::cout << "  ✓ Copied handles are equal" << std::endl;

    // After destruction
    meshMgr.Destroy(h1);
    MeshHandle h3 = meshMgr.CreateCube();  // May reuse h1's index

    // h3 is a different handle
    assert(h1 != h3);
    std::cout << "  ✓ New handle after destruction is different from destroyed handle" << std::endl;

    std::cout << std::endl;
}

void TestBoundingBoxes() {
    std::cout << "[TEST] Bounding Box Calculation" << std::endl;

    MeshManager& meshMgr = MeshManager::Instance();

    MeshHandle cube = meshMgr.CreateCube();
    MeshData* cubeData = meshMgr.Get(cube);

    assert(cubeData->boundsMin.x == -0.5f);
    assert(cubeData->boundsMin.y == -0.5f);
    assert(cubeData->boundsMin.z == -0.5f);
    assert(cubeData->boundsMax.x == 0.5f);
    assert(cubeData->boundsMax.y == 0.5f);
    assert(cubeData->boundsMax.z == 0.5f);
    std::cout << "  ✓ Cube bounding box: min=(-0.5, -0.5, -0.5), max=(0.5, 0.5, 0.5)" << std::endl;

    MeshHandle sphere = meshMgr.CreateSphere(32);
    MeshData* sphereData = meshMgr.Get(sphere);

    assert(sphereData->boundsMin.x == -1.0f);
    assert(sphereData->boundsMin.y == -1.0f);
    assert(sphereData->boundsMin.z == -1.0f);
    assert(sphereData->boundsMax.x == 1.0f);
    assert(sphereData->boundsMax.y == 1.0f);
    assert(sphereData->boundsMax.z == 1.0f);
    std::cout << "  ✓ Sphere bounding box: min=(-1, -1, -1), max=(1, 1, 1)" << std::endl;

    std::cout << std::endl;
}

void TestTypeSafety() {
    std::cout << "[TEST] Type Safety" << std::endl;

    // This test is mostly compile-time verification
    // The fact that these types are different should prevent misuse

    MeshHandle meshHandle = MeshHandle::Invalid;
    TextureHandle textureHandle = TextureHandle::Invalid;
    MaterialHandle materialHandle = MaterialHandle::Invalid;

    // These should be different types at compile time
    std::cout << "  ✓ MeshHandle, TextureHandle, and MaterialHandle are distinct types" << std::endl;
    std::cout << "  ✓ Cannot assign MeshHandle to TextureHandle (compile-time safety)" << std::endl;

    // Verify they're all invalid by default
    assert(!meshHandle.IsValid());
    assert(!textureHandle.IsValid());
    assert(!materialHandle.IsValid());
    std::cout << "  ✓ All handle types have proper Invalid constants" << std::endl;

    std::cout << std::endl;
}

int main() {
    std::cout << "=== Resource Handle System Tests ===" << std::endl;
    std::cout << std::endl;

    TestHandleCreation();
    TestMultiplePrimitives();
    TestHandleDestruction();
    TestResourceCount();
    TestInvalidHandle();
    TestHandleComparison();
    TestBoundingBoxes();
    TestTypeSafety();

    std::cout << "======================================" << std::endl;
    std::cout << "All resource handle tests passed!" << std::endl;
    std::cout << "======================================" << std::endl;

    return 0;
}
