#include <engine/script/script_context.hpp>
#include <engine/core/log.hpp>

namespace engine::script {

// Global script context - initialized once from Application
static ScriptContext s_context;
static bool s_initialized = false;

// Thread-local world override for per-frame multi-world support
static thread_local scene::World* s_current_world = nullptr;

void init_script_context(scene::World* world, physics::PhysicsWorld* physics) {
    s_context.world = world;
    s_context.physics_world = physics;
    s_initialized = true;

    core::log(core::LogLevel::Debug, "Script context initialized");
}

void shutdown_script_context() {
    s_context.world = nullptr;
    s_context.physics_world = nullptr;
    s_initialized = false;
    s_current_world = nullptr;

    core::log(core::LogLevel::Debug, "Script context shutdown");
}

ScriptContext& get_script_context() {
    if (!s_initialized) {
        core::log(core::LogLevel::Error,
            "Script context not initialized! Call init_script_context() from Application first.");
    }
    return s_context;
}

bool is_script_context_initialized() {
    return s_initialized;
}

void set_current_script_world(scene::World* world) {
    s_current_world = world;
}

scene::World* get_current_script_world() {
    // Return per-frame override if set, otherwise fall back to global context
    if (s_current_world) {
        return s_current_world;
    }
    return s_context.world;
}

} // namespace engine::script
