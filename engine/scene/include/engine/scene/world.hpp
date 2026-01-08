#pragma once

#include <engine/scene/entity.hpp>
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <utility>

namespace engine::scene {

// World manages entities and their components using EnTT
// Thread Safety Notes:
// - EnTT registry is NOT thread-safe for concurrent modifications
// - Read-only views are safe to iterate in parallel
// - All entity creation/destruction must happen on main thread
// - Component modifications must be synchronized or deferred
class World {
public:
    World() = default;
    ~World() = default;

    // Non-copyable but movable
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = default;
    World& operator=(World&&) = default;

    // Entity management
    Entity create();
    Entity create(const std::string& name);
    void destroy(Entity e);
    bool valid(Entity e) const;

    // Component management
    template<typename T, typename... Args>
    decltype(auto) emplace(Entity e, Args&&... args) {
        return m_registry.emplace_or_replace<T>(e, std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    decltype(auto) emplace_or_replace(Entity e, Args&&... args) {
        return m_registry.emplace_or_replace<T>(e, std::forward<Args>(args)...);
    }

    template<typename T>
    void remove(Entity e) {
        m_registry.remove<T>(e);
    }

    template<typename T>
    T& get(Entity e) {
        return m_registry.get<T>(e);
    }

    template<typename T>
    const T& get(Entity e) const {
        return m_registry.get<T>(e);
    }

    template<typename T>
    T* try_get(Entity e) {
        return m_registry.try_get<T>(e);
    }

    template<typename T>
    const T* try_get(Entity e) const {
        return m_registry.try_get<T>(e);
    }

    template<typename T>
    bool has(Entity e) const {
        return m_registry.all_of<T>(e);
    }

    template<typename... Ts>
    bool has_all(Entity e) const {
        return m_registry.all_of<Ts...>(e);
    }

    template<typename... Ts>
    bool has_any(Entity e) const {
        return m_registry.any_of<Ts...>(e);
    }

    // View creation for iteration
    template<typename... Ts>
    auto view() {
        return m_registry.view<Ts...>();
    }

    template<typename... Ts>
    auto view() const {
        return m_registry.view<Ts...>();
    }

    // Group creation for performance-critical iteration
    template<typename... Owned, typename... Get, typename... Exclude>
    auto group(entt::get_t<Get...> = {}, entt::exclude_t<Exclude...> = {}) {
        return m_registry.group<Owned...>(entt::get<Get...>, entt::exclude<Exclude...>);
    }

    // Direct registry access for advanced use
    entt::registry& registry() { return m_registry; }
    const entt::registry& registry() const { return m_registry; }

    // Entity count
    size_t size() const {
        const auto* storage = m_registry.storage<entt::entity>();
        return storage ? storage->free_list() : 0;
    }
    bool empty() const { return size() == 0; }

    // Clear all entities
    void clear();

    // Find entity by name (slow, for editor use)
    Entity find_by_name(const std::string& name) const;

    // Scene metadata
    const std::string& get_scene_name() const { return m_scene_name; }
    void set_scene_name(const std::string& name) { m_scene_name = name; }
    
    const std::unordered_map<std::string, std::string>& get_scene_metadata() const { return m_scene_metadata; }
    std::unordered_map<std::string, std::string>& get_scene_metadata() { return m_scene_metadata; }

private:
    entt::registry m_registry;
    uint64_t m_next_uuid = 1;
    
    // Scene metadata
    std::string m_scene_name = "Untitled";
    std::unordered_map<std::string, std::string> m_scene_metadata;
};

} // namespace engine::scene
