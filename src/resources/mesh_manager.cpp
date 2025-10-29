#include "mesh_manager.h"
#include "core/material_data.h"
#include "resources/texture_manager.h"
#include "resources/material_manager.h"
#include "resources/material_converter.h"
#include <iostream>
#include <cmath>
#include <cfloat>
#include <filesystem>

// Use GLM's pi constant
#include <glm/gtc/constants.hpp>

// Assimp includes
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

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

TextureHandle MeshManager::LoadTextureFromAssimp(
    const aiScene* scene,
    const aiString& texturePath,
    const std::string& basePath,
    const std::string& debugNamePrefix,
    const TextureLoadOptions& options)
{
    if (texturePath.length == 0) {
        return TextureHandle::Invalid;
    }

    std::string pathStr(texturePath.C_Str());

    // Check if this is an embedded texture (path starts with '*')
    if (pathStr[0] == '*') {
        // Parse embedded texture index
        u32 texIndex = std::atoi(pathStr.c_str() + 1);  // Skip '*' and parse index

        if (texIndex >= scene->mNumTextures) {
            std::cerr << "Embedded texture index " << texIndex << " out of range (max: "
                      << scene->mNumTextures << ")" << std::endl;
            return TextureHandle::Invalid;
        }

        const aiTexture* aiTex = scene->mTextures[texIndex];

        // Generate unique debug name for this embedded texture
        std::string debugName = debugNamePrefix + std::to_string(texIndex);

        // Check if this is compressed (PNG/JPG) or raw (RGBA8888)
        if (aiTex->mHeight == 0) {
            // Compressed format: mWidth contains byte size, pcData contains compressed data
            size_t bufferSize = aiTex->mWidth;
            const u8* buffer = reinterpret_cast<const u8*>(aiTex->pcData);

            std::cout << "Loading embedded texture (compressed) #" << texIndex
                      << ", size: " << bufferSize << " bytes" << std::endl;

            return TextureManager::Instance().LoadFromMemory(buffer, bufferSize, debugName, options);
        } else {
            // Raw RGBA8888 format: mWidth x mHeight, pcData is aiTexel array (BGRA)
            u32 width = aiTex->mWidth;
            u32 height = aiTex->mHeight;

            std::cout << "Loading embedded texture (raw) #" << texIndex
                      << ", dimensions: " << width << "x" << height << std::endl;

            // aiTexel is BGRA format, need to convert to RGBA
            // Use ImageLoader::CreateImageFromRawData with isBGRA=true
            const u8* rawData = reinterpret_cast<const u8*>(aiTex->pcData);
            ImageData imageData = ImageLoader::CreateImageFromRawData(rawData, width, height, 4, true);

            if (!imageData.IsValid()) {
                std::cerr << "Failed to convert raw embedded texture data" << std::endl;
                return TextureHandle::Invalid;
            }

            // Create TextureData from ImageData
            auto textureData = std::make_unique<TextureData>();
            textureData->pixels = imageData.pixels;
            textureData->width = imageData.width;
            textureData->height = imageData.height;
            textureData->channels = imageData.channels;
            textureData->usage = options.usage;
            textureData->type = options.type;
            textureData->formatOverride = options.formatOverride;
            textureData->flags = options.flags;
            textureData->compressionHint = options.compressionHint;
            textureData->samplerSettings = options.samplerSettings;
            textureData->sourcePaths.push_back(debugName);

            // Calculate mip levels if needed
            if (HasFlag(options.flags, TextureFlags::GenerateMipmaps)) {
                u32 maxDim = std::max(width, height);
                textureData->mipLevels = static_cast<u32>(std::floor(std::log2(maxDim))) + 1;
            } else {
                textureData->mipLevels = 1;
            }

            textureData->mipmapPolicy = options.mipmapPolicy;
            textureData->qualityHint = options.qualityHint;

            return TextureManager::Instance().Create(std::move(textureData));
        }
    } else {
        // External texture: resolve path relative to model file
        std::filesystem::path fullPath = std::filesystem::path(basePath) / pathStr;

        std::cout << "Loading external texture: " << fullPath.string() << std::endl;

        return TextureManager::Instance().Load(fullPath.string(), options);
    }
}

