#include "material_manager.h"
#include "texture_manager.h"
#include "renderer/vulkan_material_buffer.h"
#include "core/texture_load_options.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

MaterialManager::MaterialManager()
    : m_DefaultMaterial(MaterialHandle::Invalid)
    , m_GPUBuffer(nullptr)
    , m_VulkanContext(nullptr)
    , m_NextGPUMaterialIndex(0)
{
}

MaterialManager::~MaterialManager() {
    ShutdownGPUBuffer();
}

void MaterialManager::InitGPUBuffer(VulkanContext* context) {
    ENGINE_ASSERT(context != nullptr);
    m_VulkanContext = context;

    // GPU buffer will be created lazily when first material is uploaded
}

void MaterialManager::ShutdownGPUBuffer() {
    m_GPUBuffer.reset();
    m_VulkanContext = nullptr;
}

MaterialHandle MaterialManager::Load(const std::string& filepath) {
    // Use base class Load which handles caching
    return ResourceManager::Load(filepath);
}

MaterialHandle MaterialManager::CreateDefaultMaterial() {
    // Check if already created
    if (m_DefaultMaterial.IsValid()) {
        return m_DefaultMaterial;
    }

    // Create default material with procedural textures
    auto material = std::make_unique<MaterialData>();

    // Get TextureManager instance
    TextureManager& texMgr = TextureManager::Instance();

    // Assign default textures
    material->albedo = texMgr.CreateWhite();
    material->normal = texMgr.CreateNormalMap();
    material->metalRough = texMgr.CreateMetalRough();  // Mid roughness, low metallic
    material->ao = texMgr.CreateWhite();               // No occlusion
    material->emissive = texMgr.CreateBlack();         // No emission

    // Default PBR parameters (already set by MaterialData constructor)
    material->albedoTint = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    material->emissiveFactor = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    material->metallicFactor = 0.0f;
    material->roughnessFactor = 0.5f;
    material->normalScale = 1.0f;
    material->aoStrength = 1.0f;
    material->flags = MaterialFlags::None;

    // Upload to GPU and store index
    material->gpuMaterialIndex = UploadMaterialToGPU(*material);

    // Store in resource manager
    m_DefaultMaterial = Create(std::move(material));

    return m_DefaultMaterial;
}

std::unique_ptr<MaterialData> MaterialManager::LoadResource(const std::string& filepath) {
    return LoadMaterialFromJSON(filepath);
}

std::unique_ptr<MaterialData> MaterialManager::LoadMaterialFromJSON(const std::string& filepath) {
    // Open and parse JSON file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "MaterialManager: Failed to open material file: " << filepath << std::endl;
        return nullptr;
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        std::cerr << "MaterialManager: JSON parse error in " << filepath << ": " << e.what() << std::endl;
        return nullptr;
    }

    auto material = std::make_unique<MaterialData>();
    TextureManager& texMgr = TextureManager::Instance();

    // Load textures with auto-detected options
    if (j.contains("albedo") && j["albedo"].is_string()) {
        std::string texPath = j["albedo"].get<std::string>();
        material->albedo = texMgr.Load(texPath, TextureLoadOptions::Albedo());
    } else {
        material->albedo = texMgr.CreateWhite();
    }

    if (j.contains("normal") && j["normal"].is_string()) {
        std::string texPath = j["normal"].get<std::string>();
        material->normal = texMgr.Load(texPath, TextureLoadOptions::Normal());
    } else {
        material->normal = texMgr.CreateNormalMap();
    }

    if (j.contains("metalRough") && j["metalRough"].is_string()) {
        std::string texPath = j["metalRough"].get<std::string>();
        material->metalRough = texMgr.Load(texPath, TextureLoadOptions::PackedPBR());
    } else {
        // Use fallback metal-rough texture (rough=1.0, metal=0.5)
        material->metalRough = texMgr.CreateMetalRough();
    }

    if (j.contains("ao") && j["ao"].is_string()) {
        std::string texPath = j["ao"].get<std::string>();
        material->ao = texMgr.Load(texPath, TextureLoadOptions::AO());
    } else {
        material->ao = texMgr.CreateWhite();
    }

    if (j.contains("emissive") && j["emissive"].is_string()) {
        std::string texPath = j["emissive"].get<std::string>();
        material->emissive = texMgr.Load(texPath, TextureLoadOptions::Albedo());  // Emissive is sRGB
    } else {
        material->emissive = texMgr.CreateBlack();
    }

    // Load PBR parameters
    if (j.contains("albedoTint") && j["albedoTint"].is_array() && j["albedoTint"].size() >= 3) {
        auto& arr = j["albedoTint"];
        material->albedoTint = Vec4(
            arr[0].get<f32>(),
            arr[1].get<f32>(),
            arr[2].get<f32>(),
            arr.size() >= 4 ? arr[3].get<f32>() : 1.0f
        );
    }

    if (j.contains("emissiveFactor") && j["emissiveFactor"].is_array() && j["emissiveFactor"].size() >= 3) {
        auto& arr = j["emissiveFactor"];
        material->emissiveFactor = Vec4(
            arr[0].get<f32>(),
            arr[1].get<f32>(),
            arr[2].get<f32>(),
            arr.size() >= 4 ? arr[3].get<f32>() : 1.0f
        );
    }

    if (j.contains("metallicFactor") && j["metallicFactor"].is_number()) {
        material->metallicFactor = j["metallicFactor"].get<f32>();
    }

    if (j.contains("roughnessFactor") && j["roughnessFactor"].is_number()) {
        material->roughnessFactor = j["roughnessFactor"].get<f32>();
    }

    if (j.contains("normalScale") && j["normalScale"].is_number()) {
        material->normalScale = j["normalScale"].get<f32>();
    }

    if (j.contains("aoStrength") && j["aoStrength"].is_number()) {
        material->aoStrength = j["aoStrength"].get<f32>();
    }

    // Parse flags
    material->flags = MaterialFlags::None;
    if (j.contains("flags") && j["flags"].is_array()) {
        for (const auto& flagStr : j["flags"]) {
            if (!flagStr.is_string()) continue;

            std::string flag = flagStr.get<std::string>();
            if (flag == "doubleSided") {
                SetFlag(material->flags, MaterialFlags::DoubleSided);
            } else if (flag == "alphaBlend") {
                SetFlag(material->flags, MaterialFlags::AlphaBlend);
            } else if (flag == "alphaMask") {
                SetFlag(material->flags, MaterialFlags::AlphaMask);
            } else if (flag == "alphaTest") {
                SetFlag(material->flags, MaterialFlags::AlphaTest);
            }
        }
    }

    // Upload to GPU and store index
    material->gpuMaterialIndex = UploadMaterialToGPU(*material);

    std::cout << "MaterialManager: Loaded material from " << filepath << std::endl;

    return material;
}

