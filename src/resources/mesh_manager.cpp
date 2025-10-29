#include "mesh_manager.h"
#include <iostream>
#include <cmath>
#include <cfloat>

// Use GLM's pi constant
#include <glm/gtc/constants.hpp>

// Assimp includes
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

std::unique_ptr<MeshData> MeshManager::LoadResource(const std::string& filepath) {
    std::cout << "Loading mesh: " << filepath << std::endl;

    // Check if this is a sub-mesh request (format: "path/to/file.obj#0")
    size_t hashPos = filepath.find('#');
    std::string actualPath = filepath;
    int subMeshIndex = -1;

    if (hashPos != std::string::npos) {
        actualPath = filepath.substr(0, hashPos);
        subMeshIndex = std::stoi(filepath.substr(hashPos + 1));
    }

    // Load scene with Assimp
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(actualPath,
        aiProcess_Triangulate |           // Convert all primitives to triangles
        aiProcess_CalcTangentSpace |      // Generate tangents and bitangents
        aiProcess_GenSmoothNormals |      // Generate smooth normals if missing
        aiProcess_JoinIdenticalVertices | // Optimize vertex buffer
        aiProcess_ImproveCacheLocality |  // Optimize for GPU cache
        aiProcess_FlipUVs);               // Flip Y coordinate for Vulkan

    // Error handling
    if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
        std::cerr << "Assimp error loading '" << actualPath << "': " << importer.GetErrorString() << std::endl;
        return std::make_unique<MeshData>();
    }

    // If loading a specific sub-mesh, process it directly
    if (subMeshIndex >= 0) {
        if (subMeshIndex < static_cast<int>(scene->mNumMeshes)) {
            return ProcessMesh(scene->mMeshes[subMeshIndex], scene);
        } else {
            std::cerr << "Sub-mesh index " << subMeshIndex << " out of range (max: " << scene->mNumMeshes << ")" << std::endl;
            return std::make_unique<MeshData>();
        }
    }

    // Single mesh: load directly
    if (scene->mNumMeshes == 1) {
        return ProcessMesh(scene->mMeshes[0], scene);
    }

    // Multiple meshes: create placeholder parent with sub-mesh paths
    auto parentMesh = std::make_unique<MeshData>();
    parentMesh->subMeshPaths.reserve(scene->mNumMeshes);
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        parentMesh->subMeshPaths.push_back(actualPath + "#" + std::to_string(i));
    }

    std::cout << "Multi-mesh file detected with " << scene->mNumMeshes << " meshes" << std::endl;
    return parentMesh;
}

std::unique_ptr<MeshData> MeshManager::ProcessMesh(const aiMesh* mesh, const aiScene* scene) {
    (void)scene;  // Currently unused, but may be needed for materials later
    auto meshData = std::make_unique<MeshData>();

    // Reserve space
    meshData->vertices.reserve(mesh->mNumVertices);
    meshData->indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);

    Vec3 boundsMin(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 boundsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    // Process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex vertex{};

        // Position (required)
        vertex.position = Vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        boundsMin = glm::min(boundsMin, vertex.position);
        boundsMax = glm::max(boundsMax, vertex.position);

        // Normal (required, CalcTangentSpace ensures it exists)
        if (mesh->mNormals) {
            vertex.normal = Vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        } else {
            vertex.normal = Vec3(0.0f, 1.0f, 0.0f);  // Default normal
        }

        // Tangent + Bitangent → Vec4 with handedness
        if (mesh->mTangents && mesh->mBitangents) {
            Vec3 tangent(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
            Vec3 bitangent(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);

            // Calculate handedness: sign(dot(cross(normal, tangent), bitangent))
            f32 handedness = glm::dot(glm::cross(vertex.normal, tangent), bitangent) > 0.0f ? 1.0f : -1.0f;
            vertex.tangent = Vec4(tangent, handedness);
        } else {
            // Default tangent along X axis with positive handedness
            vertex.tangent = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
        }

        // Texture coordinates (use first channel, default to (0,0) if missing)
        if (mesh->mTextureCoords[0]) {
            vertex.texCoord = Vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        } else {
            vertex.texCoord = Vec2(0.0f, 0.0f);
        }

        meshData->vertices.push_back(vertex);
    }

    // Process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            meshData->indices.push_back(face.mIndices[j]);
        }
    }

    meshData->vertexCount = mesh->mNumVertices;
    meshData->indexCount = static_cast<u32>(meshData->indices.size());
    meshData->boundsMin = boundsMin;
    meshData->boundsMax = boundsMax;

    std::cout << "Loaded mesh: " << meshData->vertexCount << " vertices, "
              << meshData->indexCount << " indices" << std::endl;

    return meshData;
}

