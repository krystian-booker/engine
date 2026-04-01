#include <engine/reflect/type_registry.hpp>
#include <engine/core/log.hpp>
#include <engine/core/math.hpp>

namespace engine::reflect {

namespace {

engine::core::Mat4 compose_runtime_local_matrix(const engine::core::Vec3& position,
                                                const engine::core::Quat& rotation,
                                                const engine::core::Vec3& scale) {
    engine::core::Mat4 result{1.0f};
    result = glm::translate(result, position);
    result = result * glm::mat4_cast(rotation);
    result = glm::scale(result, scale);
    return result;
}

template<typename T>
bool try_get_property_value(TypeRegistry& type_registry, const std::string& type_name,
                            const std::string& prop_name, const entt::meta_any& object, T& out_value) {
    const auto* prop = type_registry.get_property_info(type_name, prop_name);
    if (!prop || !prop->getter) {
        return false;
    }

    entt::meta_any value = prop->getter(object);
    if (auto* cast_value = value.try_cast<T>()) {
        out_value = *cast_value;
        return true;
    }

    return false;
}

template<typename T>
void set_property_value(TypeRegistry& type_registry, const std::string& type_name,
                        const std::string& prop_name, entt::meta_any& object, T value) {
    const auto* prop = type_registry.get_property_info(type_name, prop_name);
    if (prop && prop->setter) {
        prop->setter(object, entt::meta_any{std::move(value)});
    }
}

void provision_runtime_local_transform_state(entt::registry& registry, entt::entity entity) {
    auto& type_registry = TypeRegistry::instance();
    entt::meta_any local_transform = type_registry.get_component_any(registry, entity, "LocalTransform");
    if (!local_transform) {
        return;
    }

    engine::core::Vec3 position{0.0f};
    engine::core::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    engine::core::Vec3 scale{1.0f};
    try_get_property_value(type_registry, "LocalTransform", "position", local_transform, position);
    try_get_property_value(type_registry, "LocalTransform", "rotation", local_transform, rotation);
    try_get_property_value(type_registry, "LocalTransform", "scale", local_transform, scale);

    const engine::core::Mat4 local_matrix = compose_runtime_local_matrix(position, rotation, scale);

    if (type_registry.add_component_any(registry, entity, "WorldTransform")) {
        entt::meta_any world_transform = type_registry.get_component_any(registry, entity, "WorldTransform");
        if (world_transform) {
            set_property_value(type_registry, "WorldTransform", "matrix", world_transform, local_matrix);
        }
    }

    if (type_registry.add_component_any(registry, entity, "PreviousTransform")) {
        entt::meta_any previous_transform = type_registry.get_component_any(registry, entity, "PreviousTransform");
        if (previous_transform) {
            set_property_value(type_registry, "PreviousTransform", "position", previous_transform, position);
            set_property_value(type_registry, "PreviousTransform", "rotation", previous_transform, rotation);
            set_property_value(type_registry, "PreviousTransform", "scale", previous_transform, scale);
        }
    }

    if (type_registry.add_component_any(registry, entity, "InterpolatedTransform")) {
        entt::meta_any interpolated_transform = type_registry.get_component_any(registry, entity, "InterpolatedTransform");
        if (interpolated_transform) {
            set_property_value(type_registry, "InterpolatedTransform", "matrix", interpolated_transform, local_matrix);
        }
    }
}

void cleanup_runtime_local_transform_state(entt::registry& registry, entt::entity entity) {
    auto& type_registry = TypeRegistry::instance();
    type_registry.remove_component_any(registry, entity, "InterpolatedTransform");
    type_registry.remove_component_any(registry, entity, "PreviousTransform");
    type_registry.remove_component_any(registry, entity, "WorldTransform");
}

} // namespace

TypeRegistry& TypeRegistry::instance() {
    static TypeRegistry instance;
    static const bool initialized = [] {
        instance.register_type<entt::entity>("Entity");
        return true;
    }();
    (void)initialized;
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

const TypeRegistry::VectorTypeInfo* TypeRegistry::get_vector_type_info(entt::id_type type_id) const {
    auto it = m_vector_types.find(type_id);
    if (it != m_vector_types.end()) {
        return &it->second;
    }
    return nullptr;
}

void TypeRegistry::serialize_any(const entt::meta_any& value, core::IArchive& ar, const char* name) {
    serialize_any(value, ar, name, nullptr);
}

void TypeRegistry::serialize_any(const entt::meta_any& value, core::IArchive& ar, const char* name,
                                  const EntityResolutionContext* entity_ctx) {
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
    else if (type_id == entt::type_hash<int8_t>::value()) {
        int32_t v = static_cast<int32_t>(value.cast<int8_t>());
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<uint8_t>::value()) {
        uint32_t v = static_cast<uint32_t>(value.cast<uint8_t>());
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<int16_t>::value()) {
        int32_t v = static_cast<int32_t>(value.cast<int16_t>());
        ar.serialize(name, v);
    }
    else if (type_id == entt::type_hash<uint16_t>::value()) {
        uint32_t v = static_cast<uint32_t>(value.cast<uint16_t>());
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
    // Handle entity references - serialize as UUID
    else if (type_id == entt::type_hash<entt::entity>::value()) {
        auto entity = value.cast<entt::entity>();
        uint64_t uuid = EntityResolutionContext::NullUUID;
        if (entity != entt::null && entity_ctx && entity_ctx->can_serialize()) {
            uuid = entity_ctx->entity_to_uuid(entity);
        }
        ar.serialize(name, uuid);
    }
    // Handle vector types
    else if (auto* vec_info = get_vector_type_info(type_id)) {
        size_t size = vec_info->get_size(value);
        ar.begin_array(name, size);

        for (size_t i = 0; i < size; ++i) {
            auto element = vec_info->get_element(value, i);
            std::string elem_name = std::to_string(i);
            serialize_any(element, ar, elem_name.c_str(), entity_ctx);
        }

        ar.end_array();
    }
    // Handle enum types - serialize as string name
    else {
        auto* type_info = get_type_info(type_id);
        if (type_info && type_info->is_enum) {
            // Get the underlying integer value from the enum
            // Enums are typically stored as their underlying type (usually int)
            int64_t int_val = 0;
            if (auto converted_int = value.allow_cast<int>(); converted_int) {
                int_val = static_cast<int64_t>(converted_int.cast<int>());
            } else if (auto converted_i64 = value.allow_cast<int64_t>(); converted_i64) {
                int_val = converted_i64.cast<int64_t>();
            } else if (auto converted_u32 = value.allow_cast<uint32_t>(); converted_u32) {
                int_val = static_cast<int64_t>(converted_u32.cast<uint32_t>());
            } else if (auto converted_i32 = value.allow_cast<int32_t>(); converted_i32) {
                int_val = static_cast<int64_t>(converted_i32.cast<int32_t>());
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
                    // Use entity context for entity ref properties
                    if (prop.meta.is_entity_ref) {
                        serialize_any(prop_value, ar, prop.name.c_str(), entity_ctx);
                    } else {
                        serialize_any(prop_value, ar, prop.name.c_str(), nullptr);
                    }
                }
            }
            ar.end_object();
        }
    }
}

entt::meta_any TypeRegistry::deserialize_any(entt::meta_type type, core::IArchive& ar, const char* name) {
    return deserialize_any(type, ar, name, nullptr);
}

entt::meta_any TypeRegistry::deserialize_any(entt::meta_type type, core::IArchive& ar, const char* name,
                                              const EntityResolutionContext* entity_ctx) {
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
    else if (type_id == entt::type_hash<int8_t>::value()) {
        int32_t v{};
        ar.serialize(name, v);
        return entt::meta_any{static_cast<int8_t>(v)};
    }
    else if (type_id == entt::type_hash<uint8_t>::value()) {
        uint32_t v{};
        ar.serialize(name, v);
        return entt::meta_any{static_cast<uint8_t>(v)};
    }
    else if (type_id == entt::type_hash<int16_t>::value()) {
        int32_t v{};
        ar.serialize(name, v);
        return entt::meta_any{static_cast<int16_t>(v)};
    }
    else if (type_id == entt::type_hash<uint16_t>::value()) {
        uint32_t v{};
        ar.serialize(name, v);
        return entt::meta_any{static_cast<uint16_t>(v)};
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
    // Handle entity references - deserialize from UUID
    else if (type_id == entt::type_hash<entt::entity>::value()) {
        uint64_t uuid = 0;
        ar.serialize(name, uuid);

        entt::entity entity = entt::null;
        if (uuid != EntityResolutionContext::NullUUID && entity_ctx && entity_ctx->can_deserialize()) {
            entity = entity_ctx->uuid_to_entity(uuid);
            if (entity == entt::null) {
                std::string message = "Entity reference UUID " + std::to_string(uuid) +
                                      " not found during deserialization";
                core::log(core::LogLevel::Warn, message.c_str());
            }
        }
        return entt::meta_any{entity};
    }
    // Handle vector types
    else if (auto* vec_info = get_vector_type_info(type_id)) {
        size_t count = ar.begin_array(name, 0);

        if (count == 0) {
            ar.end_array();
            return vec_info->create_vector(0);
        }

        auto result = vec_info->create_vector(count);

        for (size_t i = 0; i < count; ++i) {
            std::string elem_name = std::to_string(i);
            auto element = deserialize_any(vec_info->element_type, ar, elem_name.c_str(), entity_ctx);
            if (element) {
                vec_info->set_element(result, i, element);
            }
        }

        ar.end_array();
        return result;
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
                        // Use entity context for entity ref properties
                        const EntityResolutionContext* prop_ctx = prop.meta.is_entity_ref ? entity_ctx : nullptr;
                        auto prop_value = deserialize_any(prop.type, ar, prop.name.c_str(), prop_ctx);
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
    auto factory_it = m_component_factories.find(type_name);
    if (factory_it != m_component_factories.end() && factory_it->second.get) {
        return factory_it->second.get(registry, entity);
    }

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
    if (!value) {
        return;
    }

    entt::meta_any target = get_component_any(registry, entity, type_name);
    if (!target) {
        return;
    }

    auto* type_info = get_type_info(type_name);
    if (!type_info) {
        return;
    }

    // Resolve source property values first, then reacquire target storage on each write.
    std::vector<std::pair<const PropertyInfo*, entt::meta_any>> pending_updates;
    pending_updates.reserve(type_info->properties.size());
    for (const auto& prop : type_info->properties) {
        if (!prop.getter || !prop.setter) {
            continue;
        }
        auto prop_value = prop.getter(value);
        if (prop_value) {
            pending_updates.emplace_back(&prop, std::move(prop_value));
        }
    }

    for (const auto& update : pending_updates) {
        update.first->setter(target, update.second);
    }
}

bool TypeRegistry::add_component_any(entt::registry& registry, entt::entity entity, const std::string& type_name) {
    auto it = m_component_factories.find(type_name);
    if (it == m_component_factories.end()) {
        return false;
    }

    it->second.emplace(registry, entity);
    if (type_name == "LocalTransform") {
        provision_runtime_local_transform_state(registry, entity);
    }
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

    if (type_name == "LocalTransform") {
        cleanup_runtime_local_transform_state(registry, entity);
    }
    it->second.remove(registry, entity);
    return true;
}

// Static registration of common vector types
namespace {
struct CoreMetaTypeRegistrar {
    CoreMetaTypeRegistrar() {
        auto& reg = TypeRegistry::instance();
        reg.register_type<entt::entity>("Entity");
    }
};
static CoreMetaTypeRegistrar _core_meta_type_registrar;

struct VectorTypeRegistrar {
    VectorTypeRegistrar() {
        auto& reg = TypeRegistry::instance();

        // Primitive types
        reg.register_vector_type<bool>();
        reg.register_vector_type<int32_t>();
        reg.register_vector_type<uint32_t>();
        reg.register_vector_type<int64_t>();
        reg.register_vector_type<uint64_t>();
        reg.register_vector_type<float>();
        reg.register_vector_type<double>();
        reg.register_vector_type<std::string>();

        // Math types
        reg.register_vector_type<core::Vec2>();
        reg.register_vector_type<core::Vec3>();
        reg.register_vector_type<core::Vec4>();
        reg.register_vector_type<core::Quat>();

        // Entity references (for vectors of entity refs)
        reg.register_vector_type<entt::entity>();
    }
};
static VectorTypeRegistrar _vector_registrar;
}

} // namespace engine::reflect
