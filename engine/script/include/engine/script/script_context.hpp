#pragma once

namespace engine::scene {
    class World;
}

namespace engine::physics {
    class PhysicsWorld;
}

namespace engine::script {

// ScriptContext holds pointers to all engine subsystems that scripts need access to.
// This is initialized once from Application after all systems are created.
// Audio and Navigation already use global accessors, so they're not included here.
struct ScriptContext {
    scene::World* world = nullptr;
    physics::PhysicsWorld* physics_world = nullptr;
};

// Initialize the global script context - call once from Application after all systems are created.
// This must be called before any scripts execute or bindings will fail.
void init_script_context(scene::World* world, physics::PhysicsWorld* physics);

// Shutdown and clear the script context
void shutdown_script_context();

// Get the global script context - used by all bindings
// Returns reference to context; will log error and return empty context if not initialized
ScriptContext& get_script_context();

// Check if script context has been initialized
bool is_script_context_initialized();

// Per-frame world override for multi-world support.
// The script system sets this at the start of each update phase.
// Bindings should prefer this over get_script_context().world when set.
void set_current_script_world(scene::World* world);
scene::World* get_current_script_world();

} // namespace engine::script
