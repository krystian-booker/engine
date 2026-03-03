add_executable(ibl_validation WIN32
    src/main.cpp
)

target_include_directories(ibl_validation
    PRIVATE src
)

target_link_libraries(ibl_validation
    PRIVATE
        engine::engine
        engine_compiler_flags
        stb::stb
)

set_target_properties(ibl_validation PROPERTIES
    FOLDER "samples/rendering"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# Copy HDR environment map to build output directory
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/pisa.hdr"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pisa.hdr"
    COPYONLY
)
