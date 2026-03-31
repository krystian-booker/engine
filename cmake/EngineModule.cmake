# EngineModule.cmake
# Helper function to reduce boilerplate in engine module CMakeLists.txt files.
#
# Usage:
#   engine_add_module(
#       NAME core
#       SOURCES src/log.cpp src/job_system.cpp
#       HEADERS include/engine/core/log.hpp include/engine/core/job_system.hpp
#       PUBLIC_DEPS glm::glm nlohmann_json::nlohmann_json
#       PRIVATE_DEPS some_internal_lib
#   )
#
# This creates:
#   - engine_${NAME} STATIC library
#   - engine::${NAME} alias
#   - FILE_SET public headers
#   - BUILD_INTERFACE / INSTALL_INTERFACE include directories
#   - Links engine_compiler_flags PRIVATE
#   - Sets FOLDER "engine"
#   - Conditionally adds tests/ subdirectory when ENGINE_BUILD_TESTS is ON

function(engine_add_module)
    cmake_parse_arguments(MOD "" "NAME" "SOURCES;HEADERS;PUBLIC_DEPS;PRIVATE_DEPS" ${ARGN})

    if(NOT MOD_NAME)
        message(FATAL_ERROR "engine_add_module: NAME is required")
    endif()

    set(_target engine_${MOD_NAME})

    add_library(${_target} STATIC)
    add_library(engine::${MOD_NAME} ALIAS ${_target})

    target_sources(${_target}
        PRIVATE
            ${MOD_SOURCES}
        PUBLIC
            FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS include
            FILES
                ${MOD_HEADERS}
    )

    target_include_directories(${_target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(${_target}
        PUBLIC
            ${MOD_PUBLIC_DEPS}
        PRIVATE
            engine_compiler_flags
            ${MOD_PRIVATE_DEPS}
    )

    set_target_properties(${_target} PROPERTIES
        FOLDER "engine"
    )

    if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endfunction()
