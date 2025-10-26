#include "core/file_watcher.h"
#include "platform/window.h"
#include "platform/input.h"
#include "core/time.h"
#include <iostream>
#include <fstream>
#include <map>
#include <thread>
#include <chrono>

// Simple "asset" that can be hot-reloaded
struct TextAsset {
    std::string filepath;
    std::string content;
    u32 loadCount = 0;

    void Load() {
        std::ifstream file(filepath);
        if (file.is_open()) {
            content.clear();
            std::string line;
            while (std::getline(file, line)) {
                content += line + "\n";
            }
            file.close();
            loadCount++;
            std::cout << "  [LOADED] " << filepath << " (Load #" << loadCount << ")" << std::endl;
            std::cout << "  Content preview: " << content.substr(0, 50)
                      << (content.size() > 50 ? "..." : "") << std::endl;
        } else {
            std::cerr << "  [ERROR] Could not load: " << filepath << std::endl;
        }
    }
};

// Asset manager that tracks loaded assets
class AssetManager {
public:
    void LoadAsset(const std::string& filepath) {
        TextAsset asset;
        asset.filepath = filepath;
        asset.Load();
        m_Assets[filepath] = asset;
    }

    void ReloadAsset(const std::string& filepath) {
        auto it = m_Assets.find(filepath);
        if (it != m_Assets.end()) {
            std::cout << "\n[HOT-RELOAD] Reloading asset: " << filepath << std::endl;
            it->second.Load();
        } else {
            std::cout << "\n[HOT-RELOAD] New asset detected: " << filepath << std::endl;
            LoadAsset(filepath);
        }
    }

    void UnloadAsset(const std::string& filepath) {
        auto it = m_Assets.find(filepath);
        if (it != m_Assets.end()) {
            std::cout << "\n[HOT-RELOAD] Asset deleted: " << filepath << std::endl;
            m_Assets.erase(it);
        }
    }

    void PrintStats() const {
        std::cout << "\n--- Asset Manager Stats ---" << std::endl;
        std::cout << "Total assets loaded: " << m_Assets.size() << std::endl;
        for (const auto& [path, asset] : m_Assets) {
            std::cout << "  - " << path << " (reloaded " << asset.loadCount << " times)" << std::endl;
        }
        std::cout << "----------------------------" << std::endl;
    }

private:
    std::map<std::string, TextAsset> m_Assets;
};

int main() {
    std::cout << "=== File Watcher Hot-Reload Example ===" << std::endl;
    std::cout << std::endl;

    // Create window
    std::cout << "[SETUP] Creating window..." << std::endl;
    WindowProperties props;
    props.title = "File Watcher Example - Hot Reload Demo";
    props.width = 1280;
    props.height = 720;
    props.vsync = true;

    Window window(props);
    Input::Init(&window);
    Time::Init();

    // Create asset manager
    AssetManager assetManager;

    // Create test assets directory if it doesn't exist
    if (!fs::exists("example_assets")) {
        fs::create_directory("example_assets");
        std::cout << "[SETUP] Created example_assets directory" << std::endl;

        // Create some sample files
        std::ofstream file1("example_assets/config.txt");
        file1 << "# Sample Configuration File\n";
        file1 << "window_width=1280\n";
        file1 << "window_height=720\n";
        file1 << "vsync=true\n";
        file1.close();

        std::ofstream file2("example_assets/data.json");
        file2 << "{\n";
        file2 << "  \"name\": \"ExampleGame\",\n";
        file2 << "  \"version\": \"1.0.0\"\n";
        file2 << "}\n";
        file2.close();

        std::cout << "[SETUP] Created sample asset files" << std::endl;
    }

    // Setup file watcher
    std::cout << "[SETUP] Initializing file watcher..." << std::endl;
    FileWatcher watcher;

    // Register callbacks for different file types
    watcher.RegisterCallback(".txt", [&](const std::string& path, FileAction action) {
        switch (action) {
            case FileAction::Added:
                std::cout << "\n[EVENT] TXT file added: " << path << std::endl;
                assetManager.LoadAsset(path);
                break;
            case FileAction::Modified:
                std::cout << "\n[EVENT] TXT file modified: " << path << std::endl;
                assetManager.ReloadAsset(path);
                break;
            case FileAction::Deleted:
                std::cout << "\n[EVENT] TXT file deleted: " << path << std::endl;
                assetManager.UnloadAsset(path);
                break;
        }
    });

    watcher.RegisterCallback(".json", [&](const std::string& path, FileAction action) {
        switch (action) {
            case FileAction::Added:
                std::cout << "\n[EVENT] JSON file added: " << path << std::endl;
                assetManager.LoadAsset(path);
                break;
            case FileAction::Modified:
                std::cout << "\n[EVENT] JSON file modified: " << path << std::endl;
                assetManager.ReloadAsset(path);
                break;
            case FileAction::Deleted:
                std::cout << "\n[EVENT] JSON file deleted: " << path << std::endl;
                assetManager.UnloadAsset(path);
                break;
        }
    });

    // Watch the assets directory
    watcher.WatchDirectory("example_assets", true);

    std::cout << std::endl;
    std::cout << "===============================================" << std::endl;
    std::cout << "Hot-Reload Demo Running!" << std::endl;
    std::cout << "===============================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Instructions:" << std::endl;
    std::cout << "  1. Try editing files in example_assets/" << std::endl;
    std::cout << "  2. Save the files to see hot-reload in action" << std::endl;
    std::cout << "  3. Create new .txt or .json files" << std::endl;
    std::cout << "  4. Delete files to see cleanup" << std::endl;
    std::cout << "  5. Press 'P' to print asset stats" << std::endl;
    std::cout << "  6. Press ESC or close window to exit" << std::endl;
    std::cout << std::endl;

    // Main loop
    u64 frameCount = 0;
    while (!window.ShouldClose()) {
        Time::Update();
        Input::Update();
        window.PollEvents();

        // Update file watcher (checks for file changes)
        watcher.Update();

        // Print stats when 'P' is pressed
        if (Input::IsKeyPressed(KeyCode::P)) {
            assetManager.PrintStats();
        }

        // Exit on ESC
        if (Input::IsKeyPressed(KeyCode::Escape)) {
            std::cout << "\n[EXIT] ESC pressed, closing..." << std::endl;
            break;
        }

        // Update window title with FPS every 60 frames
        if (frameCount % 60 == 0) {
            f32 fps = Time::FPS();
            std::string title = "File Watcher Example - FPS: " +
                                std::to_string(static_cast<i32>(fps));
            window.SetTitle(title);
        }

        frameCount++;

        // Small sleep to avoid maxing out CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Print final stats
    std::cout << std::endl;
    assetManager.PrintStats();

    std::cout << std::endl;
    std::cout << "===============================================" << std::endl;
    std::cout << "Hot-Reload Demo Complete!" << std::endl;
    std::cout << "Total runtime: " << Time::TotalTime() << " seconds" << std::endl;
    std::cout << "Total frames: " << Time::FrameCount() << std::endl;
    std::cout << "===============================================" << std::endl;

    return 0;
}
