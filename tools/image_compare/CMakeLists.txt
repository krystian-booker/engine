# tools/image_compare/CMakeLists.txt
# CLI tool for comparing images via per-pixel RMSE

add_executable(image_compare)

target_sources(image_compare
    PRIVATE
        src/main.cpp
)

target_link_libraries(image_compare
    PRIVATE
        stb::stb
        engine_compiler_flags
)

set_target_properties(image_compare PROPERTIES
    FOLDER "tools"
)
