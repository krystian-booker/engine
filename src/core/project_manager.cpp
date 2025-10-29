#include "project_manager.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

ProjectManager::ProjectManager() {
    LoadRecentProjects();
}

ProjectManager::~ProjectManager() {
    if (m_IsDirty && HasActiveProject()) {
        SaveProject();
    }
}

bool ProjectManager::CreateProject(const std::string& folderPath, const std::string& projectName) {
    // Validate inputs
    if (!IsValidProjectName(projectName)) {
        std::cerr << "Invalid project name: " << projectName << std::endl;
        return false;
    }

    if (!ValidateProjectFolder(folderPath)) {
        std::cerr << "Invalid or non-empty folder: " << folderPath << std::endl;
        return false;
    }

    // Create project config
    m_CurrentProject = ProjectConfig();
    m_CurrentProject.name = projectName;
    m_CurrentProject.rootPath = folderPath;

    // Create directory structure
    if (!CreateProjectStructure(folderPath)) {
        std::cerr << "Failed to create project structure" << std::endl;
        return false;
    }

    // Create default scene
    std::string defaultScenePath = folderPath + "/" + m_CurrentProject.scenesPath + "/default.scene";
    if (!CreateDefaultScene(defaultScenePath)) {
        std::cerr << "Failed to create default scene" << std::endl;
        return false;
    }

    m_CurrentProject.lastOpenedScene = m_CurrentProject.scenesPath + "/default.scene";

    // Save project file
    if (!SaveProject()) {
        std::cerr << "Failed to save project file" << std::endl;
        return false;
    }

    // Add to recent projects
    AddRecentProject(GetProjectFilePath());

    m_IsDirty = false;
    return true;
}

bool ProjectManager::LoadProject(const std::string& projectFilePath) {
    try {
        // Check if file exists
        if (!fs::exists(projectFilePath)) {
            std::cerr << "Project file not found: " << projectFilePath << std::endl;
            return false;
        }

        // Read JSON file
        std::ifstream file(projectFilePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open project file: " << projectFilePath << std::endl;
            return false;
        }

        json j;
        file >> j;
        file.close();

        // Parse project config
        m_CurrentProject = ProjectConfig();
        m_CurrentProject.name = j.value("name", "Untitled Project");
        m_CurrentProject.rootPath = fs::path(projectFilePath).parent_path().string();
        m_CurrentProject.assetsPath = j.value("assetsPath", "assets");
        m_CurrentProject.scenesPath = j.value("scenesPath", "assets/scenes");
        m_CurrentProject.modelsPath = j.value("modelsPath", "assets/models");
        m_CurrentProject.texturesPath = j.value("texturesPath", "assets/textures");
        m_CurrentProject.materialsPath = j.value("materialsPath", "assets/materials");
        m_CurrentProject.shadersPath = j.value("shadersPath", "assets/shaders");
        m_CurrentProject.lastOpenedScene = j.value("lastOpenedScene", "");

        // Window settings
        if (j.contains("windowSettings")) {
            auto& ws = j["windowSettings"];
            m_CurrentProject.windowWidth = ws.value("width", 1920u);
            m_CurrentProject.windowHeight = ws.value("height", 1080u);
            m_CurrentProject.windowFullscreen = ws.value("fullscreen", false);
            m_CurrentProject.windowVSync = ws.value("vsync", true);
        }

        // Graphics settings
        if (j.contains("graphicsSettings")) {
            auto& gs = j["graphicsSettings"];
            m_CurrentProject.msaaSamples = gs.value("msaaSamples", 4u);
            m_CurrentProject.anisotropicFiltering = gs.value("anisotropicFiltering", true);
            m_CurrentProject.maxAnisotropy = gs.value("maxAnisotropy", 16.0f);
        }

        // Add to recent projects
        AddRecentProject(projectFilePath);

        m_IsDirty = false;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to load project: " << e.what() << std::endl;
        return false;
    }
}

bool ProjectManager::SaveProject() {
    if (!HasActiveProject()) {
        std::cerr << "No active project to save" << std::endl;
        return false;
    }

    try {
        // Create JSON object
        json j;
        j["name"] = m_CurrentProject.name;
        j["assetsPath"] = m_CurrentProject.assetsPath;
        j["scenesPath"] = m_CurrentProject.scenesPath;
        j["modelsPath"] = m_CurrentProject.modelsPath;
        j["texturesPath"] = m_CurrentProject.texturesPath;
        j["materialsPath"] = m_CurrentProject.materialsPath;
        j["shadersPath"] = m_CurrentProject.shadersPath;
        j["lastOpenedScene"] = m_CurrentProject.lastOpenedScene;

        // Window settings
        j["windowSettings"] = {
            {"width", m_CurrentProject.windowWidth},
            {"height", m_CurrentProject.windowHeight},
            {"fullscreen", m_CurrentProject.windowFullscreen},
            {"vsync", m_CurrentProject.windowVSync}
        };

        // Graphics settings
        j["graphicsSettings"] = {
            {"msaaSamples", m_CurrentProject.msaaSamples},
            {"anisotropicFiltering", m_CurrentProject.anisotropicFiltering},
            {"maxAnisotropy", m_CurrentProject.maxAnisotropy}
        };

        // Write to file
        std::string projectFilePath = GetProjectFilePath();
        std::ofstream file(projectFilePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open project file for writing: " << projectFilePath << std::endl;
            return false;
        }

        file << j.dump(4);  // Pretty print with 4 spaces
        file.close();

        m_IsDirty = false;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to save project: " << e.what() << std::endl;
        return false;
    }
}

