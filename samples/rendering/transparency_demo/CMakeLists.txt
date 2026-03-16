# samples/rendering/transparency_demo/CMakeLists.txt

add_executable(transparency_demo WIN32)

target_sources(transparency_demo
    PRIVATE
        src/main.cpp
)

target_link_libraries(transparency_demo
    PRIVATE
        engine::engine
        engine_compiler_flags
)

set_target_properties(transparency_demo PROPERTIES
    FOLDER "samples/rendering"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
