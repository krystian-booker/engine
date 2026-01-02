#pragma once

#include <engine/reflect/property.hpp>
#include <engine/core/serialize.hpp>
#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <cstring>

#ifdef meta
#undef meta
#endif

namespace engine::reflect {

using namespace entt::literals;

// Forward declaration
class TypeRegistry;

// Property info returned from queries
struct PropertyInfo {
    std::string name;
    entt::meta_type type;
    PropertyMeta meta;
    std::function<entt::meta_any(const entt::meta_any&)> getter;
    std::function<void(entt::meta_any&, const entt::meta_any&)> setter;
};

// Type info returned from queries
struct TypeInfo {
    std::string name;
    entt::id_type id;
    TypeMeta meta;
    std::vector<PropertyInfo> properties;
    bool is_component;
};

// Central type registry using EnTT meta
class TypeRegistry {
public:
    static TypeRegistry& instance();

    // Register a type with a name
    template<typename T>
    void register_type(const char* name, const TypeMeta& meta = {});

    // Register a component type (adds factory and enables runtime creation)
    template<typename T>
    void register_component(const char* name, const TypeMeta& meta = {});

    // Register a property on a type
    template<typename T, auto MemberPtr>
    void register_property(const char* name, const PropertyMeta& meta = {});

    // Register a method on a type
    template<typename T, auto FuncPtr>
    void register_method(const char* name);

    // Query types
    bool has_type(const std::string& name) const;
    bool has_type(entt::id_type id) const;

    entt::meta_type find_type(const std::string& name) const;
    entt::meta_type find_type(entt::id_type id) const;

    std::vector<std::string> get_all_type_names() const;
    std::vector<std::string> get_all_component_names() const;

    // Get type info
    const TypeInfo* get_type_info(const std::string& name) const;
    const TypeInfo* get_type_info(entt::id_type id) const;

    // Get property info
    const PropertyInfo* get_property_info(const std::string& type_name, const std::string& prop_name) const;

    // Serialization helpers
    void serialize_any(const entt::meta_any& value, core::IArchive& ar, const char* name);
    entt::meta_any deserialize_any(entt::meta_type type, core::IArchive& ar, const char* name);

    // Create a meta_any from a component on an entity
    entt::meta_any get_component_any(entt::registry& registry, entt::entity entity, const std::string& type_name);

    // Set a component from a meta_any
    void set_component_any(entt::registry& registry, entt::entity entity, const std::string& type_name, const entt::meta_any& value);

private:
    TypeRegistry() = default;
    ~TypeRegistry() = default;
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    // Internal storage
    std::unordered_map<std::string, entt::id_type> m_name_to_id;
    std::unordered_map<entt::id_type, TypeInfo> m_type_info;
    std::vector<std::string> m_component_names;
};

// Implementation of template methods

template<typename T>
void TypeRegistry::register_type(const char* name, const TypeMeta& meta) {
    auto type_id = entt::type_hash<T>::value();

    // Store our metadata
    m_name_to_id[name] = type_id;

    TypeInfo info;
    info.name = name;
    info.id = type_id;
    info.meta = meta;
    info.meta.name = name;
    info.is_component = false;

    m_type_info[type_id] = std::move(info);
}

template<typename T>
void TypeRegistry::register_component(const char* name, const TypeMeta& meta) {
    auto type_id = entt::type_hash<T>::value();

    // Store our metadata
    m_name_to_id[name] = type_id;

    TypeInfo info;
    info.name = name;
    info.id = type_id;
    info.meta = meta;
    info.meta.name = name;
    info.meta.is_component = true;
    info.is_component = true;

    m_type_info[type_id] = std::move(info);
    m_component_names.push_back(name);
}

template<typename T, auto MemberPtr>
void TypeRegistry::register_property(const char* name, const PropertyMeta& meta) {
    using MemberType = std::remove_reference_t<decltype(std::declval<T>().*MemberPtr)>;

    auto type_id = entt::type_hash<T>::value();
    (void)name;

    // Find our TypeInfo and add property
    auto it = m_type_info.find(type_id);
    if (it != m_type_info.end()) {
        PropertyInfo prop;
        prop.name = name;
        prop.type = entt::resolve<MemberType>();
        prop.meta = meta;
        prop.meta.name = name;

        // Create getter
        prop.getter = [](const entt::meta_any& obj) -> entt::meta_any {
            if (auto* ptr = obj.try_cast<T>()) {
                return entt::meta_any{ptr->*MemberPtr};
            }
            return {};
        };

        // Create setter
        prop.setter = [](entt::meta_any& obj, const entt::meta_any& value) {
            if (auto* ptr = obj.try_cast<T>()) {
                if (auto* val = value.try_cast<MemberType>()) {
                    ptr->*MemberPtr = *val;
                }
            }
        };

        it->second.properties.push_back(std::move(prop));
    }
}

template<typename T, auto FuncPtr>
void TypeRegistry::register_method(const char* name) {
    (void)name;
}

// Registration macros for convenience
#define ENGINE_REFLECT_TYPE(Type, ...) \
    namespace { \
        struct Type##_Registrar { \
            Type##_Registrar() { \
                ::engine::reflect::TypeRegistry::instance().register_type<Type>(#Type, TypeMeta{__VA_ARGS__}); \
            } \
        }; \
        static Type##_Registrar _reflect_##Type; \
    }

#define ENGINE_REFLECT_COMPONENT(Type, ...) \
    namespace { \
        struct Type##_Registrar { \
            Type##_Registrar() { \
                ::engine::reflect::TypeRegistry::instance().register_component<Type>(#Type, TypeMeta{__VA_ARGS__}); \
            } \
        }; \
        static Type##_Registrar _reflect_##Type; \
    }

#define ENGINE_REFLECT_PROPERTY(Type, Member, ...) \
    namespace { \
        struct Type##_##Member##_Registrar { \
            Type##_##Member##_Registrar() { \
                ::engine::reflect::TypeRegistry::instance().register_property<Type, &Type::Member>(#Member, PropertyMeta{__VA_ARGS__}); \
            } \
        }; \
        static Type##_##Member##_Registrar _reflect_##Type##_##Member; \
    }

} // namespace engine::reflect