std::string ProjectManager::GetProjectFilePath() const {
    if (!HasActiveProject()) {
        return "";
    }
    return m_CurrentProject.rootPath + "/project.json";
}

std::string ProjectManager::GetAbsolutePath(const std::string& relativePath) const {
    if (!HasActiveProject()) {
        return relativePath;
    }
    return m_CurrentProject.rootPath + "/" + relativePath;
}

void ProjectManager::SetLastOpenedScene(const std::string& scenePath) {
    m_CurrentProject.lastOpenedScene = scenePath;
    m_IsDirty = true;
}

void ProjectManager::AddRecentProject(const std::string& projectPath) {
    // Remove if already exists
    auto it = std::find(m_RecentProjects.begin(), m_RecentProjects.end(), projectPath);
    if (it != m_RecentProjects.end()) {
        m_RecentProjects.erase(it);
    }

    // Add to front
    m_RecentProjects.insert(m_RecentProjects.begin(), projectPath);

    // Trim to max size
    if (m_RecentProjects.size() > MAX_RECENT_PROJECTS) {
        m_RecentProjects.resize(MAX_RECENT_PROJECTS);
    }

    SaveRecentProjects();
}

bool ProjectManager::ValidateProjectFolder(const std::string& folderPath) {
    try {
        // Check if path exists
        if (!fs::exists(folderPath)) {
            // Try to create it
            if (!fs::create_directories(folderPath)) {
                return false;
            }
        }

        // Check if it's a directory
        if (!fs::is_directory(folderPath)) {
            return false;
        }

        // Check if directory is empty (or only contains project.json)
        u32 fileCount = 0;
        for (const auto& entry : fs::directory_iterator(folderPath)) {
            if (entry.path().filename() != "project.json") {
                fileCount++;
            }
        }

        return fileCount == 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error validating project folder: " << e.what() << std::endl;
        return false;
    }
}

bool ProjectManager::IsValidProjectName(const std::string& name) {
    if (name.empty() || name.length() > 64) {
        return false;
    }

    // Check for invalid characters
    const std::string invalidChars = "<>:\"/\\|?*";
    for (char c : name) {
        if (invalidChars.find(c) != std::string::npos) {
            return false;
        }
    }

    return true;
}

bool ProjectManager::CreateProjectStructure(const std::string& rootPath) {
    try {
        // Create root directory
        fs::create_directories(rootPath);

        // Create subdirectories
        fs::create_directories(rootPath + "/" + m_CurrentProject.assetsPath);
        fs::create_directories(rootPath + "/" + m_CurrentProject.scenesPath);
        fs::create_directories(rootPath + "/" + m_CurrentProject.modelsPath);
        fs::create_directories(rootPath + "/" + m_CurrentProject.texturesPath);
        fs::create_directories(rootPath + "/" + m_CurrentProject.materialsPath);
        fs::create_directories(rootPath + "/" + m_CurrentProject.shadersPath);

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to create project structure: " << e.what() << std::endl;
        return false;
    }
}

bool ProjectManager::CreateDefaultScene(const std::string& scenePath) {
    try {
        // Create a minimal empty scene JSON
        json sceneJson;
        sceneJson["entities"] = json::array();
        sceneJson["hierarchy"] = json::object();

        // Write to file
        std::ofstream file(scenePath);
        if (!file.is_open()) {
            return false;
        }

        file << sceneJson.dump(4);
        file.close();

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to create default scene: " << e.what() << std::endl;
        return false;
    }
}

void ProjectManager::LoadRecentProjects() {
    try {
        std::string configPath = "config/recent_projects.json";

        if (!fs::exists(configPath)) {
            return;
        }

        std::ifstream file(configPath);
        if (!file.is_open()) {
            return;
        }

        json j;
        file >> j;
        file.close();

        if (j.contains("recentProjects") && j["recentProjects"].is_array()) {
            m_RecentProjects.clear();
            for (const auto& path : j["recentProjects"]) {
                if (path.is_string()) {
                    std::string projectPath = path.get<std::string>();
                    // Only add if the project file still exists
                    if (fs::exists(projectPath)) {
                        m_RecentProjects.push_back(projectPath);
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to load recent projects: " << e.what() << std::endl;
    }
}

void ProjectManager::SaveRecentProjects() {
    try {
        // Ensure config directory exists
        fs::create_directories("config");

        json j;
        j["recentProjects"] = m_RecentProjects;

        std::ofstream file("config/recent_projects.json");
        if (!file.is_open()) {
            std::cerr << "Failed to save recent projects" << std::endl;
            return;
        }

        file << j.dump(4);
        file.close();
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to save recent projects: " << e.what() << std::endl;
    }
}
