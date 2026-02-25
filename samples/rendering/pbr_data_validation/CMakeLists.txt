add_executable(pbr_data_validation WIN32
    src/main.cpp
)

target_include_directories(pbr_data_validation
    PRIVATE src
)

target_link_libraries(pbr_data_validation
    PRIVATE
        engine::engine
        engine_compiler_flags
)

set_target_properties(pbr_data_validation PROPERTIES
    FOLDER "samples/rendering"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
