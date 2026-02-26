add_executable(pbr_analytical_final WIN32
    src/main.cpp
)

target_include_directories(pbr_analytical_final
    PRIVATE src
)

target_link_libraries(pbr_analytical_final
    PRIVATE
        engine::engine
        engine_compiler_flags
)

set_target_properties(pbr_analytical_final PROPERTIES
    FOLDER "samples/rendering"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
