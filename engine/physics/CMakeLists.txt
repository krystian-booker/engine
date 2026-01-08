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
        src/physics_system.cpp
        src/rigid_body_component.cpp
        # Water/Buoyancy/Boat physics
        src/water_volume.cpp
        src/buoyancy.cpp
        src/boat.cpp
        src/water_component_registration.cpp
        # Vehicle physics
        src/vehicle.cpp
        src/vehicle_component_registration.cpp
        # Cloth physics
        src/cloth.cpp
        src/cloth_component_registration.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/physics/physics.hpp
            include/engine/physics/physics_world.hpp
            include/engine/physics/physics_system.hpp
            include/engine/physics/body.hpp
            include/engine/physics/shapes.hpp
            include/engine/physics/layers.hpp
            include/engine/physics/debug_renderer.hpp
            include/engine/physics/character_controller.hpp
            include/engine/physics/ragdoll.hpp
            include/engine/physics/rigid_body_component.hpp
            # Water/Buoyancy/Boat physics
            include/engine/physics/water_volume.hpp
            include/engine/physics/buoyancy_component.hpp
            include/engine/physics/boat.hpp
            # Vehicle physics
            include/engine/physics/vehicle.hpp
            include/engine/physics/vehicle_component.hpp
            # Cloth physics
            include/engine/physics/cloth.hpp
            include/engine/physics/cloth_component.hpp
)

target_include_directories(engine_physics
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_physics
    PUBLIC
        engine::core
        engine::render
        engine::scene
        Jolt
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_physics PROPERTIES
    FOLDER "engine"
)

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()
