# EngineGamePlugin.cmake
# Helper functions for creating game DLL plugins

# add_game_dll(target_name source1 [source2 ...])
#
# Creates a game DLL that can be loaded by the engine.
# The resulting DLL exports the required game plugin interface.
#
# Example:
#   add_game_dll(MyGame
#       src/game_plugin.cpp
#       src/game.cpp
#       src/systems/player_system.cpp
#   )
#
function(add_game_dll TARGET_NAME)
    # Create the shared library
    add_library(${TARGET_NAME} SHARED ${ARGN})

    # Link against the engine
    target_link_libraries(${TARGET_NAME}
        PRIVATE
            Engine::engine
    )

    # Set output name to "Game" for consistency
    set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "Game"
        # Ensure symbols are exported on Windows
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
    )

    # On Windows, we need to ensure proper DLL export
    if(WIN32)
        target_compile_definitions(${TARGET_NAME} PRIVATE GAME_DLL_EXPORT)
    endif()

    # Set C++20 standard
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

    # Copy DLL to runtime directory after build
    if(DEFINED ENGINE_RUNTIME_DIR)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                $<TARGET_FILE:${TARGET_NAME}>
                ${ENGINE_RUNTIME_DIR}/Game.dll
            COMMENT "Copying game DLL to runtime directory"
        )
    endif()
endfunction()

# set_game_runtime_dir(path)
#
# Sets the directory where game DLLs should be copied after build.
# This is typically where Engine.exe is located.
#
# Example:
#   set_game_runtime_dir("${CMAKE_BINARY_DIR}/bin")
#
function(set_game_runtime_dir PATH)
    set(ENGINE_RUNTIME_DIR "${PATH}" PARENT_SCOPE)
endfunction()

# copy_game_assets(target_name source_dir)
#
# Adds a custom target that copies assets from source_dir to the runtime directory.
#
# Example:
#   copy_game_assets(MyGame_Assets "${CMAKE_SOURCE_DIR}/assets")
#
function(copy_game_assets TARGET_NAME SOURCE_DIR)
    if(NOT DEFINED ENGINE_RUNTIME_DIR)
        message(WARNING "ENGINE_RUNTIME_DIR not set. Call set_game_runtime_dir() first.")
        return()
    endif()

    add_custom_target(${TARGET_NAME} ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SOURCE_DIR}"
            "${ENGINE_RUNTIME_DIR}/assets"
        COMMENT "Copying game assets to runtime directory"
    )
endfunction()
