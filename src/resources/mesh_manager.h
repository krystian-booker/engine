#pragma once
#include "core/resource_manager.h"
#include "core/math.h"
#include <vector>

// Simplified mesh data (will expand for GPU upload)
struct MeshData {
    std::vector<f32> vertices;   // Position (3) + Normal (3) + TexCoord (2) = 8 floats per vertex
    std::vector<u32> indices;
    u32 vertexCount = 0;
    u32 indexCount = 0;

    // Bounding box for culling
    Vec3 boundsMin{0, 0, 0};
    Vec3 boundsMax{0, 0, 0};
};

class MeshManager : public ResourceManager<MeshData, MeshHandle> {
public:
    // Singleton access
    static MeshManager& Instance() {
        static MeshManager instance;
        return instance;
    }

    // Create built-in primitive meshes
    MeshHandle CreateCube();
    MeshHandle CreateSphere(u32 segments = 32);
    MeshHandle CreatePlane();
    MeshHandle CreateQuad();

protected:
    // Override to implement file loading (GLTF, OBJ, etc.)
    std::unique_ptr<MeshData> LoadResource(const std::string& filepath) override;

private:
    MeshManager() = default;
    ~MeshManager() = default;

    // Prevent copying
    MeshManager(const MeshManager&) = delete;
    MeshManager& operator=(const MeshManager&) = delete;
};
