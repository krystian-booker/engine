# samples/bouncing_balls/CMakeLists.txt
# Bouncing Balls Demo - Physics simulation with 50 bouncing spheres

add_executable(bouncing_balls_test WIN32)

target_sources(bouncing_balls_test
    PRIVATE
        src/main.cpp
)

target_link_libraries(bouncing_balls_test
    PRIVATE
        engine::engine
        engine_compiler_flags
)

set_target_properties(bouncing_balls_test PROPERTIES
    FOLDER "samples"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
