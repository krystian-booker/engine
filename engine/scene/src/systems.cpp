#include <engine/scene/systems.hpp>
#include <algorithm>

namespace engine::scene {

void Scheduler::add(Phase phase, SystemFn fn, int priority) {
    add(phase, std::move(fn), "", priority);
}

void Scheduler::add(Phase phase, SystemFn fn, const std::string& name, int priority) {
    auto& systems = m_systems[static_cast<int>(phase)];
    systems.push_back({priority, std::move(fn), name, true});
    sort_phase(phase);
}

void Scheduler::remove(const std::string& name) {
    if (name.empty()) return;

    for (auto& systems : m_systems) {
        systems.erase(
            std::remove_if(systems.begin(), systems.end(),
                [&name](const SystemEntry& entry) { return entry.name == name; }),
            systems.end()
        );
    }
}

void Scheduler::run(World& world, double dt, Phase phase) {
    auto& systems = m_systems[static_cast<int>(phase)];
    for (auto& entry : systems) {
        if (entry.enabled) {
            entry.fn(world, dt);
        }
    }
}

void Scheduler::clear() {
    for (auto& systems : m_systems) {
        systems.clear();
    }
}

void Scheduler::set_enabled(const std::string& name, bool enabled) {
    if (name.empty()) return;

    for (auto& systems : m_systems) {
        for (auto& entry : systems) {
            if (entry.name == name) {
                entry.enabled = enabled;
            }
        }
    }
}

bool Scheduler::is_enabled(const std::string& name) const {
    if (name.empty()) return false;

    for (const auto& systems : m_systems) {
        for (const auto& entry : systems) {
            if (entry.name == name) {
                return entry.enabled;
            }
        }
    }
    return false;
}

void Scheduler::sort_phase(Phase phase) {
    auto& systems = m_systems[static_cast<int>(phase)];
    // Sort by priority (higher priority first)
    std::stable_sort(systems.begin(), systems.end(),
        [](const SystemEntry& a, const SystemEntry& b) {
            return a.priority > b.priority;
        });
}

} // namespace engine::scene
