#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <optional>

/**
 * File Dialog Utility
 *
 * Wrapper around portable-file-dialogs for consistent cross-platform
 * file open/save dialog behavior.
 */
class FileDialog {
public:
    /**
     * Show an "Open File" dialog
     *
     * @param title Dialog window title
     * @param defaultPath Starting directory path (optional)
     * @param filters File type filters (e.g., {"Scene Files (.scene)", "*.scene"})
     * @return Selected file path, or empty optional if cancelled
     */
    static std::optional<std::string> OpenFile(
        const std::string& title,
        const std::string& defaultPath = "",
        const std::vector<std::string>& filters = {}
    );

    /**
     * Show a "Save File" dialog
     *
     * @param title Dialog window title
     * @param defaultPath Starting directory/filename (optional)
     * @param filters File type filters (e.g., {"Scene Files (.scene)", "*.scene"})
     * @return Selected file path, or empty optional if cancelled
     */
    static std::optional<std::string> SaveFile(
        const std::string& title,
        const std::string& defaultPath = "",
        const std::vector<std::string>& filters = {}
    );

    /**
     * Show a "Select Folder" dialog
     *
     * @param title Dialog window title
     * @param defaultPath Starting directory path (optional)
     * @return Selected folder path, or empty optional if cancelled
     */
    static std::optional<std::string> SelectFolder(
        const std::string& title,
        const std::string& defaultPath = ""
    );
};
