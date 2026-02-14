# samples/render_test/CMakeLists.txt
# Deterministic render test scene for golden image visual testing

add_executable(render_test WIN32)

target_sources(render_test
    PRIVATE
        src/main.cpp
)

target_link_libraries(render_test
    PRIVATE
        engine::engine
        engine_compiler_flags
)

set_target_properties(render_test PROPERTIES
    FOLDER "samples"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
