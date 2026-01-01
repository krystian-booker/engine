#pragma once

#include <engine/scene/world.hpp>
#include <functional>
#include <vector>
#include <string>

namespace engine::scene {

// System execution phases
enum class Phase {
    PreUpdate,      // Before fixed update (input processing, etc.)
    FixedUpdate,    // Fixed timestep update (physics, AI, etc.)
    Update,         // Variable timestep update (animations, etc.)
    PostUpdate,     // After update (late updates, constraints)
    PreRender,      // Before rendering (transform interpolation, culling)
    Render,         // Rendering
    PostRender      // After rendering (cleanup, debug drawing)
};

// System function signature
using SystemFn = std::function<void(World&, double)>;

// System scheduler manages system registration and execution
class Scheduler {
public:
    Scheduler() = default;

    // Register a system for a specific phase
    // Higher priority systems run first (default 0)
    void add(Phase phase, SystemFn fn, int priority = 0);
    void add(Phase phase, SystemFn fn, const std::string& name, int priority = 0);

    // Remove a system by name
    void remove(const std::string& name);

    // Run all systems for a specific phase
    void run(World& world, double dt, Phase phase);

    // Clear all systems
    void clear();

    // Enable/disable a system by name
    void set_enabled(const std::string& name, bool enabled);
    bool is_enabled(const std::string& name) const;

private:
    struct SystemEntry {
        int priority;
        SystemFn fn;
        std::string name;
        bool enabled = true;
    };

    // One vector of systems per phase
    std::vector<SystemEntry> m_systems[7];

    // Sort systems by priority after adding
    void sort_phase(Phase phase);
};

} // namespace engine::scene
