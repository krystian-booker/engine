#pragma once

#include <cstdint>
#include <nlohmann/json_fwd.hpp>

// Platform-specific export macro
#ifdef _WIN32
    #define GAME_API extern "C" __declspec(dllexport)
    #define GAME_IMPORT extern "C" __declspec(dllimport)
#else
    #define GAME_API extern "C" __attribute__((visibility("default")))
    #define GAME_IMPORT extern "C"
#endif

// Forward declarations to avoid including full headers
namespace engine::scene {
    class World;
    class Scheduler;
}

namespace engine::render {
    class IRenderer;
}

namespace engine::core {
    class Application;
}

namespace engine::ui {
    class UIContext;
}

namespace engine::plugin {

// Engine version for compatibility checking
constexpr uint32_t ENGINE_VERSION_MAJOR = 1;
constexpr uint32_t ENGINE_VERSION_MINOR = 0;
constexpr uint32_t ENGINE_VERSION_PATCH = 0;
constexpr uint32_t ENGINE_VERSION = (ENGINE_VERSION_MAJOR << 16) | (ENGINE_VERSION_MINOR << 8) | ENGINE_VERSION_PATCH;

// Forward declaration
class SystemRegistry;

// Plugin metadata returned by game_get_info()
struct PluginInfo {
    const char* name;           // Game name
    const char* version;        // Game version string
    uint32_t engine_version;    // Required engine version (use ENGINE_VERSION)
};

// Context passed to game initialization
struct GameContext {
    scene::World* world;            // ECS world
    scene::Scheduler* scheduler;    // Engine scheduler (for reference)
    render::IRenderer* renderer;    // Renderer interface
    ui::UIContext* ui_context;      // UI system context
    core::Application* app;         // Application instance
    const char* project_path;       // Path to project directory
};

// Function pointer types for dynamic loading
using GetInfoFn = PluginInfo(*)();
using InitFn = bool(*)(GameContext*);
using RegisterSystemsFn = void(*)(SystemRegistry*);
using RegisterComponentsFn = void(*)();
using PreReloadFn = void(*)(scene::World*, void* state);
using PostReloadFn = void(*)(scene::World*, const void* state);
using ShutdownFn = void(*)();

// Expected export names
constexpr const char* EXPORT_GET_INFO = "game_get_info";
constexpr const char* EXPORT_INIT = "game_init";
constexpr const char* EXPORT_REGISTER_SYSTEMS = "game_register_systems";
constexpr const char* EXPORT_REGISTER_COMPONENTS = "game_register_components";
constexpr const char* EXPORT_PRE_RELOAD = "game_pre_reload";
constexpr const char* EXPORT_POST_RELOAD = "game_post_reload";
constexpr const char* EXPORT_SHUTDOWN = "game_shutdown";

} // namespace engine::plugin

// ============================================================================
// MACRO FOR IMPLEMENTING GAME PLUGIN
// ============================================================================
// Users implement a Game class and use this macro to generate exports:
//
// class MyGame {
// public:
//     static engine::plugin::PluginInfo get_info();
//     static void register_components();
//     bool init(engine::plugin::GameContext* ctx);
//     void register_systems(engine::plugin::SystemRegistry* reg);
//     void pre_reload(engine::scene::World* world, nlohmann::json& state);
//     void post_reload(engine::scene::World* world, const nlohmann::json& state);
//     void shutdown();
// };
//
// IMPLEMENT_GAME_PLUGIN(MyGame)
// ============================================================================

#define IMPLEMENT_GAME_PLUGIN(GameClass) \
    static GameClass* g_game_instance = nullptr; \
    \
    GAME_API engine::plugin::PluginInfo game_get_info() { \
        return GameClass::get_info(); \
    } \
    \
    GAME_API bool game_init(engine::plugin::GameContext* ctx) { \
        if (g_game_instance) { \
            return false; \
        } \
        g_game_instance = new GameClass(); \
        if (!g_game_instance->init(ctx)) { \
            delete g_game_instance; \
            g_game_instance = nullptr; \
            return false; \
        } \
        return true; \
    } \
    \
    GAME_API void game_register_systems(engine::plugin::SystemRegistry* reg) { \
        if (g_game_instance) { \
            g_game_instance->register_systems(reg); \
        } \
    } \
    \
    GAME_API void game_register_components() { \
        GameClass::register_components(); \
    } \
    \
    GAME_API void game_pre_reload(engine::scene::World* world, void* state) { \
        if (g_game_instance && state) { \
            g_game_instance->pre_reload(world, *static_cast<nlohmann::json*>(state)); \
        } \
    } \
    \
    GAME_API void game_post_reload(engine::scene::World* world, const void* state) { \
        if (g_game_instance && state) { \
            g_game_instance->post_reload(world, *static_cast<const nlohmann::json*>(state)); \
        } \
    } \
    \
    GAME_API void game_shutdown() { \
        if (g_game_instance) { \
            g_game_instance->shutdown(); \
            delete g_game_instance; \
            g_game_instance = nullptr; \
        } \
    }
