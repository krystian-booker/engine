# engine/localization/CMakeLists.txt
# engine::localization - Localization and internationalization system

add_library(engine_localization STATIC)
add_library(engine::localization ALIAS engine_localization)

target_sources(engine_localization
    PRIVATE
        src/localization.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/localization/localization.hpp
)

target_include_directories(engine_localization
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_localization
    PUBLIC
        engine::core
    PRIVATE
        engine_compiler_flags
        nlohmann_json::nlohmann_json
)

set_target_properties(engine_localization PROPERTIES
    FOLDER "engine"
)