MaterialData MeshManager::ExtractMaterialFromAssimp(
    const aiMaterial* aiMat,
    const aiScene* scene,
    const std::string& basePath,
    const std::string& debugNamePrefix)
{
    MaterialData material;

    // Extract base color / diffuse
    aiColor3D diffuseColor(1.0f, 1.0f, 1.0f);
    aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
    material.albedoTint = Vec4(diffuseColor.r, diffuseColor.g, diffuseColor.b, 1.0f);

    // Check for opacity/transparency
    f32 opacity = 1.0f;
    if (aiMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
        material.albedoTint.a = opacity;
        if (opacity < 1.0f) {
            material.flags |= MaterialFlags::AlphaBlend;
        }
    }

    // Extract emissive
    aiColor3D emissiveColor(0.0f, 0.0f, 0.0f);
    if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS) {
        material.emissiveFactor = Vec4(emissiveColor.r, emissiveColor.g, emissiveColor.b, 1.0f);
    }

    // Try to extract PBR properties (Metallic/Roughness workflow)
    f32 metallic = 0.0f;
    f32 roughness = 0.5f;
    bool hasPBRWorkflow = false;

    if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
        material.metallicFactor = metallic;
        hasPBRWorkflow = true;
    }

    if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
        material.roughnessFactor = roughness;
        hasPBRWorkflow = true;
    }

    // If PBR workflow not found, try Specular/Glossiness workflow and convert
    if (!hasPBRWorkflow) {
        aiColor3D specularColor(0.0f, 0.0f, 0.0f);
        f32 glossiness = 0.5f;
        bool hasSpecGloss = false;

        if (aiMat->Get(AI_MATKEY_COLOR_SPECULAR, specularColor) == AI_SUCCESS) {
            hasSpecGloss = true;
        }

        if (aiMat->Get(AI_MATKEY_GLOSSINESS_FACTOR, glossiness) == AI_SUCCESS ||
            aiMat->Get(AI_MATKEY_SHININESS, glossiness) == AI_SUCCESS) {
            // Normalize shininess to [0, 1] if needed (some exporters use [0, 100])
            if (glossiness > 1.0f) {
                glossiness = glossiness / 100.0f;
            }
            hasSpecGloss = true;
        }

        if (hasSpecGloss) {
            Vec3 diffuse(diffuseColor.r, diffuseColor.g, diffuseColor.b);
            Vec3 specular(specularColor.r, specularColor.g, specularColor.b);

            // Convert using MaterialConverter
            MaterialConverter::ConversionResult conversion;
            if (specular.r > 0.0f || specular.g > 0.0f || specular.b > 0.0f) {
                conversion = MaterialConverter::ConvertSpecGlossToMetalRough(diffuse, specular, glossiness);
            } else {
                conversion = MaterialConverter::ConvertGlossinessOnly(diffuse, glossiness);
            }

            // Apply converted values
            material.albedoTint = Vec4(conversion.baseColor, material.albedoTint.a);
            material.metallicFactor = conversion.metallic;
            material.roughnessFactor = conversion.roughness;

            std::cout << "Converted Spec/Gloss to Metal/Rough: "
                      << "metallic=" << material.metallicFactor << ", roughness=" << material.roughnessFactor << std::endl;
        }
    }

    // Extract two-sided flag
    i32 twoSided = 0;
    if (aiMat->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS && twoSided != 0) {
        material.flags |= MaterialFlags::DoubleSided;
    }

    // Load textures with appropriate options
    aiString texPath;

    // Albedo/Diffuse texture
    if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS ||
        aiMat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) {
        material.albedo = LoadTextureFromAssimp(scene, texPath, basePath, debugNamePrefix + "_diffuse", TextureLoadOptions::Albedo());
    }

    // Normal map
    if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS) {
        material.normal = LoadTextureFromAssimp(scene, texPath, basePath, debugNamePrefix + "_normal", TextureLoadOptions::Normal());
    }

    // Metallic/Roughness textures
    // Try packed texture first (glTF/FBX may pack them)
    if (aiMat->GetTexture(aiTextureType_UNKNOWN, 0, &texPath) == AI_SUCCESS) {
        // Assume packed format (R=roughness, G=metalness, B=AO)
        material.metalRough = LoadTextureFromAssimp(scene, texPath, basePath, debugNamePrefix + "_metalrough", TextureLoadOptions::PackedPBR());
    } else {
        // Try separate textures
        if (aiMat->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS ||
            aiMat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texPath) == AI_SUCCESS) {
            // If we have either metalness or roughness, assume they're packed or we'll create a packed texture
            material.metalRough = LoadTextureFromAssimp(scene, texPath, basePath, debugNamePrefix + "_metalrough", TextureLoadOptions::PackedPBR());
        } else if (aiMat->GetTexture(aiTextureType_SPECULAR, 0, &texPath) == AI_SUCCESS) {
            // Specular/Glossiness texture - treat as packed PBR after conversion
            material.metalRough = LoadTextureFromAssimp(scene, texPath, basePath, debugNamePrefix + "_specgloss", TextureLoadOptions::PackedPBR());
        }
    }

    // Ambient Occlusion
    if (aiMat->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &texPath) == AI_SUCCESS ||
        aiMat->GetTexture(aiTextureType_LIGHTMAP, 0, &texPath) == AI_SUCCESS) {
        // AO is linear, single channel
        TextureLoadOptions aoOptions;
        aoOptions.usage = TextureUsage::Generic;
        aoOptions.flags = TextureFlags::GenerateMipmaps;
        material.ao = LoadTextureFromAssimp(scene, texPath, basePath, debugNamePrefix + "_ao", aoOptions);
    }

    // Emissive
    if (aiMat->GetTexture(aiTextureType_EMISSIVE, 0, &texPath) == AI_SUCCESS) {
        material.emissive = LoadTextureFromAssimp(scene, texPath, basePath, debugNamePrefix + "_emissive", TextureLoadOptions::Albedo());
    }

    return material;
}

