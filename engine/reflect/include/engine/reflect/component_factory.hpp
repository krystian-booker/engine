#pragma once

#include <engine/reflect/type_registry.hpp>
#include <entt/entt.hpp>
#include <string>

namespace engine::reflect {

// Factory for creating components at runtime by name
class ComponentFactory {
public:
    static ComponentFactory& instance();

    // Create a component by type name and add to entity
    // Returns the meta_any of the created component, or empty if failed
    entt::meta_any create(entt::registry& registry, entt::entity entity, const std::string& type_name);

    // Create a default-constructed component as meta_any (not attached to entity)
    entt::meta_any create_default(const std::string& type_name);

    // Remove a component by type name from an entity
    bool remove(entt::registry& registry, entt::entity entity, const std::string& type_name);

    // Check if an entity has a component by type name
    bool has(entt::registry& registry, entt::entity entity, const std::string& type_name);

    // Get a component as meta_any
    entt::meta_any get(entt::registry& registry, entt::entity entity, const std::string& type_name);

    // Get a component as meta_any (const version)
    entt::meta_any get(const entt::registry& registry, entt::entity entity, const std::string& type_name) const;

    // Set a property on a component
    bool set_property(entt::registry& registry, entt::entity entity,
                      const std::string& type_name, const std::string& prop_name,
                      const entt::meta_any& value);

    // Get a property from a component
    entt::meta_any get_property(entt::registry& registry, entt::entity entity,
                                const std::string& type_name, const std::string& prop_name);

    // Check if type is a registered component
    bool is_component(const std::string& type_name) const;

    // Get all registered component names
    std::vector<std::string> get_component_names() const;

private:
    ComponentFactory() = default;
    ~ComponentFactory() = default;
    ComponentFactory(const ComponentFactory&) = delete;
    ComponentFactory& operator=(const ComponentFactory&) = delete;
};

// Helper functions for common operations

// Clone a component from one entity to another
bool clone_component(entt::registry& registry,
                     entt::entity src, entt::entity dst,
                     const std::string& type_name);

// Clone all components from one entity to another
void clone_all_components(entt::registry& registry,
                          entt::entity src, entt::entity dst);

// Copy component data from meta_any to component on entity
bool apply_component_data(entt::registry& registry, entt::entity entity,
                          const std::string& type_name, const entt::meta_any& data);

} // namespace engine::reflect
