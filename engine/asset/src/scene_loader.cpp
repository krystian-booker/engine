#include <engine/asset/scene_loader.hpp>
#include <engine/core/filesystem.hpp>
#include <engine/core/log.hpp>
#include <nlohmann/json.hpp>

namespace engine::asset {

using namespace engine::core;
using json = nlohmann::json;

std::string SceneLoader::s_last_error;

std::shared_ptr<SceneAsset> SceneLoader::load(const std::string& path) {
    s_last_error.clear();

    // Read scene file
    auto content = FileSystem::read_text(path);
    if (content.empty()) {
        s_last_error = "Failed to read scene file: " + path;
        log(LogLevel::Error, s_last_error.c_str());
        return nullptr;
    }

    // Validate JSON
    try {
        json scene_json = json::parse(content);

        // Basic validation - check for required fields
        if (!scene_json.contains("entities") && !scene_json.contains("scene")) {
            s_last_error = "Invalid scene file: missing 'entities' or 'scene' root key";
            log(LogLevel::Error, s_last_error.c_str());
            return nullptr;
        }

        // Check version if present
        if (scene_json.contains("version")) {
            int version = scene_json["version"].get<int>();
            if (version > 1) {
                log(LogLevel::Warn, ("Scene file version " + std::to_string(version) + " may not be fully supported").c_str());
            }
        }

        // Check for entity array
        const json* entities = nullptr;
        if (scene_json.contains("entities")) {
            entities = &scene_json["entities"];
        } else if (scene_json.contains("scene") && scene_json["scene"].contains("entities")) {
            entities = &scene_json["scene"]["entities"];
        }

        if (entities && !entities->is_array()) {
            s_last_error = "Invalid scene file: 'entities' must be an array";
            log(LogLevel::Error, s_last_error.c_str());
            return nullptr;
        }

        // Validate each entity has required fields
        if (entities) {
            for (size_t i = 0; i < entities->size(); i++) {
                const auto& entity = (*entities)[i];
                if (!entity.is_object()) {
                    s_last_error = "Invalid entity at index " + std::to_string(i) + ": must be an object";
                    log(LogLevel::Error, s_last_error.c_str());
                    return nullptr;
                }

                // Entity should have components
                if (!entity.contains("components") && !entity.contains("name")) {
                    log(LogLevel::Warn, ("Entity at index " + std::to_string(i) + " has no components or name").c_str());
                }
            }
        }

    } catch (const json::exception& e) {
        s_last_error = "JSON parse error: " + std::string(e.what());
        log(LogLevel::Error, s_last_error.c_str());
        return nullptr;
    }

    // Create asset with raw JSON data for runtime parsing
    auto asset = std::make_shared<SceneAsset>();
    asset->path = path;
    asset->json_data = std::move(content);

    log(LogLevel::Debug, ("Loaded scene: " + path).c_str());
    return asset;
}

const std::string& SceneLoader::get_last_error() {
    return s_last_error;
}

} // namespace engine::asset
