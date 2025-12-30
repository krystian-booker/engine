cmake_minimum_required(VERSION 3.21)


project(Engine
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "C++ Game Engine"
)

# Global settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# IDE folder organization
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Custom CMake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Dependencies (bgfx.cmake)
include(Dependencies)

# Compiler flags
include(CompilerFlags)

# Options
option(ENGINE_BUILD_EDITOR "Build the Qt6 editor application" ON)

# Engine libraries
add_subdirectory(engine)

# Editor (optional)
if(ENGINE_BUILD_EDITOR)
    add_subdirectory(editor)
endif()
