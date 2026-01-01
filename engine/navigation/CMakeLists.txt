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
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/navigation/navigation.hpp
            include/engine/navigation/navmesh.hpp
            include/engine/navigation/navmesh_builder.hpp
            include/engine/navigation/pathfinder.hpp
            include/engine/navigation/nav_agent.hpp
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
        Recast::Recast
        Recast::Detour
        Recast::DetourCrowd
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_navigation PROPERTIES
    FOLDER "engine"
)
