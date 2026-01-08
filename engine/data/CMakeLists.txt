# engine/data/CMakeLists.txt
# engine::data - Data tables for designer-friendly configuration

add_library(engine_data STATIC)
add_library(engine::data ALIAS engine_data)

target_sources(engine_data
    PRIVATE
        src/data_table.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/data/data_table.hpp
            include/engine/data/data.hpp
            include/engine/data/json_loader.hpp
)

target_include_directories(engine_data
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_data
    PUBLIC
        engine::core
    PRIVATE
        engine_compiler_flags
        nlohmann_json::nlohmann_json
)

set_target_properties(engine_data PROPERTIES
    FOLDER "engine"
)

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()
