#include "core/file_dialog.h"
#include <iostream>

#ifdef PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <commdlg.h>
    #include <shobjidl.h>
    #include <vector>
    #include <algorithm>
#else
    #include "portable-file-dialogs.h"
#endif

std::optional<std::string> FileDialog::OpenFile(
    const std::string& title,
    const std::string& defaultPath,
    const std::vector<std::string>& filters
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

    // Build filter string from filters vector
    std::string filterStr;
    if (!filters.empty() && filters.size() % 2 == 0) {
        for (size_t i = 0; i < filters.size(); i += 2) {
            filterStr += filters[i] + '\0' + filters[i + 1] + '\0';
        }
        filterStr += '\0';
    } else {
        // Default filter
        filterStr = "All Files\0*.*\0\0";
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filterStr.c_str();
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
    const std::vector<std::string>& filters
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

    // Build filter string from filters vector
    std::string filterStr;
    std::string defaultExt;
    if (!filters.empty() && filters.size() % 2 == 0) {
        for (size_t i = 0; i < filters.size(); i += 2) {
            filterStr += filters[i] + '\0' + filters[i + 1] + '\0';
            // Extract default extension from first filter pattern (e.g., "*.scene" -> "scene")
            if (i == 0 && defaultExt.empty()) {
                std::string pattern = filters[i + 1];
                size_t dotPos = pattern.find('.');
                if (dotPos != std::string::npos && dotPos + 1 < pattern.length()) {
                    defaultExt = pattern.substr(dotPos + 1);
                }
            }
        }
        filterStr += '\0';
    } else {
        // Default filter
        filterStr = "All Files\0*.*\0\0";
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filterStr.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = initialDir;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrDefExt = defaultExt.empty() ? NULL : defaultExt.c_str();

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
    const std::string& title,
    const std::string& defaultPath
) {
#ifdef PLATFORM_WINDOWS
    // Initialize COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "Failed to initialize COM" << std::endl;
        return std::nullopt;
    }

    std::optional<std::string> result = std::nullopt;
    IFileDialog* pFileDialog = NULL;

    // Create the folder picker dialog
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileDialog));
    if (SUCCEEDED(hr)) {
        // Set options for folder picking
        DWORD dwOptions;
        hr = pFileDialog->GetOptions(&dwOptions);
        if (SUCCEEDED(hr)) {
            pFileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
        }

        // Set title
        if (!title.empty()) {
            std::wstring wideTitle(title.begin(), title.end());
            pFileDialog->SetTitle(wideTitle.c_str());
        }

        // Set default folder
        if (!defaultPath.empty()) {
            IShellItem* pDefaultFolder = NULL;
            std::wstring widePath(defaultPath.begin(), defaultPath.end());
            hr = SHCreateItemFromParsingName(widePath.c_str(), NULL, IID_PPV_ARGS(&pDefaultFolder));
            if (SUCCEEDED(hr)) {
                pFileDialog->SetFolder(pDefaultFolder);
                pDefaultFolder->Release();
            }
        }

        // Show the dialog
        hr = pFileDialog->Show(NULL);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = NULL;
            hr = pFileDialog->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = NULL;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    // Convert wide string to narrow string
                    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, NULL, 0, NULL, NULL);
                    if (sizeNeeded > 0) {
                        std::vector<char> buffer(sizeNeeded);
                        WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, buffer.data(), sizeNeeded, NULL, NULL);
                        result = std::string(buffer.data());
                    }
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileDialog->Release();
    }

    // Don't uninitialize COM as it may have been initialized elsewhere
    // CoUninitialize();

    return result;
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
