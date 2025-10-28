#pragma once
#include "core/resource_manager.h"
#include "core/math.h"
#include "renderer/vertex.h"
#include "renderer/vulkan_mesh.h"
#include <vector>

// Forward declarations for Assimp types (avoid including assimp headers in header file)
struct aiMesh;
struct aiScene;

// Simplified mesh data (will expand for GPU upload)
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    u32 vertexCount = 0;
    u32 indexCount = 0;

    // GPU resources (populated on-demand by renderer)
    VulkanMesh gpuMesh;
    bool gpuUploaded = false;

    // Bounding box for culling
    Vec3 boundsMin{0, 0, 0};
    Vec3 boundsMax{0, 0, 0};

    // Multi-mesh support: paths to sub-meshes (format: "path/to/file.obj#0", "path/to/file.obj#1", etc.)
    std::vector<std::string> subMeshPaths;

    // Helper to check if this is a multi-mesh placeholder
    bool HasSubMeshes() const { return !subMeshPaths.empty(); }
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

    // Helper to process a single Assimp mesh
    std::unique_ptr<MeshData> ProcessMesh(const aiMesh* mesh, const aiScene* scene);
};
