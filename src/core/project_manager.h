#pragma once

#include "core/types.h"
#include <string>
#include <vector>

/**
 * ProjectConfig
 *
 * Configuration for a game project including paths, settings, and metadata.
 */
struct ProjectConfig {
    std::string name = "Untitled Project";
    std::string rootPath;
    std::string assetsPath = "assets";
    std::string scenesPath = "assets/scenes";
    std::string modelsPath = "assets/models";
    std::string texturesPath = "assets/textures";
    std::string materialsPath = "assets/materials";
    std::string shadersPath = "assets/shaders";

    // Last opened scene
    std::string lastOpenedScene;

    // Window settings
    u32 windowWidth = 1920;
    u32 windowHeight = 1080;
    bool windowFullscreen = false;
    bool windowVSync = true;

    // Graphics settings
    u32 msaaSamples = 4;
    bool anisotropicFiltering = true;
    f32 maxAnisotropy = 16.0f;
};

/**
 * ProjectManager
 *
 * Manages project creation, loading, and configuration.
 * Handles project directory structure and .engineproject serialization.
 */
class ProjectManager {
public:
    ProjectManager();
    ~ProjectManager();

    // Project operations
    bool CreateProject(const std::string& folderPath, const std::string& projectName);
    bool LoadProject(const std::string& projectFilePath);
    bool SaveProject();
    bool HasActiveProject() const { return !m_CurrentProject.rootPath.empty(); }

    // Project queries
    const ProjectConfig& GetProject() const { return m_CurrentProject; }
    ProjectConfig& GetProject() { return m_CurrentProject; }
    std::string GetProjectFilePath() const;
    std::string GetAbsolutePath(const std::string& relativePath) const;

    // Project modification
    void SetLastOpenedScene(const std::string& scenePath);
    void MarkDirty() { m_IsDirty = true; }
    bool IsDirty() const { return m_IsDirty; }

    // Recent projects management (stored globally in engine settings)
    const std::vector<std::string>& GetRecentProjects() const { return m_RecentProjects; }
    void AddRecentProject(const std::string& projectPath);

    // Validation
    static bool ValidateProjectFolder(const std::string& folderPath);
    static bool IsValidProjectName(const std::string& name);

private:
    ProjectConfig m_CurrentProject;
    bool m_IsDirty = false;

    // Recent projects list (loaded from engine settings)
    std::vector<std::string> m_RecentProjects;
    static constexpr u32 MAX_RECENT_PROJECTS = 10;

    // Helper methods
    bool CreateProjectStructure(const std::string& rootPath);
    bool CreateDefaultScene(const std::string& scenePath);
    void LoadRecentProjects();
    void SaveRecentProjects();
};
