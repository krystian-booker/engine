#pragma once

#include "core/types.h"
#include "ecs/entity.h"
#include <string>
#include <vector>
#include <optional>

// Forward declarations
class ECSCoordinator;
class SceneSerializer;
class CameraSystem;
class CameraController;

/**
 * SceneManager
 *
 * Manages scene state including current file path, dirty flag,
 * and recent scenes list. Provides high-level scene operations
 * for ImGui integration.
 */
class SceneManager {
public:
    SceneManager(ECSCoordinator* ecs, CameraSystem* cameraSystem = nullptr, CameraController* cameraController = nullptr);
    ~SceneManager();

    // Scene operations
    void NewScene();
    bool LoadScene(const std::string& filepath);
    bool SaveScene();
    bool SaveSceneAs(const std::string& filepath);

    // State queries
    bool IsDirty() const { return m_IsDirty; }
    bool HasCurrentFile() const { return !m_CurrentFilePath.empty(); }
    const std::string& GetCurrentFilePath() const { return m_CurrentFilePath; }
    const std::vector<std::string>& GetRecentScenes() const { return m_RecentScenes; }

    // State modification
    void MarkDirty();
    void MarkClean();

    // Recent scenes management
    void AddRecentScene(const std::string& filepath);
    void LoadRecentScenesList();
    void SaveRecentScenesList();

    // Editor camera management
    Entity GetEditorCamera() const { return m_EditorCamera; }
    Entity EnsureEditorCamera();  // Creates editor camera if it doesn't exist

private:
    ECSCoordinator* m_ECS;
    SceneSerializer* m_Serializer;
    CameraSystem* m_CameraSystem;
    CameraController* m_CameraController;

    std::string m_CurrentFilePath;
    bool m_IsDirty = false;

    // Recent scenes list (up to 10 items)
    std::vector<std::string> m_RecentScenes;
    static constexpr u32 MAX_RECENT_SCENES = 10;

    // Editor camera entity (persistent across scenes, not serialized)
    Entity m_EditorCamera = Entity::Invalid;

    // Helper: Clear all entities from the scene
    void ClearScene();
    void CreateEditorCamera();
};
