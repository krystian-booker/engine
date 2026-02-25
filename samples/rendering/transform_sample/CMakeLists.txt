add_executable(transform_sample WIN32
    src/main.cpp
)

target_include_directories(transform_sample
    PRIVATE src
)

target_link_libraries(transform_sample
    PRIVATE
        engine::engine
        engine_compiler_flags
)

set_target_properties(transform_sample PROPERTIES
    FOLDER "samples/rendering"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
