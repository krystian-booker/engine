#include <engine/reflect/type_registry.hpp>
#include <engine/core/log.hpp>

namespace engine::reflect {

TypeRegistry& TypeRegistry::instance() {
    static TypeRegistry instance;
    return instance;
}

bool TypeRegistry::has_type(const std::string& name) const {
    return m_name_to_id.find(name) != m_name_to_id.end();
}

bool TypeRegistry::has_type(entt::id_type id) const {
    return m_type_info.find(id) != m_type_info.end();
}

entt::meta_type TypeRegistry::find_type(const std::string& name) const {
    auto it = m_name_to_id.find(name);
    if (it != m_name_to_id.end()) {
        return entt::resolve(it->second);
    }
    return {};
}

entt::meta_type TypeRegistry::find_type(entt::id_type id) const {
    return entt::resolve(id);
}

std::vector<std::string> TypeRegistry::get_all_type_names() const {
    std::vector<std::string> names;
    names.reserve(m_name_to_id.size());
    for (const auto& [name, id] : m_name_to_id) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> TypeRegistry::get_all_component_names() const {
    return m_component_names;
}

const TypeInfo* TypeRegistry::get_type_info(const std::string& name) const {
    auto it = m_name_to_id.find(name);
    if (it != m_name_to_id.end()) {
        return get_type_info(it->second);
    }
    return nullptr;
}

const TypeInfo* TypeRegistry::get_type_info(entt::id_type id) const {
    auto it = m_type_info.find(id);
    if (it != m_type_info.end()) {
        return &it->second;
    }
    return nullptr;
}

const PropertyInfo* TypeRegistry::get_property_info(const std::string& type_name, const std::string& prop_name) const {
    auto* type_info = get_type_info(type_name);
    if (!type_info) {
        return nullptr;
    }

    for (const auto& prop : type_info->properties) {
        if (prop.name == prop_name) {
            return &prop;
        }
    }
    return nullptr;
}

const MethodInfo* TypeRegistry::get_method_info(const std::string& type_name, const std::string& method_name) const {
    auto* type_info = get_type_info(type_name);
    if (!type_info) {
        return nullptr;
    }

    for (const auto& method : type_info->methods) {
        if (method.name == method_name) {
            return &method;
        }
    }
    return nullptr;
}

entt::meta_any TypeRegistry::invoke_method(entt::meta_any& obj, const std::string& type_name,
                                           const std::string& method_name,
                                           const std::vector<entt::meta_any>& args) {
    auto* method_info = get_method_info(type_name, method_name);
    if (!method_info || !method_info->invoker) {
        return {};
    }

    return method_info->invoker(obj, args);
}

void TypeRegistry::serialize_any(const entt::meta_any& value, core::IArchive& ar, const char* name) {
    if (!value) {
        return;
    }

    auto type = value.type();
    auto type_id = type.id();

    // Handle primitive types
    if (type_id == entt::type_hash<bool>::value()) {
        bool v = value.cast<bool>();
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<int32_t>::value()) {
        int32_t v = value.cast<int32_t>();
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<uint32_t>::value()) {
        uint32_t v = value.cast<uint32_t>();
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<int64_t>::value()) {
        int64_t v = value.cast<int64_t>();
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<uint64_t>::value()) {
        uint64_t v = value.cast<uint64_t>();
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<float>::value()) {
        float v = value.cast<float>();
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<double>::value()) {
        double v = value.cast<double>();
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<std::string>::value()) {
        std::string v = value.cast<std::string>();
        ar.serialize(name, v);
    }
    // Handle math types
    else if (type_id == entt::type_hash<core::Vec2>::value()) {
        core::Vec2 v = value.cast<core::Vec2>();
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<core::Vec3>::value()) {
        core::Vec3 v = value.cast<core::Vec3>();
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<core::Vec4>::value()) {
        core::Vec4 v = value.cast<core::Vec4>();
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<core::Quat>::value()) {
        core::Quat v = value.cast<core::Quat>();
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<core::Mat4>::value()) {
        core::Mat4 v = value.cast<core::Mat4>();
        ar.serialize(name, v);
    }
    // Handle enum types - serialize as string name
    else {
        auto* type_info = get_type_info(type_id);
        if (type_info && type_info->is_enum) {
            // Get the underlying integer value from the enum
            // Enums are typically stored as their underlying type (usually int)
            int64_t int_val = 0;
            if (value.allow_cast<int>()) {
                int_val = static_cast<int64_t>(value.cast<int>());
            } else if (value.allow_cast<int64_t>()) {
                int_val = value.cast<int64_t>();
            } else if (value.allow_cast<uint32_t>()) {
                int_val = static_cast<int64_t>(value.cast<uint32_t>());
            } else if (value.allow_cast<int32_t>()) {
                int_val = static_cast<int64_t>(value.cast<int32_t>());
            }

            // Find the string name for this value
            std::string enum_str;
            for (const auto& [enum_name, enum_val] : type_info->enum_values) {
                if (enum_val == int_val) {
                    enum_str = enum_name;
                    break;
                }
            }

            // Serialize as string (fallback to integer string if not found)
            if (enum_str.empty()) {
                enum_str = std::to_string(int_val);
            }
            ar.serialize(name, enum_str);
        }
        // Handle complex types via their registered properties
        else if (type_info && ar.begin_object(name)) {
            for (const auto& prop : type_info->properties) {
                if (prop.getter) {
                    auto prop_value = prop.getter(value);
                    serialize_any(prop_value, ar, prop.name.c_str());
                }
            }
            ar.end_object();
        }
    }
}

entt::meta_any TypeRegistry::deserialize_any(entt::meta_type type, core::IArchive& ar, const char* name) {
    if (!type) {
        return {};
    }

    auto type_id = type.id();

    // Handle primitive types
    if (type_id == entt::type_hash<bool>::value()) {
        bool v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    else if (type_id == entt::type_hash<int32_t>::value()) {
        int32_t v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    else if (type_id == entt::type_hash<uint32_t>::value()) {
        uint32_t v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    else if (type_id == entt::type_hash<int64_t>::value()) {
        int64_t v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    else if (type_id == entt::type_hash<uint64_t>::value()) {
        uint64_t v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    else if (type_id == entt::type_hash<float>::value()) {
        float v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    else if (type_id == entt::type_hash<double>::value()) {
        double v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    else if (type_id == entt::type_hash<std::string>::value()) {
        std::string v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    // Handle math types
    else if (type_id == entt::type_hash<core::Vec2>::value()) {
        core::Vec2 v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    else if (type_id == entt::type_hash<core::Vec3>::value()) {
        core::Vec3 v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    else if (type_id == entt::type_hash<core::Vec4>::value()) {
        core::Vec4 v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    else if (type_id == entt::type_hash<core::Quat>::value()) {
        core::Quat v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    else if (type_id == entt::type_hash<core::Mat4>::value()) {
        core::Mat4 v{};
        ar.serialize(name, v);
        return entt::meta_any{v};
    }
    // Handle enum types - deserialize from string name
    else {
        auto* type_info = get_type_info(type_id);
        if (type_info && type_info->is_enum) {
            // Read the string name
            std::string enum_str;
            ar.serialize(name, enum_str);

            // Look up the integer value
            int64_t int_val = 0;
            bool found = false;
            for (const auto& [enum_name, enum_val] : type_info->enum_values) {
                if (enum_name == enum_str) {
                    int_val = enum_val;
                    found = true;
                    break;
                }
            }

            // If not found by name, try parsing as integer (fallback)
            if (!found) {
                try {
                    int_val = std::stoll(enum_str);
                } catch (...) {
                    int_val = 0;
                }
            }

            // For enums, we need to return a meta_any with the correct integer value
            // The property setter will handle the type conversion
            // Store as int which is the most common underlying type for enums
            int int_value = static_cast<int>(int_val);
            return entt::meta_any{int_value};
        }
        // Handle complex types
        else if (type_info) {
            // Create default instance
            auto instance = type.construct();
            if (instance && ar.begin_object(name)) {
                for (const auto& prop : type_info->properties) {
                    if (prop.setter) {
                        auto prop_value = deserialize_any(prop.type, ar, prop.name.c_str());
                        if (prop_value) {
                            prop.setter(instance, prop_value);
                        }
                    }
                }
                ar.end_object();
            }
            return instance;
        }
    }

    return {};
}

entt::meta_any TypeRegistry::get_component_any(entt::registry& registry, entt::entity entity, const std::string& type_name) {
    auto it = m_name_to_id.find(type_name);
    if (it == m_name_to_id.end()) {
        return {};
    }

    auto type = entt::resolve(it->second);
    if (!type) {
        return {};
    }

    // Use EnTT's storage to get the component
    auto* storage = registry.storage(it->second);
    if (storage && storage->contains(entity)) {
        // Get void pointer to component and wrap in meta_any
        void* ptr = storage->value(entity);
        return type.from_void(ptr);
    }

    return {};
}

void TypeRegistry::set_component_any(entt::registry& registry, entt::entity entity, const std::string& type_name, const entt::meta_any& value) {
    auto it = m_name_to_id.find(type_name);
    if (it == m_name_to_id.end()) {
        return;
    }

    auto* storage = registry.storage(it->second);
    if (storage && storage->contains(entity)) {
        void* ptr = storage->value(entity);
        auto type = entt::resolve(it->second);
        if (type && value) {
            // Copy property-by-property from value to target
            auto* type_info = get_type_info(it->second);
            if (type_info) {
                entt::meta_any target = type.from_void(ptr);
                for (const auto& prop : type_info->properties) {
                    if (prop.getter && prop.setter) {
                        auto prop_value = prop.getter(value);
                        if (prop_value) {
                            prop.setter(target, prop_value);
                        }
                    }
                }
            }
        }
    }
}

bool TypeRegistry::add_component_any(entt::registry& registry, entt::entity entity, const std::string& type_name) {
    auto it = m_component_factories.find(type_name);
    if (it == m_component_factories.end()) {
        return false;
    }

    it->second.emplace(registry, entity);
    return true;
}

bool TypeRegistry::remove_component_any(entt::registry& registry, entt::entity entity, const std::string& type_name) {
    auto it = m_component_factories.find(type_name);
    if (it == m_component_factories.end()) {
        return false;
    }

    // Check if entity has the component before removing
    auto name_it = m_name_to_id.find(type_name);
    if (name_it == m_name_to_id.end()) {
        return false;
    }

    auto* storage = registry.storage(name_it->second);
    if (!storage || !storage->contains(entity)) {
        return false;
    }

    it->second.remove(registry, entity);
    return true;
}

} // namespace engine::reflect
