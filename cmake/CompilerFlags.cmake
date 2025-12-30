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
        $<$<CONFIG:Release>:/O2 /Ob2 /DNDEBUG>
        $<$<CONFIG:Debug>:/Od /Zi>
    )
    target_compile_definitions(engine_compiler_flags INTERFACE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
else()
    target_compile_options(engine_compiler_flags INTERFACE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
        $<$<CONFIG:Debug>:-O0 -g>
    )
endif()
