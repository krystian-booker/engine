#pragma once

#include <engine/save/save_game.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/serialize.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <future>
#include <map>
#include <unordered_map>
#include <typeindex>

namespace engine::save {

using namespace engine::core;
using namespace engine::scene;

// Forward declarations
class SaveSystem;

// Saveable component - marks an entity for persistence
struct Saveable {
    uint64_t persistent_id = 0;       // Unique ID for save/load matching
    bool save_transform = true;       // Save LocalTransform component
    bool save_components = true;      // Save other registered components
    bool destroy_on_load = true;      // Destroy this entity before loading

    // Components to exclude from saving (by type name)
    std::vector<std::string> excluded_components;

    // Generate a new persistent ID
    static uint64_t generate_id();
};

// Custom save handler interface
class ISaveHandler {
public:
    virtual ~ISaveHandler() = default;

    // Get unique type name for this handler
    virtual std::string get_type_name() const = 0;

    // Called during save
    virtual void on_save(SaveGame& save, World& world) = 0;

    // Called during load (before entities are loaded)
    virtual void on_pre_load(const SaveGame& save, World& world) {}

    // Called during load (after entities are loaded)
    virtual void on_post_load(const SaveGame& save, World& world) = 0;
};

// Component serializer function types
using ComponentSerializeFunc = std::function<void(JsonArchive&, void*)>;
using ComponentDeserializeFunc = std::function<void(JsonArchive&, void*)>;
using ComponentCreateFunc = std::function<void*(Entity, World&)>;
using ComponentGetFunc = std::function<void*(Entity, World&)>;  // Returns nullptr if not present

// Component serializer registration
struct ComponentSerializer {
    std::string type_name;
    ComponentSerializeFunc serialize;
    ComponentDeserializeFunc deserialize;
    ComponentCreateFunc create;
    ComponentGetFunc get;  // Get existing component (nullptr if not present)
    std::type_index type_id;
};

// Save migration function: takes SaveGame and source version, returns success
using SaveMigrationFunc = std::function<bool(SaveGame&, uint32_t)>;

// Save operation result
struct SaveResult {
    bool success = false;
    std::string error_message;
    std::string slot_name;
    float save_time_ms = 0.0f;
};

// Load operation result
struct LoadResult {
    bool success = false;
    std::string error_message;
    std::string slot_name;
    float load_time_ms = 0.0f;
    uint32_t entities_loaded = 0;
};

// Save system configuration
struct SaveSystemConfig {
    std::string save_directory = "saves";
    std::string save_extension = ".sav";
    std::string quick_save_slot = "quicksave";
    std::string autosave_slot = "autosave";
    float autosave_interval = 300.0f;  // 5 minutes
    int max_autosaves = 3;             // Rotate through autosave slots
};

// Save system manager
class SaveSystem {
public:
    SaveSystem() = default;
    ~SaveSystem() = default;

    // Non-copyable
    SaveSystem(const SaveSystem&) = delete;
    SaveSystem& operator=(const SaveSystem&) = delete;

    // Initialize with configuration
    void init(const SaveSystemConfig& config);
    void shutdown();

    // Register custom save handlers
    void register_handler(std::unique_ptr<ISaveHandler> handler);
    void unregister_handler(const std::string& type_name);

    // Register component serializers
    template<typename T>
    void register_component(
        const std::string& type_name,
        std::function<void(JsonArchive&, T&)> serializer
    );

    // Save operations
    SaveResult save_game(World& world, const std::string& slot_name);
    SaveResult save_game(World& world, SaveGame& save);

    // Load operations
    LoadResult load_game(World& world, const std::string& slot_name);
    LoadResult load_game(World& world, const SaveGame& save);

    // Quick save/load (uses a fixed slot)
    SaveResult quick_save(World& world);
    LoadResult quick_load(World& world);

    // Autosave
    void enable_autosave(bool enabled) { m_autosave_enabled = enabled; }
    bool is_autosave_enabled() const { return m_autosave_enabled; }
    void trigger_autosave(World& world);
    void update_autosave(World& world, float dt);

