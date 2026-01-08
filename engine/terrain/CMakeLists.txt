# engine/terrain/CMakeLists.txt
# engine::terrain - Terrain system with heightmaps, LOD, and multi-layer texturing

add_library(engine_terrain STATIC)
add_library(engine::terrain ALIAS engine_terrain)

target_sources(engine_terrain
    PRIVATE
        src/terrain.cpp
        src/heightmap.cpp
        src/terrain_renderer.cpp
        src/terrain_streaming.cpp
        src/terrain_component_registration.cpp
        src/terrain_systems.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/terrain/terrain.hpp
            include/engine/terrain/heightmap.hpp
            include/engine/terrain/terrain_renderer.hpp
            include/engine/terrain/terrain_lod.hpp
            include/engine/terrain/terrain_streaming.hpp
)

target_include_directories(engine_terrain
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_terrain
    PUBLIC
        engine::core
        engine::render
        engine::physics
        engine::scene
        engine::reflect
    PRIVATE
        engine_compiler_flags
        stb::stb
)

set_target_properties(engine_terrain PROPERTIES
    FOLDER "engine"
)

# Unit tests

