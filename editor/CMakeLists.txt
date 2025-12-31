# editor/CMakeLists.txt
# Qt6 editor application

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(editor WIN32
    src/main.cpp
    src/main_window.hpp
    src/main_window.cpp
    src/editor_state.hpp
    src/editor_state.cpp
    src/viewport_widget.hpp
    src/viewport_widget.cpp
    src/hierarchy_panel.hpp
    src/hierarchy_panel.cpp
    src/inspector_panel.hpp
    src/inspector_panel.cpp
    src/asset_browser.hpp
    src/asset_browser.cpp
    src/console_panel.hpp
    src/console_panel.cpp
    src/entity_icons.hpp
    src/entity_icons.cpp
    resources/editor.qrc
)

set_target_properties(editor PROPERTIES
    FOLDER "editor"
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

target_link_libraries(editor
    PRIVATE
        engine::engine
        engine_compiler_flags
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

# Deploy Qt DLLs on Windows
if(WIN32)
    # Get windeployqt from Qt installation
    get_target_property(QT6_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT6_BIN_DIR "${QT6_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT6_BIN_DIR}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET editor POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:editor>"
            COMMENT "Running windeployqt..."
        )
    endif()
endif()
