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
    // Handle complex types via their registered properties
    else {
        auto* type_info = get_type_info(type_id);
        if (type_info && ar.begin_object(name)) {
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
    // Handle complex types
    else {
        auto* type_info = get_type_info(type_id);
        if (type_info) {
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
        // Copy data from meta_any to the component
        auto type = entt::resolve(it->second);
        if (type && value) {
            // Use type's copy assignment if available
            auto data = type.data("_copy"_hs);
            if (data) {
                // Not directly supported, use property-by-property copy
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
}

} // namespace engine::reflect
