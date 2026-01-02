#include <engine/reflect/component_factory.hpp>
#include <engine/core/log.hpp>

namespace engine::reflect {

ComponentFactory& ComponentFactory::instance() {
    static ComponentFactory instance;
    return instance;
}

entt::meta_any ComponentFactory::create(entt::registry& registry, entt::entity entity, const std::string& type_name) {
    auto& type_registry = TypeRegistry::instance();

    auto type = type_registry.find_type(type_name);
    if (!type) {
        core::log(core::LogLevel::Error, "ComponentFactory: Unknown type '{}'", type_name);
        return {};
    }

    auto* type_info = type_registry.get_type_info(type_name);
    if (!type_info || !type_info->is_component) {
        core::log(core::LogLevel::Error, "ComponentFactory: '{}' is not a registered component", type_name);
        return {};
    }

    // Create default instance
    auto instance = type.construct();
    if (!instance) {
        core::log(core::LogLevel::Error, "ComponentFactory: Failed to construct '{}'", type_name);
        return {};
    }

    // Get or create storage for this component type
    auto* storage = registry.storage(type.id());
    if (!storage) {
        core::log(core::LogLevel::Error, "ComponentFactory: Failed to get storage for '{}'", type_name);
        return {};
    }

    // Add to entity via storage
    if (!storage->contains(entity)) {
        storage->push(entity);
    }

    return instance;
}

entt::meta_any ComponentFactory::create_default(const std::string& type_name) {
    auto& type_registry = TypeRegistry::instance();

    auto type = type_registry.find_type(type_name);
    if (!type) {
        return {};
    }

    return type.construct();
}

bool ComponentFactory::remove(entt::registry& registry, entt::entity entity, const std::string& type_name) {
    auto& type_registry = TypeRegistry::instance();

    auto type = type_registry.find_type(type_name);
    if (!type) {
        return false;
    }

    auto* storage = registry.storage(type.id());
    if (storage && storage->contains(entity)) {
        storage->remove(entity);
        return true;
    }

    return false;
}

bool ComponentFactory::has(entt::registry& registry, entt::entity entity, const std::string& type_name) {
    auto& type_registry = TypeRegistry::instance();

    auto type = type_registry.find_type(type_name);
    if (!type) {
        return false;
    }

    auto* storage = registry.storage(type.id());
    return storage && storage->contains(entity);
}

entt::meta_any ComponentFactory::get(entt::registry& registry, entt::entity entity, const std::string& type_name) {
    return TypeRegistry::instance().get_component_any(registry, entity, type_name);
}

entt::meta_any ComponentFactory::get(const entt::registry& registry, entt::entity entity, const std::string& type_name) const {
    auto& type_registry = TypeRegistry::instance();

    auto type = type_registry.find_type(type_name);
    if (!type) {
        return {};
    }

    auto* storage = registry.storage(type.id());
    if (storage && storage->contains(entity)) {
        const void* ptr = storage->value(entity);
        return type.from_void(const_cast<void*>(ptr));
    }

    return {};
}

bool ComponentFactory::set_property(entt::registry& registry, entt::entity entity,
                                    const std::string& type_name, const std::string& prop_name,
                                    const entt::meta_any& value) {
    auto component = get(registry, entity, type_name);
    if (!component) {
        return false;
    }

    auto* prop_info = TypeRegistry::instance().get_property_info(type_name, prop_name);
    if (!prop_info || !prop_info->setter) {
        return false;
    }

    prop_info->setter(component, value);
    return true;
}

entt::meta_any ComponentFactory::get_property(entt::registry& registry, entt::entity entity,
                                              const std::string& type_name, const std::string& prop_name) {
    auto component = get(registry, entity, type_name);
    if (!component) {
        return {};
    }

    auto* prop_info = TypeRegistry::instance().get_property_info(type_name, prop_name);
    if (!prop_info || !prop_info->getter) {
        return {};
    }

    return prop_info->getter(component);
}

bool ComponentFactory::is_component(const std::string& type_name) const {
    auto* type_info = TypeRegistry::instance().get_type_info(type_name);
    return type_info && type_info->is_component;
}

std::vector<std::string> ComponentFactory::get_component_names() const {
    return TypeRegistry::instance().get_all_component_names();
}

// Helper functions

bool clone_component(entt::registry& registry,
                     entt::entity src, entt::entity dst,
                     const std::string& type_name) {
    auto& factory = ComponentFactory::instance();

    if (!factory.has(registry, src, type_name)) {
        return false;
    }

    auto src_component = factory.get(registry, src, type_name);
    if (!src_component) {
        return false;
    }

    // Create component on destination
    auto dst_component = factory.create(registry, dst, type_name);
    if (!dst_component) {
        return false;
    }

    // Copy properties
    auto* type_info = TypeRegistry::instance().get_type_info(type_name);
    if (type_info) {
        for (const auto& prop : type_info->properties) {
            if (prop.getter && prop.setter) {
                auto value = prop.getter(src_component);
                if (value) {
                    prop.setter(dst_component, value);
                }
            }
        }
    }

    return true;
}

void clone_all_components(entt::registry& registry,
                          entt::entity src, entt::entity dst) {
    auto component_names = ComponentFactory::instance().get_component_names();

    for (const auto& name : component_names) {
        if (ComponentFactory::instance().has(registry, src, name)) {
            clone_component(registry, src, dst, name);
        }
    }
}

bool apply_component_data(entt::registry& registry, entt::entity entity,
                          const std::string& type_name, const entt::meta_any& data) {
    auto& factory = ComponentFactory::instance();

    if (!factory.has(registry, entity, type_name)) {
        // Create the component first
        factory.create(registry, entity, type_name);
    }

    auto component = factory.get(registry, entity, type_name);
    if (!component) {
        return false;
    }

    // Copy properties from data
    auto* type_info = TypeRegistry::instance().get_type_info(type_name);
    if (type_info) {
        for (const auto& prop : type_info->properties) {
            if (prop.getter && prop.setter) {
                auto value = prop.getter(data);
                if (value) {
                    prop.setter(component, value);
                }
            }
        }
    }

    return true;
}

} // namespace engine::reflect
