# engine/scene/CMakeLists.txt
# engine::scene - ECS, world, entities, components, systems, transforms

add_library(engine_scene STATIC)
add_library(engine::scene ALIAS engine_scene)

target_sources(engine_scene
    PRIVATE
        src/world.cpp
        src/systems.cpp
        src/transform.cpp
        src/hierarchy.cpp
        src/scene_serializer.cpp
        src/component_registration.cpp
        src/prefab_instance.cpp
        src/interaction.cpp
        src/entity_pool.cpp
        src/spawn_system.cpp
        src/targeting.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/scene/scene.hpp
            include/engine/scene/world.hpp
            include/engine/scene/entity.hpp
            include/engine/scene/components.hpp
            include/engine/scene/systems.hpp
            include/engine/scene/transform.hpp
            include/engine/scene/render_components.hpp
            include/engine/scene/scene_serializer.hpp
            include/engine/scene/prefab_instance.hpp
            include/engine/scene/interaction.hpp
            include/engine/scene/entity_pool.hpp
            include/engine/scene/spawn_system.hpp
            include/engine/scene/targeting.hpp
)

target_include_directories(engine_scene
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_scene
    PUBLIC
        engine::core
        engine::reflect
        entt::entt
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_scene PROPERTIES
    FOLDER "engine"
)
