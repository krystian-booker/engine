#include <engine/save/save_system.hpp>
#include <engine/scene/components.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <random>

namespace engine::save {

namespace fs = std::filesystem;

// Static ID generator for Saveable components
static std::atomic<uint64_t> s_next_persistent_id{1};

uint64_t Saveable::generate_id() {
    // Generate a unique ID using timestamp + random + counter
    static std::random_device rd;
    static std::mt19937_64 gen(rd());

    uint64_t timestamp = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count()
    );
    uint64_t counter = s_next_persistent_id.fetch_add(1);

    // Mix timestamp and counter for uniqueness
    return (timestamp << 20) | (counter & 0xFFFFF);
}

// Global save system instance
static SaveSystem* s_save_system = nullptr;

SaveSystem& get_save_system() {
    if (!s_save_system) {
        static SaveSystem instance;
        s_save_system = &instance;
    }
    return *s_save_system;
}

void SaveSystem::init(const SaveSystemConfig& config) {
    if (m_initialized) return;

    m_config = config;

    // Create save directory if it doesn't exist
    fs::path save_dir(m_config.save_directory);
    if (!fs::exists(save_dir)) {
        fs::create_directories(save_dir);
    }

    m_initialized = true;
}

void SaveSystem::shutdown() {
    m_handlers.clear();
    m_component_serializers.clear();
    m_autosave_enabled = false;
    m_initialized = false;
}

void SaveSystem::register_handler(std::unique_ptr<ISaveHandler> handler) {
    if (!handler) return;

    // Remove existing handler with same name
    unregister_handler(handler->get_type_name());

    m_handlers.push_back(std::move(handler));
}

void SaveSystem::unregister_handler(const std::string& type_name) {
    m_handlers.erase(
        std::remove_if(m_handlers.begin(), m_handlers.end(),
            [&type_name](const auto& h) {
                return h->get_type_name() == type_name;
            }
        ),
        m_handlers.end()
    );
}

