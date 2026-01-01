# engine/ui/CMakeLists.txt
# engine::ui - Retained-mode game UI system

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
