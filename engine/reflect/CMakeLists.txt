# engine/reflect/CMakeLists.txt
# engine::reflect - Runtime type reflection using EnTT meta

add_library(engine_reflect STATIC)
add_library(engine::reflect ALIAS engine_reflect)

target_sources(engine_reflect
    PRIVATE
        src/type_registry.cpp
        src/component_factory.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/reflect/reflect.hpp
            include/engine/reflect/property.hpp
            include/engine/reflect/type_registry.hpp
            include/engine/reflect/component_factory.hpp
)

target_include_directories(engine_reflect
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_reflect
    PUBLIC
        engine::core
        entt::entt
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_reflect PROPERTIES
    FOLDER "engine"
)