MeshHandle MeshManager::CreateCube() {
    auto mesh = std::make_unique<MeshData>();

    // Cube with per-face normals and tangents
    // Tangent.xyz points in U direction, tangent.w = handedness for bitangent calculation
    mesh->vertices = {
        // Front face (+Z), tangent points right (+X)
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

        // Back face (-Z), tangent points left (-X)
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

        // Left face (-X), tangent points forward (+Z)
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

        // Right face (+X), tangent points backward (-Z)
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {0.0f, 1.0f}},

        // Top face (+Y), tangent points right (+X)
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

        // Bottom face (-Y), tangent points right (+X)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    };

    mesh->indices = {
        // Front (0-3)
        0, 1, 2,  2, 3, 0,
        // Back (4-7)
        4, 5, 6,  6, 7, 4,
        // Left (8-11)
        8, 9, 10,  10, 11, 8,
        // Right (12-15)
        12, 13, 14,  14, 15, 12,
        // Top (16-19)
        16, 17, 18,  18, 19, 16,
        // Bottom (20-23)
        20, 21, 22,  22, 23, 20,
    };

    mesh->vertexCount = static_cast<u32>(mesh->vertices.size());
    mesh->indexCount = 36;
    mesh->boundsMin = Vec3(-0.5f, -0.5f, -0.5f);
    mesh->boundsMax = Vec3(0.5f, 0.5f, 0.5f);

    return Create(std::move(mesh));
}

MeshHandle MeshManager::CreateSphere(u32 segments) {
    auto mesh = std::make_unique<MeshData>();

    // UV sphere generation
    u32 rings = segments / 2;
    u32 sectors = segments;

    f32 R = 1.0f / (f32)(rings - 1);
    f32 S = 1.0f / (f32)(sectors - 1);

    const f32 pi = glm::pi<f32>();

    // Generate vertices
    for (u32 r = 0; r < rings; ++r) {
        for (u32 s = 0; s < sectors; ++s) {
            f32 y = std::sin(-pi / 2.0f + pi * r * R);
            f32 x = std::cos(2 * pi * s * S) * std::sin(pi * r * R);
            f32 z = std::sin(2 * pi * s * S) * std::sin(pi * r * R);

            Vertex vertex{};
            vertex.position = Vec3(x, y, z);
            vertex.normal = Normalize(Vec3(x, y, z));

            // Calculate tangent: perpendicular to normal in the horizontal plane
            // Tangent follows longitude lines (U direction)
            f32 theta = 2 * pi * s * S;
            Vec3 tangent = Normalize(Vec3(-std::sin(theta), 0.0f, std::cos(theta)));
            vertex.tangent = Vec4(tangent, 1.0f);

            vertex.texCoord = Vec2(s * S, r * R);
            mesh->vertices.push_back(vertex);
        }
    }

    // Generate indices
    for (u32 r = 0; r < rings - 1; ++r) {
        for (u32 s = 0; s < sectors - 1; ++s) {
            u32 curRow = r * sectors;
            u32 nextRow = (r + 1) * sectors;

            mesh->indices.push_back(curRow + s);
            mesh->indices.push_back(nextRow + s);
            mesh->indices.push_back(nextRow + (s + 1));

            mesh->indices.push_back(curRow + s);
            mesh->indices.push_back(nextRow + (s + 1));
            mesh->indices.push_back(curRow + (s + 1));
        }
    }

    mesh->vertexCount = static_cast<u32>(mesh->vertices.size());
    mesh->indexCount = static_cast<u32>(mesh->indices.size());
    mesh->boundsMin = Vec3(-1, -1, -1);
    mesh->boundsMax = Vec3(1, 1, 1);

    return Create(std::move(mesh));
}

MeshHandle MeshManager::CreatePlane() {
    auto mesh = std::make_unique<MeshData>();

    // Plane on XZ plane (Y = 0), normal points up, tangent points right
    mesh->vertices = {
        {{-1.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 1.0f, 0.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f, 0.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    };

    mesh->indices = {0, 1, 2,  2, 3, 0};

    mesh->vertexCount = static_cast<u32>(mesh->vertices.size());
    mesh->indexCount = 6;
    mesh->boundsMin = Vec3(-1, 0, -1);
    mesh->boundsMax = Vec3(1, 0, 1);

    return Create(std::move(mesh));
}

MeshHandle MeshManager::CreateQuad() {
    auto mesh = std::make_unique<MeshData>();

    // Quad on XY plane (Z = 0), for UI/sprites, normal points toward camera, tangent points right
    mesh->vertices = {
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    };

    mesh->indices = {0, 1, 2,  2, 3, 0};

    mesh->vertexCount = static_cast<u32>(mesh->vertices.size());
    mesh->indexCount = 6;
    mesh->boundsMin = Vec3(-0.5f, -0.5f, 0);
    mesh->boundsMax = Vec3(0.5f, 0.5f, 0);

    return Create(std::move(mesh));
}
