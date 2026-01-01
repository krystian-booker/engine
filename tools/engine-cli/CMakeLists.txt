# tools/engine-cli/CMakeLists.txt
# Engine CLI tool for project management

add_executable(engine-cli
    src/main.cpp
    src/commands.cpp
)

target_include_directories(engine-cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(engine-cli
    PRIVATE
        nlohmann_json::nlohmann_json
        engine_compiler_flags
)

# Set output name to "engine" for user convenience
set_target_properties(engine-cli PROPERTIES
    OUTPUT_NAME "engine"
    FOLDER "tools"
)

# Install the CLI tool
install(TARGETS engine-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
