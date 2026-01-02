#include <engine/asset/prefab_loader.hpp>
#include <engine/core/filesystem.hpp>
#include <engine/core/log.hpp>
#include <nlohmann/json.hpp>

namespace engine::asset {

using namespace engine::core;
using json = nlohmann::json;

std::string PrefabLoader::s_last_error;

std::shared_ptr<PrefabAsset> PrefabLoader::load(const std::string& path) {
    s_last_error.clear();

    // Read prefab file
    auto content = FileSystem::read_text(path);
    if (content.empty()) {
        s_last_error = "Failed to read prefab file: " + path;
        log(LogLevel::Error, s_last_error.c_str());
        return nullptr;
    }

    // Validate JSON
    try {
        json prefab_json = json::parse(content);

        // Basic validation - prefab must have components or be a valid entity template
        if (!prefab_json.contains("components") && !prefab_json.contains("prefab") && !prefab_json.contains("entity")) {
            s_last_error = "Invalid prefab file: missing 'components', 'prefab', or 'entity' root key";
            log(LogLevel::Error, s_last_error.c_str());
            return nullptr;
        }

        // Check version if present
        if (prefab_json.contains("version")) {
            int version = prefab_json["version"].get<int>();
            if (version > 1) {
                log(LogLevel::Warn, ("Prefab file version " + std::to_string(version) + " may not be fully supported").c_str());
            }
        }

        // Get components array/object
        const json* components = nullptr;
        if (prefab_json.contains("components")) {
            components = &prefab_json["components"];
        } else if (prefab_json.contains("prefab") && prefab_json["prefab"].contains("components")) {
            components = &prefab_json["prefab"]["components"];
        } else if (prefab_json.contains("entity") && prefab_json["entity"].contains("components")) {
            components = &prefab_json["entity"]["components"];
        }

        // Validate components structure
        if (components) {
            if (!components->is_array() && !components->is_object()) {
                s_last_error = "Invalid prefab file: 'components' must be an array or object";
                log(LogLevel::Error, s_last_error.c_str());
                return nullptr;
            }

            // If it's an array, validate each component
            if (components->is_array()) {
                for (size_t i = 0; i < components->size(); i++) {
                    const auto& comp = (*components)[i];
                    if (!comp.is_object()) {
                        s_last_error = "Invalid component at index " + std::to_string(i) + ": must be an object";
                        log(LogLevel::Error, s_last_error.c_str());
                        return nullptr;
                    }
                    // Component should have a type
                    if (!comp.contains("type") && !comp.contains("$type")) {
                        log(LogLevel::Warn, ("Component at index " + std::to_string(i) + " has no type specifier").c_str());
                    }
                }
            }
        }

        // Check for nested prefab references
        if (prefab_json.contains("children")) {
            const auto& children = prefab_json["children"];
            if (!children.is_array()) {
                s_last_error = "Invalid prefab file: 'children' must be an array";
                log(LogLevel::Error, s_last_error.c_str());
                return nullptr;
            }

            for (size_t i = 0; i < children.size(); i++) {
                const auto& child = children[i];
                // Child can be either an inline prefab or a reference
                if (child.is_string()) {
                    // Reference to another prefab file - will be resolved at runtime
                    log(LogLevel::Debug, ("Prefab references child: " + child.get<std::string>()).c_str());
                } else if (!child.is_object()) {
                    s_last_error = "Invalid child at index " + std::to_string(i) + ": must be object or string reference";
                    log(LogLevel::Error, s_last_error.c_str());
                    return nullptr;
                }
            }
        }

    } catch (const json::exception& e) {
        s_last_error = "JSON parse error: " + std::string(e.what());
        log(LogLevel::Error, s_last_error.c_str());
        return nullptr;
    }

    // Create asset with raw JSON data for runtime parsing
    auto asset = std::make_shared<PrefabAsset>();
    asset->path = path;
    asset->json_data = std::move(content);

    log(LogLevel::Debug, ("Loaded prefab: " + path).c_str());
    return asset;
}

const std::string& PrefabLoader::get_last_error() {
    return s_last_error;
}

} // namespace engine::asset
