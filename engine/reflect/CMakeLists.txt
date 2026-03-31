# engine/reflect/CMakeLists.txt
# engine::reflect - Runtime type reflection using EnTT meta

engine_add_module(
    NAME reflect
    SOURCES
        src/type_registry.cpp
        src/component_factory.cpp
    HEADERS
        include/engine/reflect/reflect.hpp
        include/engine/reflect/property.hpp
        include/engine/reflect/type_registry.hpp
        include/engine/reflect/component_factory.hpp
        include/engine/reflect/entity_resolution.hpp
    PUBLIC_DEPS
        engine::core
        entt::entt
)
