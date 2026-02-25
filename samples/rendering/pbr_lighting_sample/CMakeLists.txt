add_executable(pbr_lighting_sample WIN32
    src/main.cpp
)

target_include_directories(pbr_lighting_sample
    PRIVATE src
)

target_link_libraries(pbr_lighting_sample
    PRIVATE
        engine::engine
        engine_compiler_flags
)

set_target_properties(pbr_lighting_sample PROPERTIES
    FOLDER "samples/rendering"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