    // List available saves
    std::vector<SaveGameMetadata> list_saves() const;
    bool delete_save(const std::string& slot_name);
    bool save_exists(const std::string& slot_name) const;

    // Save version migrations
    // Register a migration from one version to the next
    // Migrations are applied sequentially: v1 -> v2 -> v3 -> ... -> current
    void register_migration(uint32_t from_version, SaveMigrationFunc migration);
    void clear_migrations();

    // Get save file path
    std::string get_save_path(const std::string& slot_name) const;

    // Async save/load
    // IMPORTANT: Caller must ensure `world` remains valid until the future completes.
    // These operations capture `world` by reference for efficiency.
    std::future<SaveResult> save_game_async(World& world, const std::string& slot_name);
    std::future<LoadResult> load_game_async(World& world, const std::string& slot_name);

    // Events/callbacks
    using SaveCallback = std::function<void(const SaveResult&)>;
    using LoadCallback = std::function<void(const LoadResult&)>;
    void set_save_callback(SaveCallback callback) { m_save_callback = callback; }
    void set_load_callback(LoadCallback callback) { m_load_callback = callback; }

    // Progress tracking for UI
    float get_save_progress() const { return m_save_progress; }
    float get_load_progress() const { return m_load_progress; }
    bool is_saving() const { return m_is_saving; }
    bool is_loading() const { return m_is_loading; }

    // Play time tracking
    void start_play_time_tracking();
    void pause_play_time_tracking();
    uint32_t get_current_play_time() const;

    // Configuration access
    const SaveSystemConfig& get_config() const { return m_config; }

private:
    void save_entities(World& world, SaveGame& save);
    void load_entities(World& world, const SaveGame& save);
    void generate_persistent_ids(World& world);
    void call_pre_load_handlers(const SaveGame& save, World& world);
    void call_post_load_handlers(const SaveGame& save, World& world);
    void call_save_handlers(SaveGame& save, World& world);
    bool apply_migrations(SaveGame& save);

    SaveSystemConfig m_config;
    std::vector<std::unique_ptr<ISaveHandler>> m_handlers;
    std::unordered_map<std::string, ComponentSerializer> m_component_serializers;
    std::map<uint32_t, SaveMigrationFunc> m_migrations;

    // Autosave state
    bool m_autosave_enabled = false;
    float m_autosave_timer = 0.0f;
    int m_autosave_index = 0;

    // Callbacks
    SaveCallback m_save_callback;
    LoadCallback m_load_callback;

    // Progress tracking
    float m_save_progress = 0.0f;
    float m_load_progress = 0.0f;
    bool m_is_saving = false;
    bool m_is_loading = false;

    // Play time tracking
    bool m_tracking_play_time = false;
    std::chrono::steady_clock::time_point m_play_time_start;
    uint32_t m_accumulated_play_time = 0;

    bool m_initialized = false;
};

// Template implementation
template<typename T>
void SaveSystem::register_component(
    const std::string& type_name,
    std::function<void(JsonArchive&, T&)> serializer
) {
    ComponentSerializer cs;
    cs.type_name = type_name;
    cs.type_id = std::type_index(typeid(T));

    cs.serialize = [serializer](JsonArchive& archive, void* ptr) {
        serializer(archive, *static_cast<T*>(ptr));
    };

    cs.deserialize = [serializer](JsonArchive& archive, void* ptr) {
        serializer(archive, *static_cast<T*>(ptr));
    };

    cs.create = [](Entity e, World& world) -> void* {
        return &world.emplace<T>(e);
    };

    cs.get = [](Entity e, World& world) -> void* {
        auto& registry = world.registry();
        if (auto* component = registry.try_get<T>(e)) {
            return component;
        }
        return nullptr;
    };

    m_component_serializers[type_name] = cs;
}

// Global save system instance
SaveSystem& get_save_system();

} // namespace engine::save