SaveResult SaveSystem::save_game(World& world, const std::string& slot_name) {
    SaveResult result;
    result.slot_name = slot_name;

    auto start_time = std::chrono::high_resolution_clock::now();
    m_is_saving = true;
    m_save_progress = 0.0f;

    try {
        SaveGame save;

        // Set metadata
        save.metadata().name = slot_name;
        save.metadata().timestamp = static_cast<uint64_t>(std::time(nullptr));
        save.metadata().play_time_seconds = get_current_play_time();

        m_save_progress = 0.1f;

        // Generate persistent IDs for any entities that don't have them
        generate_persistent_ids(world);

        m_save_progress = 0.2f;

        // Save entities
        save_entities(world, save);

        m_save_progress = 0.6f;

        // Call custom save handlers
        call_save_handlers(save, world);

        m_save_progress = 0.8f;

        // Write to file
        std::string path = get_save_path(slot_name);
        if (!save.save_to_file(path)) {
            result.success = false;
            result.error_message = "Failed to write save file: " + path;
        } else {
            result.success = true;
        }

        m_save_progress = 1.0f;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Save exception: ") + e.what();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.save_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();

    m_is_saving = false;

    if (m_save_callback) {
        m_save_callback(result);
    }

    return result;
}

SaveResult SaveSystem::save_game(World& world, SaveGame& save) {
    SaveResult result;

    auto start_time = std::chrono::high_resolution_clock::now();
    m_is_saving = true;
    m_save_progress = 0.0f;

    try {
        // Update metadata
        save.metadata().timestamp = static_cast<uint64_t>(std::time(nullptr));
        save.metadata().play_time_seconds = get_current_play_time();

        m_save_progress = 0.1f;

        // Generate persistent IDs
        generate_persistent_ids(world);

        m_save_progress = 0.2f;

        // Save entities
        save_entities(world, save);

        m_save_progress = 0.7f;

        // Call custom handlers
        call_save_handlers(save, world);

        m_save_progress = 1.0f;
        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Save exception: ") + e.what();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.save_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();

    m_is_saving = false;

    return result;
}

LoadResult SaveSystem::load_game(World& world, const std::string& slot_name) {
    LoadResult result;
    result.slot_name = slot_name;

    auto start_time = std::chrono::high_resolution_clock::now();
    m_is_loading = true;
    m_load_progress = 0.0f;

    try {
        std::string path = get_save_path(slot_name);

        SaveGame save;
        if (!save.load_from_file(path)) {
            result.success = false;
            result.error_message = "Failed to load save file: " + path;
            m_is_loading = false;
            return result;
        }

        m_load_progress = 0.2f;

        // Call pre-load handlers
        call_pre_load_handlers(save, world);

        m_load_progress = 0.3f;

        // Load entities
        load_entities(world, save);

        m_load_progress = 0.7f;

        // Call post-load handlers
        call_post_load_handlers(save, world);

        m_load_progress = 1.0f;

        result.success = true;
        result.entities_loaded = static_cast<uint32_t>(save.get_all_entity_ids().size());

        // Restore play time
        m_accumulated_play_time = save.metadata().play_time_seconds;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Load exception: ") + e.what();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.load_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();

    m_is_loading = false;

    if (m_load_callback) {
        m_load_callback(result);
    }

    return result;
}

LoadResult SaveSystem::load_game(World& world, const SaveGame& save) {
    LoadResult result;

    auto start_time = std::chrono::high_resolution_clock::now();
    m_is_loading = true;
    m_load_progress = 0.0f;

    try {
        // Call pre-load handlers
        call_pre_load_handlers(save, world);

        m_load_progress = 0.3f;

        // Load entities (need non-const for internal operations)
        SaveGame& mutable_save = const_cast<SaveGame&>(save);
        load_entities(world, mutable_save);

        m_load_progress = 0.7f;

        // Call post-load handlers
        call_post_load_handlers(save, world);

        m_load_progress = 1.0f;

        result.success = true;
        result.entities_loaded = static_cast<uint32_t>(save.get_all_entity_ids().size());

        // Restore play time
        m_accumulated_play_time = save.metadata().play_time_seconds;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Load exception: ") + e.what();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.load_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();

    m_is_loading = false;

    return result;
}

SaveResult SaveSystem::quick_save(World& world) {
    return save_game(world, m_config.quick_save_slot);
}

LoadResult SaveSystem::quick_load(World& world) {
    return load_game(world, m_config.quick_save_slot);
}

void SaveSystem::trigger_autosave(World& world) {
    // Rotate through autosave slots
    std::string slot = m_config.autosave_slot + "_" + std::to_string(m_autosave_index);
    m_autosave_index = (m_autosave_index + 1) % m_config.max_autosaves;

    save_game(world, slot);
}

void SaveSystem::update_autosave(World& world, float dt) {
    if (!m_autosave_enabled) return;

    m_autosave_timer += dt;
    if (m_autosave_timer >= m_config.autosave_interval) {
        m_autosave_timer = 0.0f;
        trigger_autosave(world);
    }
}

std::vector<SaveGameMetadata> SaveSystem::list_saves() const {
    std::vector<SaveGameMetadata> saves;

    fs::path save_dir(m_config.save_directory);
    if (!fs::exists(save_dir)) return saves;

    for (const auto& entry : fs::directory_iterator(save_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == m_config.save_extension) {
            SaveGame save;
            if (save.load_from_file(entry.path().string())) {
                saves.push_back(save.metadata());
            }
        }
    }

    // Sort by timestamp (newest first)
    std::sort(saves.begin(), saves.end(),
        [](const auto& a, const auto& b) {
            return a.timestamp > b.timestamp;
        }
    );

    return saves;
}

bool SaveSystem::delete_save(const std::string& slot_name) {
    std::string path = get_save_path(slot_name);
    return fs::remove(path);
}

bool SaveSystem::save_exists(const std::string& slot_name) const {
    std::string path = get_save_path(slot_name);
    return fs::exists(path);
}

std::string SaveSystem::get_save_path(const std::string& slot_name) const {
    fs::path save_dir(m_config.save_directory);
    fs::path save_file = save_dir / (slot_name + m_config.save_extension);
    return save_file.string();
}

std::future<SaveResult> SaveSystem::save_game_async(World& world, const std::string& slot_name) {
    return std::async(std::launch::async, [this, &world, slot_name]() {
        return save_game(world, slot_name);
    });
}

std::future<LoadResult> SaveSystem::load_game_async(World& world, const std::string& slot_name) {
    return std::async(std::launch::async, [this, &world, slot_name]() {
        return load_game(world, slot_name);
    });
}

void SaveSystem::start_play_time_tracking() {
    m_tracking_play_time = true;
    m_play_time_start = std::chrono::steady_clock::now();
}

void SaveSystem::pause_play_time_tracking() {
    if (m_tracking_play_time) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_play_time_start);
        m_accumulated_play_time += static_cast<uint32_t>(elapsed.count());
        m_tracking_play_time = false;
    }
}

uint32_t SaveSystem::get_current_play_time() const {
    uint32_t total = m_accumulated_play_time;

    if (m_tracking_play_time) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_play_time_start);
        total += static_cast<uint32_t>(elapsed.count());
    }

    return total;
}

