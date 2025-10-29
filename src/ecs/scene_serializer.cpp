#include "scene_serializer.h"
#include "components/transform.h"
#include "components/renderable.h"
#include "components/camera.h"
#include "components/rotator.h"
#include "components/light.h"
#include "resources/material_manager.h"
#include "resources/mesh_manager.h"
#include "resources/texture_manager.h"
#include "core/material_data.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>

using json = nlohmann::json;

// Helper: Serialize MaterialData to JSON
static json SerializeMaterialData(const MaterialData& material) {
    json materialJson;

    // Serialize texture paths (only if valid)
    auto& texMgr = TextureManager::Instance();
    if (material.albedo.IsValid()) {
        std::string path = texMgr.GetPath(material.albedo);
        if (!path.empty()) {
            materialJson["albedo"] = path;
        }
    }
    if (material.normal.IsValid()) {
        std::string path = texMgr.GetPath(material.normal);
        if (!path.empty()) {
            materialJson["normal"] = path;
        }
    }
    if (material.metalRough.IsValid()) {
        std::string path = texMgr.GetPath(material.metalRough);
        if (!path.empty()) {
            materialJson["metalRough"] = path;
        }
    }
    if (material.ao.IsValid()) {
        std::string path = texMgr.GetPath(material.ao);
        if (!path.empty()) {
            materialJson["ao"] = path;
        }
    }
    if (material.emissive.IsValid()) {
        std::string path = texMgr.GetPath(material.emissive);
        if (!path.empty()) {
            materialJson["emissive"] = path;
        }
    }

    // Serialize PBR parameters
    materialJson["albedoTint"] = {
        {"r", material.albedoTint.r},
        {"g", material.albedoTint.g},
        {"b", material.albedoTint.b},
        {"a", material.albedoTint.a}
    };
    materialJson["emissiveFactor"] = {
        {"r", material.emissiveFactor.r},
        {"g", material.emissiveFactor.g},
        {"b", material.emissiveFactor.b},
        {"a", material.emissiveFactor.a}
    };
    materialJson["metallicFactor"] = material.metallicFactor;
    materialJson["roughnessFactor"] = material.roughnessFactor;
    materialJson["normalScale"] = material.normalScale;
    materialJson["aoStrength"] = material.aoStrength;
    materialJson["flags"] = static_cast<u32>(material.flags);

    return materialJson;
}

// Helper: Deserialize MaterialData from JSON (returns MaterialData, not MaterialHandle)
static MaterialData DeserializeMaterialData(const json& materialJson, const std::string& sceneDir) {
    MaterialData material;

    // Load textures
    auto& texMgr = TextureManager::Instance();

    if (materialJson.contains("albedo") && materialJson["albedo"].is_string()) {
        std::string texPath = materialJson["albedo"].get<std::string>();
        std::filesystem::path path(texPath);
        if (path.is_relative()) {
            texPath = sceneDir + "/" + texPath;
        }
        material.albedo = texMgr.Load(texPath, TextureLoadOptions::Albedo());
    }

    if (materialJson.contains("normal") && materialJson["normal"].is_string()) {
        std::string texPath = materialJson["normal"].get<std::string>();
        std::filesystem::path path(texPath);
        if (path.is_relative()) {
            texPath = sceneDir + "/" + texPath;
        }
        material.normal = texMgr.Load(texPath, TextureLoadOptions::Normal());
    }

    if (materialJson.contains("metalRough") && materialJson["metalRough"].is_string()) {
        std::string texPath = materialJson["metalRough"].get<std::string>();
        std::filesystem::path path(texPath);
        if (path.is_relative()) {
            texPath = sceneDir + "/" + texPath;
        }
        material.metalRough = texMgr.Load(texPath, TextureLoadOptions::PackedPBR());
    }

    if (materialJson.contains("ao") && materialJson["ao"].is_string()) {
        std::string texPath = materialJson["ao"].get<std::string>();
        std::filesystem::path path(texPath);
        if (path.is_relative()) {
            texPath = sceneDir + "/" + texPath;
        }
        material.ao = texMgr.Load(texPath, TextureLoadOptions::AO());
    }

    if (materialJson.contains("emissive") && materialJson["emissive"].is_string()) {
        std::string texPath = materialJson["emissive"].get<std::string>();
        std::filesystem::path path(texPath);
        if (path.is_relative()) {
            texPath = sceneDir + "/" + texPath;
        }
        // Emissive textures don't have a dedicated preset, use default (generic)
        material.emissive = texMgr.Load(texPath, TextureLoadOptions());
    }

    // Load PBR parameters
    if (materialJson.contains("albedoTint")) {
        const auto& tint = materialJson["albedoTint"];
        material.albedoTint = Vec4(
            tint["r"].get<f32>(),
            tint["g"].get<f32>(),
            tint["b"].get<f32>(),
            tint["a"].get<f32>()
        );
    }

    if (materialJson.contains("emissiveFactor")) {
        const auto& emissive = materialJson["emissiveFactor"];
        material.emissiveFactor = Vec4(
            emissive["r"].get<f32>(),
            emissive["g"].get<f32>(),
            emissive["b"].get<f32>(),
            emissive["a"].get<f32>()
        );
    }

    if (materialJson.contains("metallicFactor")) {
        material.metallicFactor = materialJson["metallicFactor"].get<f32>();
    }

    if (materialJson.contains("roughnessFactor")) {
        material.roughnessFactor = materialJson["roughnessFactor"].get<f32>();
    }

    if (materialJson.contains("normalScale")) {
        material.normalScale = materialJson["normalScale"].get<f32>();
    }

    if (materialJson.contains("aoStrength")) {
        material.aoStrength = materialJson["aoStrength"].get<f32>();
    }

    if (materialJson.contains("flags")) {
        material.flags = static_cast<MaterialFlags>(materialJson["flags"].get<u32>());
    }

    return material;
}

