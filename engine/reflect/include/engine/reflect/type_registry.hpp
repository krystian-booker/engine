#pragma once

#include <engine/reflect/property.hpp>
#include <engine/reflect/entity_resolution.hpp>
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

// Method info returned from queries
struct MethodInfo {
    std::string name;
    entt::meta_type return_type;
    std::vector<entt::meta_type> param_types;
    std::function<entt::meta_any(entt::meta_any&, const std::vector<entt::meta_any>&)> invoker;
};

// Type info returned from queries
struct TypeInfo {
    std::string name;
    entt::id_type id;
    TypeMeta meta;
    std::vector<PropertyInfo> properties;
    std::vector<MethodInfo> methods;
    bool is_component;
    bool is_enum;
    std::vector<std::pair<std::string, int64_t>> enum_values;
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

    // Register an enum type
    template<typename T>
    void register_enum(const char* name, std::initializer_list<std::pair<T, const char*>> values);

    // Register a property with custom getter/setter
    template<typename T, auto MemberPtr, typename Getter, typename Setter>
    void register_property(const char* name, const PropertyMeta& meta, Getter&& getter, Setter&& setter);

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

    // Get method info
    const MethodInfo* get_method_info(const std::string& type_name, const std::string& method_name) const;

    // Invoke a method by name
    entt::meta_any invoke_method(entt::meta_any& obj, const std::string& type_name,
                                 const std::string& method_name,
                                 const std::vector<entt::meta_any>& args = {});

    // Serialization helpers
    void serialize_any(const entt::meta_any& value, core::IArchive& ar, const char* name);
    entt::meta_any deserialize_any(entt::meta_type type, core::IArchive& ar, const char* name);

    // Serialization with entity resolution context (for entity reference properties)
    void serialize_any(const entt::meta_any& value, core::IArchive& ar, const char* name,
                       const EntityResolutionContext* entity_ctx);
    entt::meta_any deserialize_any(entt::meta_type type, core::IArchive& ar, const char* name,
                                   const EntityResolutionContext* entity_ctx);

    // Create a meta_any from a component on an entity
    entt::meta_any get_component_any(entt::registry& registry, entt::entity entity, const std::string& type_name);

    // Set a component from a meta_any
    void set_component_any(entt::registry& registry, entt::entity entity, const std::string& type_name, const entt::meta_any& value);

    // Add a default-constructed component to an entity
    bool add_component_any(entt::registry& registry, entt::entity entity, const std::string& type_name);

    // Remove a component from an entity
    bool remove_component_any(entt::registry& registry, entt::entity entity, const std::string& type_name);

    // Query if type is a registered vector type
    struct VectorTypeInfo;
    const VectorTypeInfo* get_vector_type_info(entt::id_type type_id) const;

    // Register a vector type for serialization
    template<typename T>
    void register_vector_type();

private:
    // Component factory function types
    using ComponentEmplacer = std::function<void(entt::registry&, entt::entity)>;
    using ComponentRemover = std::function<void(entt::registry&, entt::entity)>;

    struct ComponentFactory {
        ComponentEmplacer emplace;
        ComponentRemover remove;
    };

    // Vector type info for serialization
    struct VectorTypeInfo {
        entt::id_type vector_type_id;
        entt::id_type element_type_id;
        entt::meta_type element_type;
        std::function<size_t(const entt::meta_any&)> get_size;
        std::function<entt::meta_any(const entt::meta_any&, size_t)> get_element;
        std::function<entt::meta_any(size_t)> create_vector;
        std::function<void(entt::meta_any&, size_t, const entt::meta_any&)> set_element;
    };

    TypeRegistry() = default;
    ~TypeRegistry() = default;
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    // Internal storage
    std::unordered_map<std::string, entt::id_type> m_name_to_id;
    std::unordered_map<entt::id_type, TypeInfo> m_type_info;
    std::unordered_map<std::string, ComponentFactory> m_component_factories;
    std::vector<std::string> m_component_names;
    std::unordered_map<entt::id_type, VectorTypeInfo> m_vector_types;
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

