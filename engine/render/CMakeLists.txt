# engine/render/CMakeLists.txt
# engine::render - bgfx wrapper, renderer abstraction

# Include bgfx shader compilation utilities
include(${CMAKE_SOURCE_DIR}/external/bgfx.cmake/cmake/bgfxToolUtils.cmake)

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

# Compile shaders at build time
if(TARGET bgfx::shaderc)
    set(BGFX_SHADER_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/external/bgfx.cmake/bgfx/src")
    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    set(RUNTIME_SHADER_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")

    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_default.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR VS_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_default.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # Create custom target for shaders
    add_custom_target(engine_shaders DEPENDS ${VS_OUTPUTS} ${FS_OUTPUTS})

    # Copy compiled shaders next to the runtime binaries so the renderer can find them
    add_custom_command(TARGET engine_shaders POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RUNTIME_SHADER_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_OUTPUT_DIR} ${RUNTIME_SHADER_DIR}
        COMMENT "Copying BGFX shaders to ${RUNTIME_SHADER_DIR}"
    )

    add_dependencies(engine_render engine_shaders)
endif()
