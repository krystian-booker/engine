#include "scene_serializer.h"
#include "components/transform.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

bool SceneSerializer::SaveScene(const std::string& filepath) {
    json sceneJson;
    sceneJson["version"] = 1;
    sceneJson["entities"] = json::array();

    auto transforms = m_ECS->GetComponentRegistry()->GetComponentArray<Transform>();

    // Serialize all entities with transforms
    for (size_t i = 0; i < transforms->Size(); ++i) {
        Entity entity = transforms->GetEntity(i);
        const Transform& transform = transforms->Get(entity);

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
