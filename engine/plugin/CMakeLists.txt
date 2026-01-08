# engine/plugin/CMakeLists.txt
# engine::plugin - Game DLL plugin system with hot reload

add_library(engine_plugin STATIC)
add_library(engine::plugin ALIAS engine_plugin)

target_sources(engine_plugin
    PRIVATE
        src/plugin_loader.cpp
        src/system_registry.cpp
        src/hot_reload_manager.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/plugin/plugin.hpp
            include/engine/plugin/game_interface.hpp
            include/engine/plugin/plugin_loader.hpp
            include/engine/plugin/system_registry.hpp
            include/engine/plugin/hot_reload_manager.hpp
)

target_include_directories(engine_plugin
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_plugin
    PUBLIC
        engine::core
        engine::scene
        nlohmann_json::nlohmann_json
    PRIVATE
        engine_compiler_flags
)

# Platform-specific linking
if(WIN32)
    # No additional libs needed - uses LoadLibrary/FreeLibrary
else()
    target_link_libraries(engine_plugin PRIVATE dl)
endif()

set_target_properties(engine_plugin PROPERTIES
    FOLDER "engine"
)

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()
