#include <iostream>
#include "resources/mesh_manager.h"

int main() {
    std::cout << "=== Resource Handle System Example ===" << std::endl;
    std::cout << std::endl;

    // Get singleton instance
    MeshManager& meshMgr = MeshManager::Instance();

    // Create primitive meshes
    std::cout << "[1] Creating Primitive Meshes" << std::endl;
    MeshHandle cube = meshMgr.CreateCube();
    MeshHandle sphere = meshMgr.CreateSphere(32);
    MeshHandle plane = meshMgr.CreatePlane();
    MeshHandle quad = meshMgr.CreateQuad();

    std::cout << "  Created cube handle:   index=" << cube.index << ", gen=" << cube.generation << std::endl;
    std::cout << "  Created sphere handle: index=" << sphere.index << ", gen=" << sphere.generation << std::endl;
    std::cout << "  Created plane handle:  index=" << plane.index << ", gen=" << plane.generation << std::endl;
    std::cout << "  Created quad handle:   index=" << quad.index << ", gen=" << quad.generation << std::endl;
    std::cout << std::endl;

    // Access mesh data through handles
    std::cout << "[2] Accessing Mesh Data" << std::endl;
    MeshData* cubeData = meshMgr.Get(cube);
    if (cubeData) {
        std::cout << "  Cube mesh:" << std::endl;
        std::cout << "    Vertices: " << cubeData->vertexCount << std::endl;
        std::cout << "    Indices:  " << cubeData->indexCount << std::endl;
        std::cout << "    Bounds:   min(" << cubeData->boundsMin.x << ", "
                  << cubeData->boundsMin.y << ", " << cubeData->boundsMin.z << "), "
                  << "max(" << cubeData->boundsMax.x << ", "
                  << cubeData->boundsMax.y << ", " << cubeData->boundsMax.z << ")" << std::endl;
        if (!cubeData->vertices.empty()) {
            const Vertex& v = cubeData->vertices.front();
            std::cout << "    First vertex position: (" << v.position.x << ", " << v.position.y << ", " << v.position.z << ")" << std::endl;
            std::cout << "    First vertex color:    (" << v.color.r << ", " << v.color.g << ", " << v.color.b << ")" << std::endl;
            std::cout << "    First vertex texcoord: (" << v.texCoord.x << ", " << v.texCoord.y << ")" << std::endl;
        }
        std::cout << "    GPU uploaded: " << std::boolalpha << cubeData->gpuUploaded << std::noboolalpha << std::endl;
    }
    std::cout << std::endl;

    // Demonstrate generation counter (use-after-free safety)
    std::cout << "[3] Generation Counter Demo" << std::endl;
    std::cout << "  Original sphere handle: index=" << sphere.index << ", gen=" << sphere.generation << std::endl;

    // Store the old handle
    MeshHandle oldSphere = sphere;

    // Destroy the sphere
    meshMgr.Destroy(sphere);
    std::cout << "  Destroyed sphere mesh" << std::endl;

    // Try to access with old handle (safely returns nullptr)
    MeshData* invalidData = meshMgr.Get(oldSphere);
    if (!invalidData) {
        std::cout << "  ✓ Old handle correctly returns nullptr (generation mismatch)" << std::endl;
    }

    // Create a new sphere (reuses the index)
    MeshHandle newSphere = meshMgr.CreateSphere(16);
    std::cout << "  New sphere handle: index=" << newSphere.index << ", gen=" << newSphere.generation << std::endl;

    // Old handle still doesn't work (generation counter protects us)
    invalidData = meshMgr.Get(oldSphere);
    if (!invalidData) {
        std::cout << "  ✓ Old handle still invalid even though index was reused" << std::endl;
    }

    // New handle works fine
    MeshData* newSphereData = meshMgr.Get(newSphere);
    if (newSphereData) {
        std::cout << "  ✓ New handle works correctly: " << newSphereData->vertexCount << " vertices" << std::endl;
    }
    std::cout << std::endl;

    // Resource count tracking
    std::cout << "[4] Resource Management" << std::endl;
    std::cout << "  Active resources: " << meshMgr.Count() << std::endl;

    meshMgr.Destroy(cube);
    meshMgr.Destroy(plane);
    std::cout << "  After destroying cube and plane: " << meshMgr.Count() << " active resources" << std::endl;
    std::cout << std::endl;

    // Type safety demonstration
    std::cout << "[5] Type Safety" << std::endl;
    [[maybe_unused]] MeshHandle mesh = MeshHandle::Invalid;
    [[maybe_unused]] TextureHandle texture = TextureHandle::Invalid;
    [[maybe_unused]] MaterialHandle material = MaterialHandle::Invalid;

    std::cout << "  ✓ MeshHandle, TextureHandle, and MaterialHandle are distinct types" << std::endl;
    std::cout << "  ✓ Compiler prevents mixing handle types (compile-time safety)" << std::endl;
    std::cout << std::endl;

    std::cout << "======================================" << std::endl;
    std::cout << "Resource handle system demonstration complete!" << std::endl;
    std::cout << "======================================" << std::endl;

    return 0;
}
