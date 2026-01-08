# engine/navigation/CMakeLists.txt
# engine::navigation - Recast/Detour navigation and pathfinding

add_library(engine_navigation STATIC)
add_library(engine::navigation ALIAS engine_navigation)

target_sources(engine_navigation
    PRIVATE
        src/navmesh.cpp
        src/navmesh_builder.cpp
        src/pathfinder.cpp
        src/nav_agent.cpp
        src/nav_crowd.cpp
        src/nav_tile_cache.cpp
        src/nav_behaviors.cpp
        src/navigation_systems.cpp
        src/navigation_component_registration.cpp
        src/navigation_debug.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/navigation/navigation.hpp
            include/engine/navigation/navigation_systems.hpp
            include/engine/navigation/navmesh.hpp
            include/engine/navigation/navmesh_builder.hpp
            include/engine/navigation/pathfinder.hpp
            include/engine/navigation/nav_agent.hpp
            include/engine/navigation/nav_crowd.hpp
            include/engine/navigation/nav_tile_cache.hpp
            include/engine/navigation/nav_obstacle.hpp
            include/engine/navigation/nav_behaviors.hpp
            include/engine/navigation/navigation_debug.hpp
)

target_include_directories(engine_navigation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_navigation
    PUBLIC
        engine::core
        engine::scene
        engine::reflect
        Recast::Recast
        Recast::Detour
        Recast::DetourCrowd
        Recast::DetourTileCache
    PRIVATE
        engine_compiler_flags
        engine::debug-gui
        engine::render
)

set_target_properties(engine_navigation PROPERTIES
    FOLDER "engine"
)

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()
