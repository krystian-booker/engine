cmake_minimum_required(VERSION 3.21)
project({{PROJECT_NAME}} VERSION 1.0.0 LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the Engine package
find_package(Engine REQUIRED)

# Set runtime directory for DLL output
set_game_runtime_dir("${CMAKE_BINARY_DIR}/bin")

# Create the game DLL
add_game_dll(Game
    src/game_plugin.cpp
    src/game.cpp
    src/systems/player_system.cpp
)

# Copy assets to runtime directory
copy_game_assets(GameAssets "${CMAKE_SOURCE_DIR}/assets")

# Make sure assets are copied before running
add_dependencies(Game GameAssets)
