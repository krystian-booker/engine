#pragma once

#include <engine/plugin/game_interface.hpp>

// Main game class
// Implement game initialization, system registration, and lifecycle methods
class MyGame {
public:
    // Plugin metadata - returned to engine for version checking
    static engine::plugin::PluginInfo get_info() {
        return {
            "{{PROJECT_NAME}}",     // Plugin name
            "1.0.0",                // Plugin version
            ENGINE_VERSION          // Required engine version
        };
    }

    // Register custom components with the reflection system
    static void register_components();

    // Called once when the game DLL is loaded
    bool init(engine::plugin::GameContext* ctx);

    // Register game systems with the scheduler
    void register_systems(engine::plugin::SystemRegistry* reg);

    // Called before hot reload - save any state that won't survive
    void pre_reload(engine::scene::World* world, void* state);

    // Called after hot reload - restore saved state
    void post_reload(engine::scene::World* world, const void* state);

    // Called when the game is shutting down
    void shutdown();

private:
    engine::plugin::GameContext* m_ctx = nullptr;
};

// This macro generates all the exported functions the engine expects
IMPLEMENT_GAME_PLUGIN(MyGame)
