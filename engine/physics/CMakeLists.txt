# engine/physics/CMakeLists.txt
# engine::physics - Jolt Physics wrapper

add_library(engine_physics STATIC)
add_library(engine::physics ALIAS engine_physics)

target_sources(engine_physics
    PRIVATE
        src/physics_world.cpp
        src/jolt_impl.cpp
        src/debug_renderer.cpp
        src/character_controller.cpp
        src/ragdoll.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/physics/physics.hpp
            include/engine/physics/physics_world.hpp
            include/engine/physics/body.hpp
            include/engine/physics/shapes.hpp
            include/engine/physics/layers.hpp
            include/engine/physics/debug_renderer.hpp
            include/engine/physics/character_controller.hpp
            include/engine/physics/ragdoll.hpp
)

target_include_directories(engine_physics
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_physics
    PUBLIC
        engine::core
        engine::core
        engine::render
        engine::scene
        Jolt
        Jolt
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_physics PROPERTIES
    FOLDER "engine"
)
