#include "core/file_dialog.h"
#include <iostream>

#ifdef PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <commdlg.h>
    #include <vector>
    #include <algorithm>
#else
    #include "portable-file-dialogs.h"
#endif

std::optional<std::string> FileDialog::OpenFile(
    const std::string& title,
    const std::string& defaultPath,
    [[maybe_unused]] const std::vector<std::string>& filters
) {
#ifdef PLATFORM_WINDOWS
    // Windows-specific implementation using native dialogs
    OPENFILENAMEA ofn = {};
    char szFile[260] = {0};

    // Get absolute path for initial directory
    char absolutePath[MAX_PATH];
    if (!defaultPath.empty()) {
        if (GetFullPathNameA(defaultPath.c_str(), MAX_PATH, absolutePath, NULL) != 0) {
            ofn.lpstrInitialDir = absolutePath;
        } else {
            ofn.lpstrInitialDir = defaultPath.c_str();
        }
    } else {
        ofn.lpstrInitialDir = NULL;
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Scene Files\0*.scene\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = title.c_str();

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }

    return std::nullopt;
#else
    // Unix/Linux implementation using portable-file-dialogs
    try {
        pfd::open_file dialog(title, defaultPath, filters, pfd::opt::none);
        auto result = dialog.result();

        if (!result.empty()) {
            return result[0];
        }
    } catch (const std::exception& e) {
        std::cerr << "FileDialog::OpenFile error: " << e.what() << std::endl;
    }

    return std::nullopt;
#endif
}

std::optional<std::string> FileDialog::SaveFile(
    const std::string& title,
    const std::string& defaultPath,
    [[maybe_unused]] const std::vector<std::string>& filters
) {
#ifdef PLATFORM_WINDOWS
    // Windows-specific implementation using native dialogs
    OPENFILENAMEA ofn = {};
    char szFile[260] = {0};

    // Get absolute path and extract directory and filename
    char absolutePath[MAX_PATH];
    const char* initialDir = NULL;
    if (!defaultPath.empty()) {
        if (GetFullPathNameA(defaultPath.c_str(), MAX_PATH, absolutePath, NULL) != 0) {
            // Copy the full path to szFile (this will be the default filename)
            strncpy_s(szFile, sizeof(szFile), absolutePath, _TRUNCATE);

            // Extract directory for initial dir by finding the last backslash
            char* lastSlash = strrchr(absolutePath, '\\');
            if (lastSlash != NULL) {
                *lastSlash = '\0';  // Null-terminate to get just the directory
                initialDir = absolutePath;
            }
        } else {
            strncpy_s(szFile, sizeof(szFile), defaultPath.c_str(), _TRUNCATE);
        }
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Scene Files\0*.scene\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = initialDir;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrDefExt = "scene";

    if (GetSaveFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }

    return std::nullopt;
#else
    // Unix/Linux implementation using portable-file-dialogs
    try {
        pfd::save_file dialog(title, defaultPath, filters, pfd::opt::none);
        auto result = dialog.result();

        if (!result.empty()) {
            return result;
        }
    } catch (const std::exception& e) {
        std::cerr << "FileDialog::SaveFile error: " << e.what() << std::endl;
    }

    return std::nullopt;
#endif
}

std::optional<std::string> FileDialog::SelectFolder(
    [[maybe_unused]] const std::string& title,
    [[maybe_unused]] const std::string& defaultPath
) {
#ifdef PLATFORM_WINDOWS
    // Windows folder browser (simplified - full implementation would use IFileDialog)
    // For now, just return nullopt as folder selection is not critical for scene management
    std::cerr << "SelectFolder not implemented on Windows yet" << std::endl;
    return std::nullopt;
#else
    // Unix/Linux implementation using portable-file-dialogs
    try {
        pfd::select_folder dialog(title, defaultPath, pfd::opt::none);
        auto result = dialog.result();

        if (!result.empty()) {
            return result;
        }
    } catch (const std::exception& e) {
        std::cerr << "FileDialog::SelectFolder error: " << e.what() << std::endl;
    }

    return std::nullopt;
#endif
}
