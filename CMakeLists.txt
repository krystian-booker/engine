cmake_minimum_required(VERSION 3.25)

# Auto-detect vcpkg toolchain (must be before project())
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    # 1) VCPKG_ROOT environment variable
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    # 2) vcpkg cloned inside the repo (vcpkg/ subdirectory)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    # 3) vcpkg integrate install (writes path to LOCALAPPDATA on Windows)
    elseif(WIN32 AND DEFINED ENV{LOCALAPPDATA})
        set(_vcpkg_path_file "$ENV{LOCALAPPDATA}/vcpkg/vcpkg.path.txt")
        if(EXISTS "${_vcpkg_path_file}")
            file(READ "${_vcpkg_path_file}" _vcpkg_root)
            string(STRIP "${_vcpkg_root}" _vcpkg_root)
            if(EXISTS "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
                set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake"
                    CACHE STRING "Vcpkg toolchain file")
            endif()
        endif()
        unset(_vcpkg_path_file)
        unset(_vcpkg_root)
    endif()
endif()

project(Engine
    VERSION 0.1.0
    LANGUAGES C CXX
    DESCRIPTION "C++ Game Engine"
)

# IDE folder organization
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Custom CMake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Dependencies (bgfx.cmake, EnTT, GLM, etc.)
include(Dependencies)

# Compiler flags
include(CompilerFlags)

# Module helper
include(EngineModule)

# Options
option(ENGINE_BUILD_TESTS "Build unit tests" OFF)

# Testing
if(ENGINE_BUILD_TESTS)
    enable_testing()
    include(CTest)
endif()

# Engine libraries
add_subdirectory(engine)

# CLI Tool for game development
add_subdirectory(tools/engine-cli)
