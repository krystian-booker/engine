# CompilerFlags.cmake
# Compiler flags for MSVC, GCC, and Clang

add_library(engine_compiler_flags INTERFACE)

target_compile_features(engine_compiler_flags INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(engine_compiler_flags INTERFACE
        /W4
        /permissive-
        /utf-8
        /MP
    )
    target_compile_definitions(engine_compiler_flags INTERFACE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
else()
    target_compile_options(engine_compiler_flags INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow
        -Wconversion
        -pthread
    )
    target_link_options(engine_compiler_flags INTERFACE -pthread)
endif()

# Sanitizers (GCC/Clang only — MSVC ASan doesn't support link-time flag)
option(ENGINE_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENGINE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(ENGINE_ENABLE_ASAN)
    if(MSVC)
        target_compile_options(engine_compiler_flags INTERFACE /fsanitize=address)
    else()
        target_compile_options(engine_compiler_flags INTERFACE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(engine_compiler_flags INTERFACE -fsanitize=address)
    endif()
endif()

if(ENGINE_ENABLE_UBSAN)
    if(NOT MSVC)
        target_compile_options(engine_compiler_flags INTERFACE -fsanitize=undefined)
        target_link_options(engine_compiler_flags INTERFACE -fsanitize=undefined)
    endif()
endif()