    // Store factory functions for runtime add/remove
    ComponentFactory factory;
    factory.emplace = [](entt::registry& reg, entt::entity ent) {
        reg.emplace_or_replace<T>(ent);
    };
    factory.remove = [](entt::registry& reg, entt::entity ent) {
        reg.remove<T>(ent);
    };
    m_component_factories[name] = std::move(factory);
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
                // Handle int -> enum conversion for deserialization
                else if constexpr (std::is_enum_v<MemberType>) {
                    if (auto* int_val = value.try_cast<int>()) {
                        ptr->*MemberPtr = static_cast<MemberType>(*int_val);
                    }
                }
            }
        };

        it->second.properties.push_back(std::move(prop));
    }
}

namespace detail {
    // Helper to extract member function traits
    template<typename T>
    struct member_function_traits;

    // Non-const member function
    template<typename R, typename C, typename... Args>
    struct member_function_traits<R(C::*)(Args...)> {
        using return_type = R;
        using class_type = C;
        using args_tuple = std::tuple<Args...>;
        static constexpr size_t arity = sizeof...(Args);
    };

    // Const member function
    template<typename R, typename C, typename... Args>
    struct member_function_traits<R(C::*)(Args...) const> {
        using return_type = R;
        using class_type = C;
        using args_tuple = std::tuple<Args...>;
        static constexpr size_t arity = sizeof...(Args);
    };

    // Helper to invoke method with args from vector
    template<typename T, auto FuncPtr, typename R, size_t... Is, typename... Args>
    entt::meta_any invoke_impl(T* obj, const std::vector<entt::meta_any>& args,
                               std::index_sequence<Is...>, std::tuple<Args...>) {
        if constexpr (std::is_void_v<R>) {
            (obj->*FuncPtr)(args[Is].template cast<std::decay_t<Args>>()...);
            return {};
        } else {
            return entt::meta_any{(obj->*FuncPtr)(args[Is].template cast<std::decay_t<Args>>()...)};
        }
    }
}

template<typename T, auto FuncPtr>
void TypeRegistry::register_method(const char* name) {
    using traits = detail::member_function_traits<decltype(FuncPtr)>;
    using return_type = typename traits::return_type;
    using args_tuple = typename traits::args_tuple;
    constexpr size_t arity = traits::arity;

    auto type_id = entt::type_hash<T>::value();

    auto it = m_type_info.find(type_id);
    if (it != m_type_info.end()) {
        MethodInfo method;
        method.name = name;
        method.return_type = entt::resolve<return_type>();

        // Store parameter types
        if constexpr (arity > 0) {
            method.param_types.reserve(arity);
            std::apply([&](auto... args) {
                (method.param_types.push_back(entt::resolve<std::decay_t<decltype(args)>>()), ...);
            }, args_tuple{});
        }

        // Create invoker lambda
        if constexpr (arity == 0) {
            method.invoker = [](entt::meta_any& obj, const std::vector<entt::meta_any>& args) -> entt::meta_any {
                if (!args.empty()) {
                    return {};
                }
                if (auto* ptr = obj.try_cast<T>()) {
                    if constexpr (std::is_void_v<return_type>) {
                        (ptr->*FuncPtr)();
                        return {};
                    } else {
                        return entt::meta_any{(ptr->*FuncPtr)()};
                    }
                }
                return {};
            };
        } else {
            method.invoker = [](entt::meta_any& obj, const std::vector<entt::meta_any>& args) -> entt::meta_any {
                if (args.size() != arity) {
                    return {};
                }
                if (auto* ptr = obj.try_cast<T>()) {
                    return detail::invoke_impl<T, FuncPtr, return_type>(
                        ptr, args, std::make_index_sequence<arity>{}, args_tuple{});
                }
                return {};
            };
        }

        it->second.methods.push_back(std::move(method));
    }
}

