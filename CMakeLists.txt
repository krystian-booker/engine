cmake_minimum_required(VERSION 4.0)
project(engine)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Option to enable sanitizers (off by default for MinGW compatibility)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX)  # Warning level 4, warnings as errors
    add_compile_options(/MP)       # Multi-processor compilation

    # Sanitizers for MSVC (limited support)
    if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/fsanitize=address)
    endif()
else()
    # GCC/Clang/MinGW
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)

    # Sanitizers for GCC/Clang (requires libraries that MinGW might not have)
    if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Check if sanitizers are available
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-fsanitize=address" HAS_ASAN)
        check_cxx_compiler_flag("-fsanitize=undefined" HAS_UBSAN)

        if(HAS_ASAN)
            add_compile_options(-fsanitize=address)
            add_link_options(-fsanitize=address)
        endif()

        if(HAS_UBSAN)
            add_compile_options(-fsanitize=undefined)
            add_link_options(-fsanitize=undefined)
        endif()
    endif()
endif()

# Platform detection
if(WIN32)
    add_compile_definitions(PLATFORM_WINDOWS)
elseif(UNIX AND NOT APPLE)
    add_compile_definitions(PLATFORM_LINUX)
elseif(APPLE)
    add_compile_definitions(PLATFORM_MACOS)
endif()

# Source files - we'll add these as we create them
set(ENGINE_SOURCES
        src/main.cpp
        src/core/math.h
)

# Main executable
add_executable(engine ${ENGINE_SOURCES})

# Include directories
target_include_directories(engine PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# Platform-specific linking
if(WIN32)
    # Windows libraries (we'll add these as needed)
    # target_link_libraries(engine PRIVATE User32 Gdi32)
endif()

# Enable testing
enable_testing()