void SaveSystem::save_entities(World& world, SaveGame& save) {
    auto& registry = world.registry();

    // Clear existing entity data
    save.clear_entity_data();

    // Find all entities with Saveable component
    auto view = registry.view<Saveable>();

    for (auto entity : view) {
        const auto& saveable = view.get<Saveable>(entity);

        if (saveable.persistent_id == 0) continue;

        json entity_json;
        entity_json["persistent_id"] = saveable.persistent_id;
        entity_json["save_transform"] = saveable.save_transform;
        entity_json["save_components"] = saveable.save_components;
        entity_json["destroy_on_load"] = saveable.destroy_on_load;

        // Save transform if requested
        if (saveable.save_transform) {
            if (auto* transform = registry.try_get<scene::LocalTransform>(entity)) {
                json transform_json;
                transform_json["position"] = {
                    transform->position.x,
                    transform->position.y,
                    transform->position.z
                };
                transform_json["rotation"] = {
                    transform->rotation.x,
                    transform->rotation.y,
                    transform->rotation.z,
                    transform->rotation.w
                };
                transform_json["scale"] = {
                    transform->scale.x,
                    transform->scale.y,
                    transform->scale.z
                };
                entity_json["transform"] = transform_json;
            }
        }

        // Save registered components
        if (saveable.save_components) {
            json components_json;

            for (const auto& [type_name, serializer] : m_component_serializers) {
                // Check if this component is excluded
                bool excluded = std::find(
                    saveable.excluded_components.begin(),
                    saveable.excluded_components.end(),
                    type_name
                ) != saveable.excluded_components.end();

                if (excluded) continue;

                // TODO: Use reflection or type registry to get component pointer
                // For now, components must be explicitly registered with get functions
            }

            if (!components_json.empty()) {
                entity_json["components"] = components_json;
            }
        }

        save.set_entity_data(saveable.persistent_id, entity_json.dump());
    }
}

void SaveSystem::load_entities(World& world, const SaveGame& save) {
    auto& registry = world.registry();

    // First, destroy entities marked for destruction on load
    {
        auto view = registry.view<Saveable>();
        std::vector<Entity> to_destroy;

        for (auto entity : view) {
            const auto& saveable = view.get<Saveable>(entity);
            if (saveable.destroy_on_load) {
                to_destroy.push_back(entity);
            }
        }

        for (auto entity : to_destroy) {
            world.destroy(entity);
        }
    }

    // Load entity data
    auto entity_ids = save.get_all_entity_ids();

    // Create a map of persistent ID to existing entity
    std::unordered_map<uint64_t, Entity> id_to_entity;
    {
        auto view = registry.view<Saveable>();
        for (auto entity : view) {
            const auto& saveable = view.get<Saveable>(entity);
            if (saveable.persistent_id != 0) {
                id_to_entity[saveable.persistent_id] = entity;
            }
        }
    }

    for (uint64_t persistent_id : entity_ids) {
        std::string entity_data_str = save.get_entity_data(persistent_id);
        if (entity_data_str.empty()) continue;

        try {
            json entity_json = json::parse(entity_data_str);

            // Find or create entity
            Entity entity;
            auto it = id_to_entity.find(persistent_id);
            if (it != id_to_entity.end()) {
                entity = it->second;
            } else {
                // Create new entity
                entity = world.create();
                auto& saveable = registry.emplace<Saveable>(entity);
                saveable.persistent_id = persistent_id;
                saveable.save_transform = entity_json.value("save_transform", true);
                saveable.save_components = entity_json.value("save_components", true);
                saveable.destroy_on_load = entity_json.value("destroy_on_load", true);
            }

            // Load transform
            if (entity_json.contains("transform")) {
                const auto& transform_json = entity_json["transform"];

                auto& transform = registry.get_or_emplace<scene::LocalTransform>(entity);

                if (transform_json.contains("position")) {
                    auto& pos = transform_json["position"];
                    transform.position = Vec3(pos[0], pos[1], pos[2]);
                }

                if (transform_json.contains("rotation")) {
                    auto& rot = transform_json["rotation"];
                    transform.rotation = Quat(rot[0], rot[1], rot[2], rot[3]);
                }

                if (transform_json.contains("scale")) {
                    auto& scl = transform_json["scale"];
                    transform.scale = Vec3(scl[0], scl[1], scl[2]);
                }
            }

            // Load components
            if (entity_json.contains("components")) {
                const auto& components_json = entity_json["components"];

                for (auto& [type_name, component_data] : components_json.items()) {
                    auto serializer_it = m_component_serializers.find(type_name);
                    if (serializer_it != m_component_serializers.end()) {
                        // Create component if needed
                        void* component_ptr = serializer_it->second.create(entity, world);
                        if (component_ptr) {
                            JsonArchive archive(component_data);
                            serializer_it->second.deserialize(archive, component_ptr);
                        }
                    }
                }
            }

        } catch (const std::exception&) {
            // Skip malformed entity data
            continue;
        }
    }
}

void SaveSystem::generate_persistent_ids(World& world) {
    auto& registry = world.registry();
    auto view = registry.view<Saveable>();

    for (auto entity : view) {
        auto& saveable = view.get<Saveable>(entity);
        if (saveable.persistent_id == 0) {
            saveable.persistent_id = Saveable::generate_id();
        }
    }
}

void SaveSystem::call_pre_load_handlers(const SaveGame& save, World& world) {
    for (auto& handler : m_handlers) {
        handler->on_pre_load(save, world);
    }
}

void SaveSystem::call_post_load_handlers(const SaveGame& save, World& world) {
    for (auto& handler : m_handlers) {
        handler->on_post_load(save, world);
    }
}

void SaveSystem::call_save_handlers(SaveGame& save, World& world) {
    for (auto& handler : m_handlers) {
        handler->on_save(save, world);
    }
}

} // namespace engine::save