MeshLoadResult MeshManager::ProcessMeshWithMaterial(
    const aiMesh* mesh,
    const aiScene* scene,
    const std::string& basePath,
    const std::string& debugNamePrefix)
{
    MeshLoadResult result;

    // Process mesh geometry
    std::unique_ptr<MeshData> meshData = ProcessMesh(mesh, scene);
    if (!meshData) {
        std::cerr << "Failed to process mesh geometry" << std::endl;
        return result;
    }

    // Create mesh handle
    result.mesh = Create(std::move(meshData));

    // Extract and create material if mesh has one
    if (mesh->mMaterialIndex < scene->mNumMaterials) {
        const aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];

        // Extract material data from Assimp
        MaterialData materialData = ExtractMaterialFromAssimp(aiMat, scene, basePath, debugNamePrefix);

        // Get material name from Assimp for debugging
        aiString matName;
        std::string materialDebugName = debugNamePrefix;
        if (aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS && matName.length > 0) {
            materialDebugName = std::string(matName.C_Str());
        }

        // Get or create material (with caching by content hash)
        result.material = MaterialManager::Instance().GetOrCreate(materialData, materialDebugName);
    } else {
        // No material, use default
        result.material = MaterialManager::Instance().GetDefaultMaterial();
    }

    return result;
}

MeshLoadResult MeshManager::LoadWithMaterial(const std::string& filepath) {
    std::cout << "Loading mesh with material: " << filepath << std::endl;

    MeshLoadResult result;

    // Check if this is a sub-mesh request (format: "path/to/file.obj#0")
    size_t hashPos = filepath.find('#');
    std::string actualPath = filepath;
    int subMeshIndex = -1;

    if (hashPos != std::string::npos) {
        actualPath = filepath.substr(0, hashPos);
        subMeshIndex = std::stoi(filepath.substr(hashPos + 1));
    }

    // Extract base path for texture resolution
    std::filesystem::path fsPath(actualPath);
    std::string basePath = fsPath.parent_path().string();
    std::string fileName = fsPath.filename().string();

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
        return result;
    }

    // If loading a specific sub-mesh, process it directly
    if (subMeshIndex >= 0) {
        if (subMeshIndex < static_cast<int>(scene->mNumMeshes)) {
            std::string debugPrefix = fileName + "#" + std::to_string(subMeshIndex);
            result = ProcessMeshWithMaterial(scene->mMeshes[subMeshIndex], scene, basePath, debugPrefix);
        } else {
            std::cerr << "Sub-mesh index " << subMeshIndex << " out of range (max: " << scene->mNumMeshes << ")" << std::endl;
        }
        return result;
    }

    // Single mesh: load directly with material
    if (scene->mNumMeshes == 1) {
        result = ProcessMeshWithMaterial(scene->mMeshes[0], scene, basePath, fileName);
        return result;
    }

    // Multiple meshes: create parent placeholder and populate submeshes
    auto parentMesh = std::make_unique<MeshData>();
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        std::string subMeshPath = actualPath + "#" + std::to_string(i);
        parentMesh->subMeshPaths.push_back(subMeshPath);

        // Also process each submesh and add to result
        std::string debugPrefix = fileName + "#" + std::to_string(i);
        MeshLoadResult subResult = ProcessMeshWithMaterial(scene->mMeshes[i], scene, basePath, debugPrefix);
        result.subMeshes.push_back(subResult);
    }

    // Create placeholder parent mesh handle
    result.mesh = Create(std::move(parentMesh));

    std::cout << "Multi-mesh file detected with " << scene->mNumMeshes << " meshes" << std::endl;
    return result;
}
