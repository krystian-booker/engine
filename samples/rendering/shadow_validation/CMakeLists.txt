add_executable(shadow_validation WIN32
    src/main.cpp
)

target_include_directories(shadow_validation
    PRIVATE src
)

target_link_libraries(shadow_validation
    PRIVATE
        engine::engine
        engine_compiler_flags
)

set_target_properties(shadow_validation PROPERTIES
    FOLDER "samples/rendering"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
