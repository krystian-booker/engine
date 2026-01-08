#pragma once

#include <engine/core/log.hpp>
#include <engine/core/filesystem.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <fstream>

namespace engine::data {

// ============================================================================
// LoadResult - Result of a JSON loading operation
// ============================================================================

template<typename T>
struct LoadResult {
    std::vector<T> items;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    size_t total_processed = 0;

    bool success() const { return errors.empty(); }
    size_t loaded_count() const { return items.size(); }
    size_t error_count() const { return errors.size(); }
};

// ============================================================================
// JSON Loading Utilities
// ============================================================================

// Load and parse a JSON file
// Returns nullopt on failure
inline std::optional<nlohmann::json> load_json_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        core::log(core::LogLevel::Error, "[JsonLoader] Failed to open file: {}", path);
        return std::nullopt;
    }

    try {
        nlohmann::json j;
        file >> j;
        return j;
    } catch (const nlohmann::json::parse_error& e) {
        core::log(core::LogLevel::Error, "[JsonLoader] Parse error in {}: {}", path, e.what());
        return std::nullopt;
    }
}

// ============================================================================
// load_json_array - Load an array of objects from JSON file
// ============================================================================

// Deserializer function signature:
// std::optional<T> deserialize(const nlohmann::json& obj, std::string& out_error)
// - Returns std::nullopt on failure, with error message in out_error
// - Returns the deserialized object on success

template<typename T, typename Deserializer>
LoadResult<T> load_json_array(
    const std::string& path,
    Deserializer deserialize_fn,
    const std::string& array_key = ""  // Empty = root is array, otherwise look for this key
) {
    LoadResult<T> result;

    // Load and parse the file
    auto json_opt = load_json_file(path);
    if (!json_opt) {
        result.errors.push_back("Failed to load or parse file: " + path);
        return result;
    }

    const nlohmann::json& root = *json_opt;

    // Get the array to iterate
    const nlohmann::json* arr = nullptr;
    if (array_key.empty()) {
        // Root should be an array
        if (!root.is_array()) {
            result.errors.push_back("Expected root to be an array");
            return result;
        }
        arr = &root;
    } else {
        // Look for the specified key
        if (!root.contains(array_key)) {
            result.errors.push_back("Missing key '" + array_key + "' in JSON");
            return result;
        }
        if (!root[array_key].is_array()) {
            result.errors.push_back("Key '" + array_key + "' is not an array");
            return result;
        }
        arr = &root[array_key];
    }

    // Process each item
    result.items.reserve(arr->size());
    size_t index = 0;

    for (const auto& item : *arr) {
        ++result.total_processed;

        if (!item.is_object()) {
            result.warnings.push_back("Item at index " + std::to_string(index) + " is not an object, skipping");
            ++index;
            continue;
        }

        std::string error;
        auto obj_opt = deserialize_fn(item, error);

        if (obj_opt) {
            result.items.push_back(std::move(*obj_opt));
        } else {
            result.errors.push_back("Item " + std::to_string(index) + ": " + error);
        }

        ++index;
    }

    return result;
}

// ============================================================================
// load_and_register - Load from JSON and register each item
// ============================================================================

template<typename T, typename Registry, typename Deserializer, typename RegisterFn>
bool load_and_register(
    const std::string& path,
    Registry& registry,
    Deserializer deserialize_fn,
    RegisterFn register_fn,
    const std::string& array_key = "",
    const std::string& log_category = "JsonLoader"
) {
    auto result = load_json_array<T>(path, deserialize_fn, array_key);

    // Log warnings
    for (const auto& warn : result.warnings) {
        core::log(core::LogLevel::Warn, "[{}] {}", log_category, warn);
    }

    // Log errors
    for (const auto& err : result.errors) {
        core::log(core::LogLevel::Error, "[{}] {}", log_category, err);
    }

    // Register successfully loaded items
    for (auto& item : result.items) {
        register_fn(registry, std::move(item));
    }

    core::log(core::LogLevel::Info, "[{}] Loaded {} items from {} ({} errors)",
              log_category, result.loaded_count(), path, result.error_count());

    return result.success();
}

// ============================================================================
// JSON Value Helpers - Safe extraction with defaults
// ============================================================================

namespace json_helpers {

// Get string with default
inline std::string get_string(const nlohmann::json& j, const std::string& key, const std::string& def = "") {
    if (j.contains(key) && j[key].is_string()) {
        return j[key].get<std::string>();
    }
    return def;
}

// Get int with default
inline int get_int(const nlohmann::json& j, const std::string& key, int def = 0) {
    if (j.contains(key) && j[key].is_number_integer()) {
        return j[key].get<int>();
    }
    return def;
}

// Get float with default
inline float get_float(const nlohmann::json& j, const std::string& key, float def = 0.0f) {
    if (j.contains(key) && j[key].is_number()) {
        return j[key].get<float>();
    }
    return def;
}

// Get bool with default
inline bool get_bool(const nlohmann::json& j, const std::string& key, bool def = false) {
    if (j.contains(key) && j[key].is_boolean()) {
        return j[key].get<bool>();
    }
    return def;
}

// Get enum from int
template<typename E>
E get_enum(const nlohmann::json& j, const std::string& key, E def = E{}) {
    if (j.contains(key) && j[key].is_number_integer()) {
        return static_cast<E>(j[key].get<int>());
    }
    return def;
}

// Get string array
inline std::vector<std::string> get_string_array(const nlohmann::json& j, const std::string& key) {
    std::vector<std::string> result;
    if (j.contains(key) && j[key].is_array()) {
        for (const auto& item : j[key]) {
            if (item.is_string()) {
                result.push_back(item.get<std::string>());
            }
        }
    }
    return result;
}

// Check if required field exists and is correct type
inline bool require_string(const nlohmann::json& j, const std::string& key, std::string& out_error) {
    if (!j.contains(key)) {
        out_error = "Missing required field '" + key + "'";
        return false;
    }
    if (!j[key].is_string()) {
        out_error = "Field '" + key + "' must be a string";
        return false;
    }
    return true;
}

inline bool require_int(const nlohmann::json& j, const std::string& key, std::string& out_error) {
    if (!j.contains(key)) {
        out_error = "Missing required field '" + key + "'";
        return false;
    }
    if (!j[key].is_number_integer()) {
        out_error = "Field '" + key + "' must be an integer";
        return false;
    }
    return true;
}

} // namespace json_helpers

} // namespace engine::data
