# engine/plugin/CMakeLists.txt
# engine::plugin - Game DLL plugin system with hot reload

engine_add_module(
    NAME plugin
    SOURCES
        src/plugin_loader.cpp
        src/system_registry.cpp
        src/hot_reload_manager.cpp
    HEADERS
        include/engine/plugin/plugin.hpp
        include/engine/plugin/game_interface.hpp
        include/engine/plugin/plugin_loader.hpp
        include/engine/plugin/system_registry.hpp
        include/engine/plugin/hot_reload_manager.hpp
    PUBLIC_DEPS
        engine::core
        engine::scene
        nlohmann_json::nlohmann_json
)

# Platform-specific linking
if(NOT WIN32)
    target_link_libraries(engine_plugin PRIVATE ${CMAKE_DL_LIBS})
endif()