template<typename T>
void TypeRegistry::register_enum(const char* name, std::initializer_list<std::pair<T, const char*>> values) {
    auto type_id = entt::type_hash<T>::value();

    // Store our metadata
    m_name_to_id[name] = type_id;

    TypeInfo info;
    info.name = name;
    info.id = type_id;
    info.is_component = false;
    info.is_enum = true;

    // Store enum values for serialization and editor display
    info.enum_values.reserve(values.size());
    for (const auto& [value, value_name] : values) {
        info.enum_values.emplace_back(value_name, static_cast<int64_t>(value));
    }

    m_type_info[type_id] = std::move(info);
}

template<typename T, auto MemberPtr, typename Getter, typename Setter>
void TypeRegistry::register_property(const char* name, const PropertyMeta& meta, Getter&& getter, Setter&& setter) {
    using PropertyType = std::invoke_result_t<Getter, const T&>;
    // MemberPtr is unused for type deduction here, we use the getter return type
    
    auto type_id = entt::type_hash<T>::value();

    auto it = m_type_info.find(type_id);
    if (it != m_type_info.end()) {
        PropertyInfo prop;
        prop.name = name;
        prop.type = entt::resolve<PropertyType>();
        prop.meta = meta;
        prop.meta.name = name;

        // Create getter
        prop.getter = [g = std::forward<Getter>(getter)](const entt::meta_any& obj) -> entt::meta_any {
            if (auto* ptr = obj.try_cast<T>()) {
                return entt::meta_any{g(*ptr)};
            }
            return {};
        };

        // Create setter
        prop.setter = [s = std::forward<Setter>(setter)](entt::meta_any& obj, const entt::meta_any& value) {
            if (auto* ptr = obj.try_cast<T>()) {
                // Try direct cast first
                if (auto* val = value.try_cast<PropertyType>()) {
                    s(*ptr, *val);
                }
                // Handle int -> enum conversion for deserialization
                else if constexpr (std::is_enum_v<PropertyType>) {
                    if (auto* int_val = value.try_cast<int>()) {
                        s(*ptr, static_cast<PropertyType>(*int_val));
                    }
                }
            }
        };

        it->second.properties.push_back(std::move(prop));
    }
}

template<typename T>
void TypeRegistry::register_vector_type() {
    using VectorType = std::vector<T>;

    auto vector_type_id = entt::type_hash<VectorType>::value();

    VectorTypeInfo info;
    info.vector_type_id = vector_type_id;
    info.element_type_id = entt::type_hash<T>::value();
    info.element_type = entt::resolve<T>();

    info.get_size = [](const entt::meta_any& vec) -> size_t {
        if (auto* ptr = vec.try_cast<VectorType>()) {
            return ptr->size();
        }
        return 0;
    };

    info.get_element = [](const entt::meta_any& vec, size_t index) -> entt::meta_any {
        if (auto* ptr = vec.try_cast<VectorType>()) {
            if (index < ptr->size()) {
                return entt::meta_any{(*ptr)[index]};
            }
        }
        return {};
    };

    info.create_vector = [](size_t size) -> entt::meta_any {
        VectorType vec(size);
        return entt::meta_any{std::move(vec)};
    };

    info.set_element = [](entt::meta_any& vec, size_t index, const entt::meta_any& value) {
        if (auto* ptr = vec.try_cast<VectorType>()) {
            if (index < ptr->size()) {
                if (auto* val = value.try_cast<T>()) {
                    (*ptr)[index] = *val;
                }
            }
        }
    };

    m_vector_types[vector_type_id] = std::move(info);
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

#define ENGINE_REFLECT_METHOD(Type, Method) \
    namespace { \
        struct Type##_##Method##_Registrar { \
            Type##_##Method##_Registrar() { \
                ::engine::reflect::TypeRegistry::instance().register_method<Type, &Type::Method>(#Method); \
            } \
        }; \
        static Type##_##Method##_Registrar _reflect_##Type##_##Method; \
    }

} // namespace engine::reflect
