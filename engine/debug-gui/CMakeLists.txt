# engine/debug-gui/CMakeLists.txt
# engine::debug-gui - Debug GUI overlays using DearImGui

# Only build full implementation in Debug configurations
# For Release, provide empty interface target
set(IS_DEBUG_BUILD FALSE)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(IS_DEBUG_BUILD TRUE)
elseif("Debug" IN_LIST CMAKE_CONFIGURATION_TYPES)
    set(IS_DEBUG_BUILD TRUE)
endif()

if(NOT IS_DEBUG_BUILD)
    # Release build - create empty interface target
    add_library(engine_debug_gui INTERFACE)
    add_library(engine::debug-gui ALIAS engine_debug_gui)
    target_compile_definitions(engine_debug_gui INTERFACE ENGINE_DEBUG_GUI_DISABLED=1)
    message(STATUS "debug-gui: Disabled in Release build")
    return()
endif()

# ImGui source from bgfx 3rdparty - use FetchContent variable
set(IMGUI_DIR ${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty/dear-imgui)

# Verify ImGui exists
if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
    message(WARNING "ImGui not found at ${IMGUI_DIR}, debug-gui will be disabled")
    add_library(engine_debug_gui INTERFACE)
    add_library(engine::debug-gui ALIAS engine_debug_gui)
    target_compile_definitions(engine_debug_gui INTERFACE ENGINE_DEBUG_GUI_DISABLED=1)
    return()
endif()

message(STATUS "debug-gui: Using ImGui from ${IMGUI_DIR}")

add_library(engine_debug_gui STATIC)
add_library(engine::debug-gui ALIAS engine_debug_gui)

# ImGui source files
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)

target_sources(engine_debug_gui
    PRIVATE
        src/imgui_layer.cpp
        src/debug_gui_manager.cpp
        src/debug_console.cpp
        src/debug_profiler.cpp
        src/debug_entity_inspector.cpp
        ${IMGUI_SOURCES}
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/debug-gui/debug_gui.hpp
            include/engine/debug-gui/imgui_layer.hpp
            include/engine/debug-gui/debug_window.hpp
            include/engine/debug-gui/debug_gui_manager.hpp
            include/engine/debug-gui/debug_console.hpp
            include/engine/debug-gui/debug_profiler.hpp
            include/engine/debug-gui/debug_entity_inspector.hpp
)

target_include_directories(engine_debug_gui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${IMGUI_DIR}
    PRIVATE
        ${PROJECT_BINARY_DIR}/_deps/stb-src
)

target_compile_definitions(engine_debug_gui
    PUBLIC
        ENGINE_DEBUG_GUI_ENABLED=1
    PRIVATE
        IMGUI_DEFINE_MATH_OPERATORS
)

target_link_libraries(engine_debug_gui
    PUBLIC
        engine::core
        engine::render
        engine::scene
        engine::reflect
        bgfx::bgfx
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_debug_gui PROPERTIES
    FOLDER "engine"
)