bool SceneSerializer::SaveScene(const std::string& filepath) {
    json sceneJson;
    sceneJson["version"] = 1;
    sceneJson["entities"] = json::array();

    auto transforms = m_ECS->GetComponentRegistry()->GetComponentArray<Transform>();

    // Serialize all entities with transforms
    for (size_t i = 0; i < transforms->Size(); ++i) {
        Entity entity = transforms->GetEntity(i);
        const Transform& transform = transforms->Get(entity);

        // Skip editor camera entities (they are not part of the scene)
        if (m_ECS->HasComponent<Camera>(entity)) {
            const Camera& camera = m_ECS->GetComponent<Camera>(entity);
            if (camera.isEditorCamera) {
                continue;  // Don't serialize editor cameras
            }
        }

        json entityJson;
        entityJson["id"] = entity.index;
        entityJson["generation"] = entity.generation;

        // Serialize transform
        json transformJson;
        transformJson["position"] = {
            {"x", transform.localPosition.x},
            {"y", transform.localPosition.y},
            {"z", transform.localPosition.z}
        };
        transformJson["rotation"] = {
            {"w", transform.localRotation.w},
            {"x", transform.localRotation.x},
            {"y", transform.localRotation.y},
            {"z", transform.localRotation.z}
        };
        transformJson["scale"] = {
            {"x", transform.localScale.x},
            {"y", transform.localScale.y},
            {"z", transform.localScale.z}
        };

        entityJson["transform"] = transformJson;

        // Serialize Renderable component
        if (m_ECS->HasComponent<Renderable>(entity)) {
            const Renderable& renderable = m_ECS->GetComponent<Renderable>(entity);
            json renderableJson;

            // Store mesh path (retrieve from MeshManager)
            std::string meshPath;
            if (renderable.mesh.IsValid()) {
                meshPath = MeshManager::Instance().GetPath(renderable.mesh);
                if (!meshPath.empty()) {
                    renderableJson["mesh"] = meshPath;
                }
            }

            // Store material - hybrid approach
            if (renderable.material.IsValid()) {
                std::string materialPath = MaterialManager::Instance().GetPath(renderable.material);

                if (!materialPath.empty()) {
                    // File-based material - just store the path
                    renderableJson["material"] = materialPath;
                } else {
                    // Procedural/embedded material - store inline materialData
                    const MaterialData* materialData = MaterialManager::Instance().Get(renderable.material);
                    if (materialData) {
                        renderableJson["materialData"] = SerializeMaterialData(*materialData);
                    }

                    // Also store meshPath for fallback reload with LoadWithMaterial()
                    if (!meshPath.empty()) {
                        renderableJson["meshPath"] = meshPath;
                    }
                }
            } else if (!meshPath.empty()) {
                // No material but we have a mesh - store meshPath for potential material reload
                renderableJson["meshPath"] = meshPath;
            }

            renderableJson["visible"] = renderable.visible;
            renderableJson["castsShadows"] = renderable.castsShadows;

            entityJson["renderable"] = renderableJson;
        }

        // Serialize Camera component
        if (m_ECS->HasComponent<Camera>(entity)) {
            const Camera& camera = m_ECS->GetComponent<Camera>(entity);
            json cameraJson;

            // Projection type (0 = Perspective, 1 = Orthographic)
            cameraJson["projection"] = static_cast<i32>(camera.projection);

            // Perspective parameters
            cameraJson["fov"] = camera.fov;
            cameraJson["aspectRatio"] = camera.aspectRatio;
            cameraJson["nearPlane"] = camera.nearPlane;
            cameraJson["farPlane"] = camera.farPlane;

            // Orthographic parameters
            cameraJson["orthoSize"] = camera.orthoSize;

            // Clear color
            cameraJson["clearColor"] = {
                {"r", camera.clearColor.r},
                {"g", camera.clearColor.g},
                {"b", camera.clearColor.b},
                {"a", camera.clearColor.a}
            };

            // Active flag
            cameraJson["isActive"] = camera.isActive;

            entityJson["camera"] = cameraJson;
        }

        // Serialize Rotator component
        if (m_ECS->HasComponent<Rotator>(entity)) {
            const Rotator& rotator = m_ECS->GetComponent<Rotator>(entity);
            json rotatorJson;

            rotatorJson["axis"] = {
                {"x", rotator.axis.x},
                {"y", rotator.axis.y},
                {"z", rotator.axis.z}
            };
            rotatorJson["speed"] = rotator.speed;

            entityJson["rotator"] = rotatorJson;
        }

        // Serialize Light component
        if (m_ECS->HasComponent<Light>(entity)) {
            const Light& light = m_ECS->GetComponent<Light>(entity);
            json lightJson;

            // Light type (0 = Directional, 1 = Point, 2 = Spot)
            lightJson["type"] = static_cast<i32>(light.type);

            // Color and intensity
            lightJson["color"] = {
                {"r", light.color.r},
                {"g", light.color.g},
                {"b", light.color.b}
            };
            lightJson["intensity"] = light.intensity;

            // Point/Spot parameters
            lightJson["range"] = light.range;
            lightJson["attenuation"] = light.attenuation;

            // Spot parameters
            lightJson["innerConeAngle"] = light.innerConeAngle;
            lightJson["outerConeAngle"] = light.outerConeAngle;

            // Shadow casting
            lightJson["castsShadows"] = light.castsShadows;

            entityJson["light"] = lightJson;
        }

        // Serialize hierarchy
        Entity parent = m_ECS->GetParent(entity);
        if (parent.IsValid()) {
            entityJson["parent"] = parent.index;
        }

        sceneJson["entities"].push_back(entityJson);
    }

    // Write to file
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filepath << std::endl;
        return false;
    }

    file << sceneJson.dump(4);  // Pretty print with 4-space indent
    file.close();

    std::cout << "Scene saved to: " << filepath << std::endl;
    return true;
}

