# engine/ui/CMakeLists.txt
# engine::ui - Retained-mode game UI system

# Include bgfx shader compilation utilities
include(${bgfx_cmake_SOURCE_DIR}/cmake/bgfxToolUtils.cmake)

add_library(engine_ui STATIC)
add_library(engine::ui ALIAS engine_ui)

target_sources(engine_ui
    PRIVATE
        src/ui_context.cpp
        src/ui_canvas.cpp
        src/ui_element.cpp
        src/ui_panel.cpp
        src/ui_button.cpp
        src/ui_label.cpp
        src/ui_image.cpp
        src/ui_slider.cpp
        src/ui_text_input.cpp
        src/ui_font.cpp
        src/ui_renderer.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/ui/ui.hpp
            include/engine/ui/ui_context.hpp
            include/engine/ui/ui_canvas.hpp
            include/engine/ui/ui_element.hpp
            include/engine/ui/ui_elements.hpp
            include/engine/ui/ui_style.hpp
            include/engine/ui/ui_font.hpp
            include/engine/ui/ui_renderer.hpp
            include/engine/ui/ui_types.hpp
)

target_include_directories(engine_ui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_ui
    PUBLIC
        engine::core
        engine::render
        stb::stb
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_ui PROPERTIES
    FOLDER "engine"
)

# Compile UI shaders at build time
if(TARGET bgfx::shaderc AND ENGINE_COMPILE_SHADERS)
    set(BGFX_SHADER_INCLUDE_PATH "${bgfx_cmake_SOURCE_DIR}/bgfx/src")
    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")

    # UI vertex shader
    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_ui.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR VS_UI_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # UI fragment shader
    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_ui.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_UI_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # Create custom target for UI shaders
    add_custom_target(engine_ui_shaders DEPENDS
        ${VS_UI_OUTPUTS} ${FS_UI_OUTPUTS}
    )

    add_dependencies(engine_ui engine_ui_shaders)

    # Add UI shaders to the main engine_shaders target if it exists
    if(TARGET engine_shaders)
        add_dependencies(engine_shaders engine_ui_shaders)
    endif()
endif()
