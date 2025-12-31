# engine/render/CMakeLists.txt
# engine::render - bgfx wrapper, renderer abstraction

add_library(engine_render STATIC)
add_library(engine::render ALIAS engine_render)

target_sources(engine_render
    PRIVATE
        src/device.cpp
        src/bgfx_renderer.cpp
        src/debug_draw.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/render/device.hpp
            include/engine/render/render.hpp
            include/engine/render/renderer.hpp
            include/engine/render/types.hpp
            include/engine/render/debug_draw.hpp
)

target_include_directories(engine_render
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_render
    PUBLIC
        engine::core
        bgfx::bgfx
        bgfx::bimg
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_render PROPERTIES
    FOLDER "engine"
)
