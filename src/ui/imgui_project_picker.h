#pragma once

#include "core/types.h"
#include <string>

// Forward declarations
class ProjectManager;
struct GLFWwindow;

/**
 * ProjectPickerResult
 *
 * Result returned from the project picker window.
 */
struct ProjectPickerResult {
    bool success = false;               // Whether a project was selected/created
    std::string projectPath;             // Path to the selected/created project.json
    bool setAsDefault = false;           // Whether to set this as the default project
    bool cancelled = false;              // Whether the user cancelled
};

/**
 * ImGuiProjectPicker
 *
 * Dear ImGui-based project selection UI shown before engine initialization.
 * Uses OpenGL 3.3 backend with GLFW to show a standalone window.
 * Allows user to:
 * - Select from recent projects
 * - Create a new project
 * - Open an existing project
 * - Set a project as default (skip picker next time)
 */
class ImGuiProjectPicker {
public:
    ImGuiProjectPicker(ProjectManager* projectMgr);
    ~ImGuiProjectPicker();

    // Disable copy
    ImGuiProjectPicker(const ImGuiProjectPicker&) = delete;
    ImGuiProjectPicker& operator=(const ImGuiProjectPicker&) = delete;

    // Run the project picker window (blocks until user makes a selection)
    ProjectPickerResult Show();

private:
    ProjectManager* m_ProjectManager;
    GLFWwindow* m_Window = nullptr;
    bool m_Initialized = false;

    // UI state
    char m_NewProjectName[128] = {};
    char m_NewProjectPath[512] = {};
    i32 m_SelectedRecentIndex = -1;
    bool m_SetAsDefault = false;
    bool m_ShowCreateDialog = false;
    bool m_ShowError = false;
    std::string m_ErrorMessage;

    // Initialization
    bool Init();
    void Shutdown();

    // UI rendering
    void RenderMainWindow();
    void RenderRecentProjects();
    void RenderCreateDialog();
    void RenderErrorPopup();

    // Actions
    void OnCreateProject();
    void OnOpenProject();
    void OnSelectRecentProject(i32 index);
    void ShowError(const std::string& message);
};
