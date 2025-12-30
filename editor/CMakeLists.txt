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

# Copy Qt DLLs on Windows
if(WIN32)
    add_custom_command(TARGET editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt6::Core>
            $<TARGET_FILE:Qt6::Gui>
            $<TARGET_FILE:Qt6::Widgets>
            $<TARGET_FILE_DIR:editor>
    )
endif()
