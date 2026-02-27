add_executable(tonemapping_validation WIN32
    src/main.cpp
)

target_include_directories(tonemapping_validation
    PRIVATE src
)

target_link_libraries(tonemapping_validation
    PRIVATE
        engine::engine
        engine_compiler_flags
)

set_target_properties(tonemapping_validation PROPERTIES
    FOLDER "samples/rendering"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
