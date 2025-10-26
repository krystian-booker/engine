#pragma once
#include "ecs_coordinator.h"
#include "hierarchy_manager.h"
#include <string>

// SceneSerializer: Save/load entire scenes to JSON format
// Serializes entities, Transform components, and hierarchy relationships
class SceneSerializer {
public:
    SceneSerializer(ECSCoordinator* ecs)
        : m_ECS(ecs) {}

    // Save scene to JSON file
    // Returns true on success, false on file I/O error
    bool SaveScene(const std::string& filepath);

    // Load scene from JSON file
    // Creates new entities and restores components and hierarchy
    // Returns true on success, false on file I/O or parse error
    bool LoadScene(const std::string& filepath);

private:
    ECSCoordinator* m_ECS;
};
