# editor/CMakeLists.txt
# Qt6 editor application

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(editor WIN32
    src/main.cpp
    src/main_window.cpp
    src/main_window.hpp
    resources/editor.qrc
)

target_link_libraries(editor
    PRIVATE
        engine::engine
        engine_compiler_flags
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

set_target_properties(editor PROPERTIES
    FOLDER "editor"
)

# Deploy Qt DLLs and plugins on Windows using windeployqt
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QTDIR}/bin")
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
