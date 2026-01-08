# samples/ai_demo/CMakeLists.txt
# AI Demo - Demonstrates behavior trees, perception, patrol, and combat

add_executable(ai_demo WIN32)

target_sources(ai_demo
    PRIVATE
        src/main.cpp
)

target_link_libraries(ai_demo
    PRIVATE
        engine::engine
        engine_compiler_flags
)

set_target_properties(ai_demo PROPERTIES
    FOLDER "samples"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
