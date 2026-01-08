#pragma once

#include <engine/core/uuid.hpp>
#include <engine/core/asset_handle.hpp>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <vector>

namespace engine::asset {

using namespace engine::core;

/**
 * @brief Metadata about a registered asset
 */
struct AssetMetadata {
    UUID id{};                  // Unique identifier
    AssetType type = AssetType::Unknown; // Asset type (Mesh, Texture, etc.)
    std::string path;           // Current file path
    uint64_t last_modified = 0; // File modification time
    bool is_loaded = false;     // Currently loaded in memory
};

/**
 * @brief Central registry for asset path ↔ UUID mapping
 * 
 * The AssetRegistry provides stable asset identification that survives
 * file renames and moves. Each asset file is assigned a UUID when first
 * imported, and this UUID is stored in a sidecar file or database.
 * 
 * Thread Safety: All public methods are thread-safe.
 * 
 * Example usage:
 * @code
 * auto& registry = get_asset_registry();
 * 
 * // Register a new asset (generates UUID)
 * UUID id = registry.register_asset("assets/player.gltf", AssetType::Mesh);
 * 
 * // Later, find by path
 * auto found = registry.find_by_path("assets/player.gltf");
 * if (found) {
 *     UUID id = *found;
 * }
 * 
 * // Update path when file moves
 * registry.update_path(id, "assets/characters/player.gltf");
 * @endcode
 */
class AssetRegistry {
public:
    AssetRegistry() = default;
    ~AssetRegistry() = default;

    // Non-copyable, non-movable (singleton pattern)
    AssetRegistry(const AssetRegistry&) = delete;
    AssetRegistry& operator=(const AssetRegistry&) = delete;
    AssetRegistry(AssetRegistry&&) = delete;
    AssetRegistry& operator=(AssetRegistry&&) = delete;

    /**
     * @brief Register a new asset (generates UUID if not already registered)
     * @param path File path of the asset
     * @param type Type of the asset
     * @return UUID for the asset (existing or newly generated)
     */
    UUID register_asset(const std::string& path, AssetType type);

    /**
     * @brief Register an asset with explicit UUID (for loading from database)
     * @param id UUID to assign
     * @param path File path of the asset
     * @param type Type of the asset
     */
    void register_asset(UUID id, const std::string& path, AssetType type);

    /**
     * @brief Unregister an asset by UUID
     * @param id UUID of asset to unregister
     */
    void unregister(UUID id);

    /**
     * @brief Unregister an asset by path
     * @param path File path of asset to unregister
     */
    void unregister(const std::string& path);

    /**
     * @brief Find UUID by file path
     * @param path File path to search for
     * @return UUID if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<UUID> find_by_path(const std::string& path) const;

    /**
     * @brief Find asset metadata by UUID
     * @param id UUID to search for
     * @return AssetMetadata if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<AssetMetadata> find_by_id(UUID id) const;

    /**
     * @brief Get file path for a UUID
     * @param id UUID to look up
     * @return File path if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<std::string> get_path(UUID id) const;

    /**
     * @brief Update the file path for an asset (for file moves/renames)
     * @param id UUID of the asset
     * @param new_path New file path
     * @return true if updated successfully, false if UUID not found
     */
    bool update_path(UUID id, const std::string& new_path);

    /**
     * @brief Update the loaded status for an asset
     * @param id UUID of the asset
     * @param loaded Whether the asset is currently loaded
     */
    void set_loaded(UUID id, bool loaded);

    /**
     * @brief Update the last modified timestamp
     * @param id UUID of the asset
     * @param timestamp New modification timestamp
     */
    void set_last_modified(UUID id, uint64_t timestamp);

    /**
     * @brief Get all registered assets
     * @return Vector of all asset metadata
     */
    [[nodiscard]] std::vector<AssetMetadata> get_all() const;

    /**
     * @brief Get all registered assets of a specific type
     * @param type Asset type to filter by
     * @return Vector of matching asset metadata
     */
    [[nodiscard]] std::vector<AssetMetadata> get_by_type(AssetType type) const;

    /**
     * @brief Save registry to a JSON file
     * @param path File path to save to
     * @return true if saved successfully
     */
    bool save_to_file(const std::string& path) const;

    /**
     * @brief Load registry from a JSON file
     * @param path File path to load from
     * @return true if loaded successfully
     */
    bool load_from_file(const std::string& path);

    /**
     * @brief Clear all registered assets
     */
    void clear();

    /**
     * @brief Get total number of registered assets
     */
    [[nodiscard]] size_t count() const;

    /**
     * @brief Get number of registered assets of a specific type
     * @param type Asset type to count
     */
    [[nodiscard]] size_t count_by_type(AssetType type) const;

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<UUID, AssetMetadata> m_assets;
    std::unordered_map<std::string, UUID> m_path_to_id;
};

/**
 * @brief Get the global asset registry instance
 */
AssetRegistry& get_asset_registry();

} // namespace engine::asset
