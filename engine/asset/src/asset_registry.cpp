#include <engine/asset/asset_registry.hpp>
#include <engine/core/log.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

namespace engine::asset {

using namespace engine::core;
using json = nlohmann::json;

UUID AssetRegistry::register_asset(const std::string& path, AssetType type) {
    std::unique_lock lock(m_mutex);

    // Check if already registered
    auto it = m_path_to_id.find(path);
    if (it != m_path_to_id.end()) {
        return it->second;
    }

    // Generate new UUID
    UUID id = UUID::generate();

    AssetMetadata meta;
    meta.id = id;
    meta.type = type;
    meta.path = path;
    meta.last_modified = 0;
    meta.is_loaded = false;

    m_assets[id] = meta;
    m_path_to_id[path] = id;

    log(LogLevel::Debug, "AssetRegistry: Registered {} as {}", path, id.to_string());

    return id;
}

void AssetRegistry::register_asset(UUID id, const std::string& path, AssetType type) {
    std::unique_lock lock(m_mutex);

    // Remove any existing entry with this path
    auto path_it = m_path_to_id.find(path);
    if (path_it != m_path_to_id.end()) {
        m_assets.erase(path_it->second);
        m_path_to_id.erase(path_it);
    }

    // Remove any existing entry with this UUID
    auto id_it = m_assets.find(id);
    if (id_it != m_assets.end()) {
        m_path_to_id.erase(id_it->second.path);
        m_assets.erase(id_it);
    }

    AssetMetadata meta;
    meta.id = id;
    meta.type = type;
    meta.path = path;
    meta.last_modified = 0;
    meta.is_loaded = false;

    m_assets[id] = meta;
    m_path_to_id[path] = id;
}

void AssetRegistry::unregister(UUID id) {
    std::unique_lock lock(m_mutex);

    auto it = m_assets.find(id);
    if (it != m_assets.end()) {
        m_path_to_id.erase(it->second.path);
        m_assets.erase(it);
    }
}

void AssetRegistry::unregister(const std::string& path) {
    std::unique_lock lock(m_mutex);

    auto it = m_path_to_id.find(path);
    if (it != m_path_to_id.end()) {
        m_assets.erase(it->second);
        m_path_to_id.erase(it);
    }
}

std::optional<UUID> AssetRegistry::find_by_path(const std::string& path) const {
    std::shared_lock lock(m_mutex);

    auto it = m_path_to_id.find(path);
    if (it != m_path_to_id.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<AssetMetadata> AssetRegistry::find_by_id(UUID id) const {
    std::shared_lock lock(m_mutex);

    auto it = m_assets.find(id);
    if (it != m_assets.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<std::string> AssetRegistry::get_path(UUID id) const {
    std::shared_lock lock(m_mutex);

    auto it = m_assets.find(id);
    if (it != m_assets.end()) {
        return it->second.path;
    }
    return std::nullopt;
}

bool AssetRegistry::update_path(UUID id, const std::string& new_path) {
    std::unique_lock lock(m_mutex);

    auto it = m_assets.find(id);
    if (it == m_assets.end()) {
        return false;
    }

    // Remove old path mapping
    m_path_to_id.erase(it->second.path);

    // Update to new path
    it->second.path = new_path;
    m_path_to_id[new_path] = id;

    log(LogLevel::Debug, "AssetRegistry: Updated path for {} to {}", id.to_string(), new_path);

    return true;
}

void AssetRegistry::set_loaded(UUID id, bool loaded) {
    std::unique_lock lock(m_mutex);

    auto it = m_assets.find(id);
    if (it != m_assets.end()) {
        it->second.is_loaded = loaded;
    }
}

void AssetRegistry::set_last_modified(UUID id, uint64_t timestamp) {
    std::unique_lock lock(m_mutex);

    auto it = m_assets.find(id);
    if (it != m_assets.end()) {
        it->second.last_modified = timestamp;
    }
}

std::vector<AssetMetadata> AssetRegistry::get_all() const {
    std::shared_lock lock(m_mutex);

    std::vector<AssetMetadata> result;
    result.reserve(m_assets.size());
    for (const auto& [id, meta] : m_assets) {
        result.push_back(meta);
    }
    return result;
}

std::vector<AssetMetadata> AssetRegistry::get_by_type(AssetType type) const {
    std::shared_lock lock(m_mutex);

    std::vector<AssetMetadata> result;
    for (const auto& [id, meta] : m_assets) {
        if (meta.type == type) {
            result.push_back(meta);
        }
    }
    return result;
}

bool AssetRegistry::save_to_file(const std::string& path) const {
    std::shared_lock lock(m_mutex);

    try {
        json j;
        j["version"] = 1;
        j["assets"] = json::array();

        for (const auto& [id, meta] : m_assets) {
            json asset_json;
            asset_json["id"] = id.to_string();
            asset_json["type"] = static_cast<int>(meta.type);
            asset_json["path"] = meta.path;
            asset_json["last_modified"] = meta.last_modified;
            j["assets"].push_back(asset_json);
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            log(LogLevel::Error, "AssetRegistry: Failed to open file for writing: {}", path);
            return false;
        }

        file << j.dump(2);
        file.close();

        log(LogLevel::Info, "AssetRegistry: Saved {} assets to {}", m_assets.size(), path);
        return true;

    } catch (const std::exception& e) {
        log(LogLevel::Error, "AssetRegistry: Failed to save: {}", e.what());
        return false;
    }
}

bool AssetRegistry::load_from_file(const std::string& path) {
    std::unique_lock lock(m_mutex);

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            log(LogLevel::Warn, "AssetRegistry: File not found: {}", path);
            return false;
        }

        json j;
        file >> j;
        file.close();

        // Clear existing data
        m_assets.clear();
        m_path_to_id.clear();

        int version = j.value("version", 1);
        (void)version;  // For future compatibility checks

        for (const auto& asset_json : j["assets"]) {
            std::string id_str = asset_json["id"].get<std::string>();
            auto id_opt = UUID::from_string(id_str);
            if (!id_opt) {
                log(LogLevel::Warn, "AssetRegistry: Invalid UUID: {}", id_str);
                continue;
            }

            AssetMetadata meta;
            meta.id = *id_opt;
            meta.type = static_cast<AssetType>(asset_json["type"].get<int>());
            meta.path = asset_json["path"].get<std::string>();
            meta.last_modified = asset_json.value("last_modified", 0ULL);
            meta.is_loaded = false;

            m_assets[meta.id] = meta;
            m_path_to_id[meta.path] = meta.id;
        }

        log(LogLevel::Info, "AssetRegistry: Loaded {} assets from {}", m_assets.size(), path);
        return true;

    } catch (const std::exception& e) {
        log(LogLevel::Error, "AssetRegistry: Failed to load: {}", e.what());
        return false;
    }
}

void AssetRegistry::clear() {
    std::unique_lock lock(m_mutex);
    m_assets.clear();
    m_path_to_id.clear();
}

size_t AssetRegistry::count() const {
    std::shared_lock lock(m_mutex);
    return m_assets.size();
}

size_t AssetRegistry::count_by_type(AssetType type) const {
    std::shared_lock lock(m_mutex);
    return std::count_if(m_assets.begin(), m_assets.end(),
        [type](const auto& pair) { return pair.second.type == type; });
}

// Global instance
AssetRegistry& get_asset_registry() {
    static AssetRegistry instance;
    return instance;
}

} // namespace engine::asset
