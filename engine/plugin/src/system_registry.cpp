#include <engine/plugin/system_registry.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <mutex>

namespace engine::plugin {

SystemRegistry::SystemRegistry() = default;
SystemRegistry::~SystemRegistry() = default;

void SystemRegistry::set_engine_scheduler(scene::Scheduler* scheduler) {
    m_engine_scheduler = scheduler;
}

void SystemRegistry::add(scene::Phase phase, scene::SystemFn fn, const std::string& name, int priority) {
    std::unique_lock<std::shared_mutex> lock(m_systems_mutex);

    m_game_scheduler.add(phase, std::move(fn), name, priority);
    m_game_system_names.push_back(name);
    std::string message = "Registered game system: " + name +
                          " (phase " + std::to_string(static_cast<int>(phase)) +
                          ", priority " + std::to_string(priority) + ")";
    core::log(core::LogLevel::Debug, message.c_str());
}

void SystemRegistry::remove(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(m_systems_mutex);

    m_game_scheduler.remove(name);
    m_game_system_names.erase(
        std::remove(m_game_system_names.begin(), m_game_system_names.end(), name),
        m_game_system_names.end()
    );
}

void SystemRegistry::clear_game_systems() {
    std::unique_lock<std::shared_mutex> lock(m_systems_mutex);

    std::string message = "Clearing " + std::to_string(m_game_system_names.size()) +
                          " game systems for hot reload";
    core::log(core::LogLevel::Info, message.c_str());

    for (const auto& name : m_game_system_names) {
        m_game_scheduler.remove(name);
    }
    m_game_system_names.clear();
    m_game_scheduler.clear();
}

void SystemRegistry::run(scene::World& world, double dt, scene::Phase phase) {
    std::shared_lock<std::shared_mutex> lock(m_systems_mutex);

    // Run engine systems first
    if (m_engine_scheduler) {
        m_engine_scheduler->run(world, dt, phase);
    }

    // Then run game systems
    m_game_scheduler.run(world, dt, phase);
}

void SystemRegistry::set_enabled(const std::string& name, bool enabled) {
    m_game_scheduler.set_enabled(name, enabled);
}

bool SystemRegistry::is_enabled(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(m_systems_mutex);
    return m_game_scheduler.is_enabled(name);
}

std::vector<std::string> SystemRegistry::get_game_system_names() const {
    std::shared_lock<std::shared_mutex> lock(m_systems_mutex);
    return m_game_system_names;
}

size_t SystemRegistry::game_system_count() const {
    std::shared_lock<std::shared_mutex> lock(m_systems_mutex);
    return m_game_system_names.size();
}

} // namespace engine::plugin
