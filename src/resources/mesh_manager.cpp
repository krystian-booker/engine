#include "mesh_manager.h"
#include <iostream>
#include <cmath>

// Use GLM's pi constant
#include <glm/gtc/constants.hpp>

std::unique_ptr<MeshData> MeshManager::LoadResource(const std::string& filepath) {
    std::cout << "Loading mesh: " << filepath << std::endl;

    // TODO: Implement actual mesh loading (GLTF, OBJ, FBX via Assimp)
    // For now, just return empty mesh
    auto mesh = std::make_unique<MeshData>();

    std::cerr << "Mesh loading not implemented yet!" << std::endl;

    return mesh;
}

MeshHandle MeshManager::CreateCube() {
    auto mesh = std::make_unique<MeshData>();

    // Cube vertices (position only for simplicity)
    // 8 vertices, 36 indices (12 triangles, 2 per face)
    mesh->vertices = {
        // Positions (x, y, z)
        // Front face
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        // Back face
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
    };

    mesh->indices = {
        // Front
        0, 1, 2,  2, 3, 0,
        // Back
        5, 4, 7,  7, 6, 5,
        // Left
        4, 0, 3,  3, 7, 4,
        // Right
        1, 5, 6,  6, 2, 1,
        // Top
        3, 2, 6,  6, 7, 3,
        // Bottom
        4, 5, 1,  1, 0, 4,
    };

    mesh->vertexCount = 8;
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

            mesh->vertices.push_back(x);
            mesh->vertices.push_back(y);
            mesh->vertices.push_back(z);
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

    mesh->vertexCount = rings * sectors;
    mesh->indexCount = static_cast<u32>(mesh->indices.size());
    mesh->boundsMin = Vec3(-1, -1, -1);
    mesh->boundsMax = Vec3(1, 1, 1);

    return Create(std::move(mesh));
}

MeshHandle MeshManager::CreatePlane() {
    auto mesh = std::make_unique<MeshData>();

    // Plane on XZ plane (Y = 0)
    mesh->vertices = {
        -1.0f, 0.0f, -1.0f,
         1.0f, 0.0f, -1.0f,
         1.0f, 0.0f,  1.0f,
        -1.0f, 0.0f,  1.0f,
    };

    mesh->indices = {0, 1, 2,  2, 3, 0};

    mesh->vertexCount = 4;
    mesh->indexCount = 6;
    mesh->boundsMin = Vec3(-1, 0, -1);
    mesh->boundsMax = Vec3(1, 0, 1);

    return Create(std::move(mesh));
}

MeshHandle MeshManager::CreateQuad() {
    auto mesh = std::make_unique<MeshData>();

    // Quad on XY plane (Z = 0), for UI/sprites
    mesh->vertices = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f,
    };

    mesh->indices = {0, 1, 2,  2, 3, 0};

    mesh->vertexCount = 4;
    mesh->indexCount = 6;
    mesh->boundsMin = Vec3(-0.5f, -0.5f, 0);
    mesh->boundsMax = Vec3(0.5f, 0.5f, 0);

    return Create(std::move(mesh));
}
