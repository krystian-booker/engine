# samples/spinning_cube/CMakeLists.txt
# Spinning Cube Demo - Milestone 1 proof of concept

add_executable(spinning_cube WIN32)

target_sources(spinning_cube
    PRIVATE
        src/main.cpp
)

target_link_libraries(spinning_cube
    PRIVATE
        engine::engine
        engine_compiler_flags
)

set_target_properties(spinning_cube PROPERTIES
    FOLDER "samples"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
