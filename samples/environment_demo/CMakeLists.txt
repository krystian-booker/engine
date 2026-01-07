# samples/environment_demo/CMakeLists.txt
# Environment Demo - Demonstrates TimeOfDay and SkyController systems

add_executable(environment_demo WIN32)

target_sources(environment_demo
    PRIVATE
        src/main.cpp
)

target_link_libraries(environment_demo
    PRIVATE
        engine::engine
        engine_compiler_flags
)

set_target_properties(environment_demo PROPERTIES
    FOLDER "samples"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
