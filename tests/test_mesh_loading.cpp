#include "resources/mesh_manager.h"
#include "core/types.h"
#include <iostream>
#include <cassert>

void TestLoadOBJ() {
    std::cout << "Test: Load OBJ file..." << std::endl;

    MeshManager& meshMgr = MeshManager::Instance();
    MeshHandle handle = meshMgr.Load(ENGINE_SOURCE_DIR "/tests/test_assets/cube.obj");

    MeshData* mesh = meshMgr.Get(handle);
    assert(mesh != nullptr && "Mesh should be loaded");
    assert(mesh->vertexCount > 0 && "Mesh should have vertices");
    assert(mesh->indexCount > 0 && "Mesh should have indices");

    std::cout << "  Loaded " << mesh->vertexCount << " vertices, "
              << mesh->indexCount << " indices" << std::endl;

    // Verify tangent data exists
    for (u32 i = 0; i < mesh->vertexCount; ++i) {
        const Vertex& v = mesh->vertices[i];
        assert((v.tangent.w == 1.0f || v.tangent.w == -1.0f) && "Tangent handedness should be +1 or -1");
    }

    std::cout << "  ✓ OBJ loading test passed" << std::endl;
}

void TestBoundingBox() {
    std::cout << "Test: Bounding box calculation..." << std::endl;

    MeshManager& meshMgr = MeshManager::Instance();
    MeshHandle handle = meshMgr.Load(ENGINE_SOURCE_DIR "/tests/test_assets/cube.obj");

    MeshData* mesh = meshMgr.Get(handle);
    assert(mesh != nullptr);

    // Cube should have bounds approximately [-0.5, -0.5, -0.5] to [0.5, 0.5, 0.5]
    f32 epsilon = 0.01f;
    assert(mesh->boundsMin.x < -0.5f + epsilon && mesh->boundsMin.x > -0.5f - epsilon);
    assert(mesh->boundsMax.x < 0.5f + epsilon && mesh->boundsMax.x > 0.5f - epsilon);

    std::cout << "  Bounds: [" << mesh->boundsMin.x << ", " << mesh->boundsMin.y << ", " << mesh->boundsMin.z
              << "] to [" << mesh->boundsMax.x << ", " << mesh->boundsMax.y << ", " << mesh->boundsMax.z << "]" << std::endl;
    std::cout << "  ✓ Bounding box test passed" << std::endl;
}

void TestProceduralMesh() {
    std::cout << "Test: Procedural mesh generation..." << std::endl;

    MeshManager& meshMgr = MeshManager::Instance();
    MeshHandle cubeHandle = meshMgr.CreateCube();
    MeshHandle sphereHandle = meshMgr.CreateSphere(16);
    MeshHandle planeHandle = meshMgr.CreatePlane();

    MeshData* cube = meshMgr.Get(cubeHandle);
    MeshData* sphere = meshMgr.Get(sphereHandle);
    MeshData* plane = meshMgr.Get(planeHandle);

    assert(cube != nullptr && cube->vertexCount > 0);
    assert(sphere != nullptr && sphere->vertexCount > 0);
    assert(plane != nullptr && plane->vertexCount > 0);

    std::cout << "  Cube: " << cube->vertexCount << " vertices" << std::endl;
    std::cout << "  Sphere: " << sphere->vertexCount << " vertices" << std::endl;
    std::cout << "  Plane: " << plane->vertexCount << " vertices" << std::endl;
    std::cout << "  ✓ Procedural mesh test passed" << std::endl;
}

void TestResourceCaching() {
    std::cout << "Test: Resource caching..." << std::endl;

    MeshManager& meshMgr = MeshManager::Instance();
    MeshHandle handle1 = meshMgr.Load(ENGINE_SOURCE_DIR "/tests/test_assets/cube.obj");
    MeshHandle handle2 = meshMgr.Load(ENGINE_SOURCE_DIR "/tests/test_assets/cube.obj");

    // Same path should return same handle (due to caching in ResourceManager)
    assert(handle1.index == handle2.index && "Same file should return cached handle");

    std::cout << "  ✓ Resource caching test passed" << std::endl;
}

int main() {
    std::cout << "=== Mesh Loading Tests ===" << std::endl;

    TestLoadOBJ();
    TestBoundingBox();
    TestProceduralMesh();
    TestResourceCaching();

    std::cout << "\n=== All Mesh Loading Tests Passed ===" << std::endl;
    return 0;
}