bool SceneSerializer::LoadScene(const std::string& filepath) {
    // Read file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for reading: " << filepath << std::endl;
        return false;
    }

    json sceneJson;
    file >> sceneJson;
    file.close();

    // Check version
    if (!sceneJson.contains("version") || sceneJson["version"] != 1) {
        std::cerr << "Unsupported scene version" << std::endl;
        return false;
    }

    // Map old entity IDs to new entities (for hierarchy)
    std::unordered_map<u32, Entity> entityMap;

    // First pass: Create entities and components
    for (const auto& entityJson : sceneJson["entities"]) {
        Entity entity = m_ECS->CreateEntity();
        u32 oldId = entityJson["id"];
        entityMap[oldId] = entity;

        // Load transform
        if (entityJson.contains("transform")) {
            const auto& tJson = entityJson["transform"];

            Transform transform;
            transform.localPosition = Vec3(
                tJson["position"]["x"],
                tJson["position"]["y"],
                tJson["position"]["z"]
            );
            transform.localRotation = Quat(
                tJson["rotation"]["w"],
                tJson["rotation"]["x"],
                tJson["rotation"]["y"],
                tJson["rotation"]["z"]
            );
            transform.localScale = Vec3(
                tJson["scale"]["x"],
                tJson["scale"]["y"],
                tJson["scale"]["z"]
            );

            m_ECS->AddComponent(entity, transform);
        }

        // Load Renderable component
        if (entityJson.contains("renderable")) {
            const auto& rJson = entityJson["renderable"];

            Renderable renderable;

            // Load mesh
            std::string meshPath;
            if (rJson.contains("mesh") && rJson["mesh"].is_string()) {
                meshPath = rJson["mesh"].get<std::string>();

                // If path is relative, prepend ENGINE_SOURCE_DIR
                std::filesystem::path path(meshPath);
                if (path.is_relative()) {
                    meshPath = std::string(ENGINE_SOURCE_DIR) + "/" + meshPath;
                }

                renderable.mesh = MeshManager::Instance().Load(meshPath);
            }

            // Hybrid material loading: Priority 1) materialData, 2) material, 3) meshPath reload
            bool materialLoaded = false;

            // Priority 1: Inline materialData (custom/embedded materials)
            if (rJson.contains("materialData") && rJson["materialData"].is_object()) {
                // Get scene directory for relative texture paths
                std::filesystem::path scenePath(filepath);
                std::string sceneDir = scenePath.parent_path().string();

                // Deserialize material data
                MaterialData materialData = DeserializeMaterialData(rJson["materialData"], sceneDir);

                // Check if the material has any valid textures
                // If materialData exists but has no textures, it's likely embedded textures
                // that weren't serialized, so we should fall back to meshPath reload
                bool hasAnyTexture = materialData.albedo.IsValid() ||
                                   materialData.normal.IsValid() ||
                                   materialData.metalRough.IsValid() ||
                                   materialData.ao.IsValid() ||
                                   materialData.emissive.IsValid();

                if (hasAnyTexture) {
                    // Create material via GetOrCreate (deduplicates if same material exists)
                    renderable.material = MaterialManager::Instance().GetOrCreate(materialData, "SceneInlineMaterial");
                    materialLoaded = true;
                }
                // If no textures, fall through to meshPath reload (Priority 3)
            }

            // Priority 2: File-based material path
            if (!materialLoaded && rJson.contains("material") && rJson["material"].is_string()) {
                std::string materialPath = rJson["material"].get<std::string>();

                // If path is relative, prepend ENGINE_SOURCE_DIR
                std::filesystem::path path(materialPath);
                if (path.is_relative()) {
                    materialPath = std::string(ENGINE_SOURCE_DIR) + "/" + materialPath;
                }

                renderable.material = MaterialManager::Instance().Load(materialPath);
                materialLoaded = true;
            }

            // Priority 3: Reload material from meshPath (for embedded textures in FBX/glTF)
            if (!materialLoaded && rJson.contains("meshPath") && rJson["meshPath"].is_string()) {
                std::string reloadMeshPath = rJson["meshPath"].get<std::string>();

                // If path is relative, prepend ENGINE_SOURCE_DIR
                std::filesystem::path path(reloadMeshPath);
                if (path.is_relative()) {
                    reloadMeshPath = std::string(ENGINE_SOURCE_DIR) + "/" + reloadMeshPath;
                }

                // Load mesh with embedded materials
                MeshLoadResult result = MeshManager::Instance().LoadWithMaterial(reloadMeshPath);
                if (result.IsValid()) {
                    // Use the material from the loaded result
                    renderable.material = result.material;

                    // Note: We already loaded the mesh above, so we keep using that mesh handle
                    // This ensures the mesh data is cached and shared properly
                }
            }

            if (rJson.contains("visible")) {
                renderable.visible = rJson["visible"].get<bool>();
            }

            if (rJson.contains("castsShadows")) {
                renderable.castsShadows = rJson["castsShadows"].get<bool>();
            }

            m_ECS->AddComponent(entity, renderable);
        }

        // Load Camera component
        if (entityJson.contains("camera")) {
            const auto& cJson = entityJson["camera"];

            Camera camera;

            // Projection type
            if (cJson.contains("projection")) {
                camera.projection = static_cast<CameraProjection>(cJson["projection"].get<i32>());
            }

            // Perspective parameters
            if (cJson.contains("fov")) {
                camera.fov = cJson["fov"].get<f32>();
            }
            if (cJson.contains("aspectRatio")) {
                camera.aspectRatio = cJson["aspectRatio"].get<f32>();
            }
            if (cJson.contains("nearPlane")) {
                camera.nearPlane = cJson["nearPlane"].get<f32>();
            }
            if (cJson.contains("farPlane")) {
                camera.farPlane = cJson["farPlane"].get<f32>();
            }

            // Orthographic parameters
            if (cJson.contains("orthoSize")) {
                camera.orthoSize = cJson["orthoSize"].get<f32>();
            }

            // Clear color
            if (cJson.contains("clearColor")) {
                const auto& ccJson = cJson["clearColor"];
                camera.clearColor = Vec4(
                    ccJson["r"].get<f32>(),
                    ccJson["g"].get<f32>(),
                    ccJson["b"].get<f32>(),
                    ccJson["a"].get<f32>()
                );
            }

            // Active flag
            if (cJson.contains("isActive")) {
                camera.isActive = cJson["isActive"].get<bool>();
            }

            m_ECS->AddComponent(entity, camera);
        }

        // Load Rotator component
        if (entityJson.contains("rotator")) {
            const auto& rotJson = entityJson["rotator"];

            Rotator rotator;

            // Axis
            if (rotJson.contains("axis")) {
                const auto& axisJson = rotJson["axis"];
                rotator.axis = Vec3(
                    axisJson["x"].get<f32>(),
                    axisJson["y"].get<f32>(),
                    axisJson["z"].get<f32>()
                );
            }

            // Speed
            if (rotJson.contains("speed")) {
                rotator.speed = rotJson["speed"].get<f32>();
            }

            m_ECS->AddComponent(entity, rotator);
        }

        // Load Light component
        if (entityJson.contains("light")) {
            const auto& lJson = entityJson["light"];

            Light light;

            // Light type
            if (lJson.contains("type")) {
                light.type = static_cast<LightType>(lJson["type"].get<i32>());
            }

            // Color
            if (lJson.contains("color")) {
                const auto& colorJson = lJson["color"];
                light.color = Vec3(
                    colorJson["r"].get<f32>(),
                    colorJson["g"].get<f32>(),
                    colorJson["b"].get<f32>()
                );
            }

            // Intensity
            if (lJson.contains("intensity")) {
                light.intensity = lJson["intensity"].get<f32>();
            }

            // Range
            if (lJson.contains("range")) {
                light.range = lJson["range"].get<f32>();
            }

            // Attenuation
            if (lJson.contains("attenuation")) {
                light.attenuation = lJson["attenuation"].get<f32>();
            }

            // Cone angles
            if (lJson.contains("innerConeAngle")) {
                light.innerConeAngle = lJson["innerConeAngle"].get<f32>();
            }
            if (lJson.contains("outerConeAngle")) {
                light.outerConeAngle = lJson["outerConeAngle"].get<f32>();
            }

            // Shadow casting
            if (lJson.contains("castsShadows")) {
                light.castsShadows = lJson["castsShadows"].get<bool>();
            }

            m_ECS->AddComponent(entity, light);
        }
    }

    // Second pass: Set up hierarchy
    for (const auto& entityJson : sceneJson["entities"]) {
        if (entityJson.contains("parent")) {
            u32 childOldId = entityJson["id"];
            u32 parentOldId = entityJson["parent"];

            Entity child = entityMap[childOldId];
            Entity parent = entityMap[parentOldId];

            m_ECS->SetParent(child, parent);
        }
    }

    std::cout << "Scene loaded from: " << filepath << std::endl;
    return true;
}
