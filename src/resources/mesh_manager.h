#pragma once
#include "core/resource_manager.h"
#include "core/math.h"
#include "core/texture_load_options.h"
#include "renderer/vertex.h"
#include "renderer/vulkan_mesh.h"
#include <vector>

// Forward declarations for Assimp types (avoid including assimp headers in header file)
struct aiMesh;
struct aiScene;
struct aiMaterial;
struct aiString;

// Result of mesh loading (includes mesh and its associated material)
struct MeshLoadResult {
    MeshHandle mesh = MeshHandle::Invalid;
    MaterialHandle material = MaterialHandle::Invalid;

    // For multi-mesh files, submeshes with their materials
    std::vector<MeshLoadResult> subMeshes;

    bool IsValid() const { return mesh.IsValid(); }
    bool HasSubMeshes() const { return !subMeshes.empty(); }
};

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

    // Load mesh with material extraction from file (new API)
    // Returns MeshLoadResult with mesh and material handles
    // For multi-mesh files, result.subMeshes will be populated
    MeshLoadResult LoadWithMaterial(const std::string& filepath);

    // Legacy: Load mesh only (without material extraction)
    // Uses base ResourceManager::Load(), doesn't extract materials
    MeshHandle LoadMeshOnly(const std::string& filepath) {
        return ResourceManager::Load(filepath);
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

    // Helper to process a single Assimp mesh (geometry only, no material)
    std::unique_ptr<MeshData> ProcessMesh(const aiMesh* mesh, const aiScene* scene);

    // Helper to process a single Assimp mesh with material extraction
    // Returns MeshLoadResult with both mesh and material handles
    MeshLoadResult ProcessMeshWithMaterial(
        const aiMesh* mesh,
        const aiScene* scene,
        const std::string& basePath,
        const std::string& debugNamePrefix);

    // Helper to load texture from Assimp (handles embedded and external paths)
    // Returns TextureHandle (Invalid if texture couldn't be loaded)
    // basePath: Directory containing the FBX/model file (for resolving relative external paths)
    // debugNamePrefix: Used for naming embedded textures (e.g., "Avocado.fbx:embedded_")
    TextureHandle LoadTextureFromAssimp(
        const aiScene* scene,
        const aiString& texturePath,
        const std::string& basePath,
        const std::string& debugNamePrefix,
        const TextureLoadOptions& options);

    // Helper to extract material data from Assimp material
    // Returns MaterialData populated with PBR properties and textures
    // Converts Specular/Glossiness workflow to Metallic/Roughness if needed
    MaterialData ExtractMaterialFromAssimp(
        const aiMaterial* aiMat,
        const aiScene* scene,
        const std::string& basePath,
        const std::string& debugNamePrefix);
};
