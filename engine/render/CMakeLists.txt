# engine/render/CMakeLists.txt
# engine::render - bgfx wrapper (device init, frame graph)

add_library(engine_render STATIC)
add_library(engine::render ALIAS engine_render)

target_sources(engine_render
    PRIVATE
        src/device.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/render/device.hpp
            include/engine/render/render.hpp
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
