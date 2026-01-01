#pragma once

#include <engine/scene/systems.hpp>
#include <string>
#include <vector>
#include <functional>

namespace engine::plugin {

// System registration for game plugins
// Separates game systems from engine systems to support hot reload
class SystemRegistry {
public:
    SystemRegistry();
    ~SystemRegistry();

    // Set the engine's base scheduler (called by engine during init)
    void set_engine_scheduler(scene::Scheduler* scheduler);

    // Register a game system
    // These are tracked separately and can be cleared on hot reload
    void add(scene::Phase phase, scene::SystemFn fn, const std::string& name, int priority = 0);

    // Remove a game system by name
    void remove(const std::string& name);

    // Clear all game systems (called before hot reload)
    void clear_game_systems();

    // Run all systems for a phase (engine + game)
    void run(scene::World& world, double dt, scene::Phase phase);

    // Enable/disable a game system
    void set_enabled(const std::string& name, bool enabled);
    bool is_enabled(const std::string& name) const;

    // Get list of registered game systems
    std::vector<std::string> get_game_system_names() const;

    // Get count of game systems
    size_t game_system_count() const;

private:
    scene::Scheduler* m_engine_scheduler = nullptr;
    scene::Scheduler m_game_scheduler;

    // Track which systems are game systems for clear operation
    std::vector<std::string> m_game_system_names;
};

} // namespace engine::plugin
