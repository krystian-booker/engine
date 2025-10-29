#include "core/scene_manager.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/scene_serializer.h"
#include "ecs/components/transform.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

SceneManager::SceneManager(ECSCoordinator* ecs)
    : m_ECS(ecs)
    , m_Serializer(new SceneSerializer(ecs))
    , m_IsDirty(false)
{
    LoadRecentScenesList();
}

SceneManager::~SceneManager() {
    delete m_Serializer;
}

void SceneManager::NewScene() {
    ClearScene();
    m_CurrentFilePath.clear();
    m_IsDirty = false;
}

bool SceneManager::LoadScene(const std::string& filepath) {
    // Clear existing scene
    ClearScene();

    // Load scene using serializer
    bool success = m_Serializer->LoadScene(filepath);

    if (success) {
        m_CurrentFilePath = filepath;
        m_IsDirty = false;
        AddRecentScene(filepath);
        return true;
    }

    return false;
}

bool SceneManager::SaveScene() {
    // If no current file, cannot save (use SaveSceneAs instead)
    if (m_CurrentFilePath.empty()) {
        std::cerr << "No current scene file. Use Save As instead." << std::endl;
        return false;
    }

    bool success = m_Serializer->SaveScene(m_CurrentFilePath);

    if (success) {
        m_IsDirty = false;
        AddRecentScene(m_CurrentFilePath);
    }

    return success;
}

bool SceneManager::SaveSceneAs(const std::string& filepath) {
    bool success = m_Serializer->SaveScene(filepath);

    if (success) {
        m_CurrentFilePath = filepath;
        m_IsDirty = false;
        AddRecentScene(filepath);
    }

    return success;
}

void SceneManager::MarkDirty() {
    m_IsDirty = true;
}

void SceneManager::MarkClean() {
    m_IsDirty = false;
}

void SceneManager::AddRecentScene(const std::string& filepath) {
    // Remove if already exists
    auto it = std::find(m_RecentScenes.begin(), m_RecentScenes.end(), filepath);
    if (it != m_RecentScenes.end()) {
        m_RecentScenes.erase(it);
    }

    // Add to front
    m_RecentScenes.insert(m_RecentScenes.begin(), filepath);

    // Trim to max size
    if (m_RecentScenes.size() > MAX_RECENT_SCENES) {
        m_RecentScenes.resize(MAX_RECENT_SCENES);
    }

    // Save to disk
    SaveRecentScenesList();
}

void SceneManager::LoadRecentScenesList() {
    std::string configPath = "config/recent_scenes.json";
    std::ifstream file(configPath);

    if (!file.is_open()) {
        // File doesn't exist yet, that's fine
        return;
    }

    try {
        json data;
        file >> data;
        file.close();

        if (data.contains("recent_scenes") && data["recent_scenes"].is_array()) {
            m_RecentScenes.clear();
            for (const auto& item : data["recent_scenes"]) {
                if (item.is_string()) {
                    m_RecentScenes.push_back(item.get<std::string>());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to load recent scenes list: " << e.what() << std::endl;
    }
}

void SceneManager::SaveRecentScenesList() {
    // Ensure config directory exists
    // Note: This is a simple implementation. In production, you'd want
    // proper directory creation using std::filesystem
    std::string configPath = "config/recent_scenes.json";

    try {
        json data;
        data["recent_scenes"] = json::array();

        for (const auto& path : m_RecentScenes) {
            data["recent_scenes"].push_back(path);
        }

        std::ofstream file(configPath);
        if (file.is_open()) {
            file << data.dump(4);
            file.close();
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to save recent scenes list: " << e.what() << std::endl;
    }
}

void SceneManager::ClearScene() {
    // Collect all entities (via Transform component array, which all entities should have)
    auto transforms = m_ECS->GetComponentRegistry()->GetComponentArray<Transform>();

    std::vector<Entity> entitiesToDestroy;
    for (size_t i = 0; i < transforms->Size(); ++i) {
        entitiesToDestroy.push_back(transforms->GetEntity(i));
    }

    // Destroy all collected entities
    for (Entity entity : entitiesToDestroy) {
        m_ECS->DestroyEntity(entity);
    }
}
