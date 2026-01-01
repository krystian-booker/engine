# engine/vegetation/CMakeLists.txt
# engine::vegetation - Vegetation system (grass, trees, foliage)

add_library(engine_vegetation STATIC)
add_library(engine::vegetation ALIAS engine_vegetation)

target_sources(engine_vegetation
    PRIVATE
        src/grass.cpp
        src/foliage.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/vegetation/vegetation.hpp
            include/engine/vegetation/grass.hpp
            include/engine/vegetation/foliage.hpp
)

target_include_directories(engine_vegetation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_vegetation
    PUBLIC
        engine::core
        engine::render
        engine::terrain
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_vegetation PROPERTIES
    FOLDER "engine"
)
