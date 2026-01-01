# engine/terrain/CMakeLists.txt
# engine::terrain - Terrain system with heightmaps, LOD, and multi-layer texturing

add_library(engine_terrain STATIC)
add_library(engine::terrain ALIAS engine_terrain)

target_sources(engine_terrain
    PRIVATE
        src/terrain.cpp
        src/heightmap.cpp
        src/terrain_renderer.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/terrain/terrain.hpp
            include/engine/terrain/heightmap.hpp
            include/engine/terrain/terrain_renderer.hpp
            include/engine/terrain/terrain_lod.hpp
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
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_terrain PROPERTIES
    FOLDER "engine"
)