TextureLoadOptions MaterialManager::InferTextureOptions(const std::string& slotName) const {
    if (slotName == "albedo" || slotName == "baseColor") {
        return TextureLoadOptions::Albedo();
    } else if (slotName == "normal") {
        return TextureLoadOptions::Normal();
    } else if (slotName == "metalRough" || slotName == "metallicRoughness" || slotName == "packedPBR") {
        return TextureLoadOptions::PackedPBR();
    } else if (slotName == "roughness") {
        return TextureLoadOptions::Roughness();
    } else if (slotName == "metalness" || slotName == "metallic") {
        return TextureLoadOptions::Metalness();
    } else if (slotName == "ao" || slotName == "ambientOcclusion") {
        return TextureLoadOptions::AO();
    } else if (slotName == "emissive") {
        return TextureLoadOptions::Albedo();  // Emissive uses sRGB
    }

    // Default fallback
    return TextureLoadOptions();
}

u32 MaterialManager::UploadMaterialToGPU(const MaterialData& /*material*/) {
    // For now, just assign a sequential index
    // Actual GPU buffer upload will be implemented with VulkanMaterialBuffer
    u32 index = m_NextGPUMaterialIndex++;

    // TODO: Upload to GPU buffer once VulkanMaterialBuffer is implemented

    return index;
}

void MaterialManager::UpdateMaterialOnGPU(u32 /*gpuIndex*/, const MaterialData& /*material*/) {
    // TODO: Update GPU buffer once VulkanMaterialBuffer is implemented
}

// Descriptor caching implementation
u32 MaterialManager::EnsureMaterial(MaterialHandle handle) {
    MaterialData* mat = Get(handle);
    if (!mat) {
        std::cerr << "MaterialManager::EnsureMaterial: Invalid handle, returning default material index" << std::endl;
        return 0;  // Return default material index
    }

    // Compute current hash of material state
    u64 currentHash = mat->ComputeDescriptorHash();

    // Check if descriptor set is valid and up-to-date
    if (!mat->descriptorDirty &&
        mat->descriptorSet != VK_NULL_HANDLE &&
        mat->descriptorHash == currentHash) {
        // Cache hit! Descriptor set is still valid
        return mat->gpuMaterialIndex;
    }

    // Cache miss - need to rebuild/update
    std::cout << "MaterialManager: Rebuilding descriptor cache for material (GPU index "
              << mat->gpuMaterialIndex << ")" << std::endl;

    // In hybrid approach, descriptor sets are for optimization only
    // The actual material data is in the SSBO + bindless textures
    // Here we just track that the material state has changed

    // Update the hash to mark cache as valid
    mat->descriptorHash = currentHash;
    mat->descriptorDirty = false;

    // Note: In the hybrid approach (chosen by user), descriptor sets are for
    // internal caching/optimization. The primary path is SSBO + bindless array.
    // This function primarily validates that material data is current.

    return mat->gpuMaterialIndex;
}

void MaterialManager::InvalidateMaterial(MaterialHandle handle) {
    MaterialData* mat = Get(handle);
    if (mat) {
        mat->descriptorDirty = true;
        std::cout << "MaterialManager: Invalidated material (GPU index "
                  << mat->gpuMaterialIndex << ")" << std::endl;
    }
}

void MaterialManager::InvalidateMaterialsUsingTexture(TextureHandle texHandle) {
    u32 invalidatedCount = 0;

    // Iterate through all active materials using protected ForEachResource
    ForEachResource([&](MaterialData& material) {
        // Check if this material uses the texture
        if (material.albedo == texHandle ||
            material.normal == texHandle ||
            material.metalRough == texHandle ||
            material.ao == texHandle ||
            material.emissive == texHandle) {

            material.descriptorDirty = true;
            invalidatedCount++;
        }
    });

    if (invalidatedCount > 0) {
        std::cout << "MaterialManager: Invalidated " << invalidatedCount
                  << " materials using texture (index " << texHandle.index
                  << ", gen " << texHandle.generation << ")" << std::endl;
    }
}
