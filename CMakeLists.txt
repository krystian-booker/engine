cmake_minimum_required(VERSION 3.21)


project(Engine
    VERSION 0.1.0
    LANGUAGES C CXX
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

# Dependencies (bgfx.cmake, EnTT, GLM, etc.)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
include(Dependencies)

# Compiler flags
include(CompilerFlags)

# Options
option(ENGINE_BUILD_EDITOR "Build the Qt6 editor application" ON)
option(ENGINE_BUILD_SAMPLES "Build sample applications" ON)
option(ENGINE_BUILD_TESTS "Build unit tests" OFF)
option(ENGINE_INSTALL "Generate install targets (disable for dev builds)" OFF)

# Testing
if(ENGINE_BUILD_TESTS)
    enable_testing()
    include(CTest)
endif()

# Engine libraries
add_subdirectory(engine)

# Editor (optional)
if(ENGINE_BUILD_EDITOR)
    # Auto-detect Qt6 on Windows if not already configured
    if(WIN32 AND NOT Qt6_DIR AND NOT DEFINED ENV{QTDIR})
        # Search common Qt installation paths
        file(GLOB QT_ROOT_CANDIDATES "C:/Qt/6.*")
        if(QT_ROOT_CANDIDATES)
            # Sort to get the latest version (highest number last)
            list(SORT QT_ROOT_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
            list(GET QT_ROOT_CANDIDATES 0 QT_LATEST_VERSION)

            # Look for MSVC compiler variant (prefer 2022, then 2019)
            foreach(MSVC_VARIANT msvc2022_64 msvc2019_64)
                set(QT_CANDIDATE_PATH "${QT_LATEST_VERSION}/${MSVC_VARIANT}")
                if(EXISTS "${QT_CANDIDATE_PATH}/lib/cmake/Qt6/Qt6Config.cmake")
                    message(STATUS "Auto-detected Qt6 at: ${QT_CANDIDATE_PATH}")
                    list(PREPEND CMAKE_PREFIX_PATH "${QT_CANDIDATE_PATH}")
                    break()
                endif()
            endforeach()
        endif()
    endif()

    add_subdirectory(editor)
endif()

# Samples (optional)
if(ENGINE_BUILD_SAMPLES)
    add_subdirectory(samples/spinning_cube)
    add_subdirectory(samples/environment_demo)
    add_subdirectory(samples/bouncing_balls)
    add_subdirectory(samples/ai_demo)
endif()

# CLI Tool for game development
add_subdirectory(tools/engine-cli)

# Installation rules (optional, disabled by default for dev builds)
if(ENGINE_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install engine libraries
    install(TARGETS
        engine_core
        engine_reflect
        engine_scene
        engine_script
        engine_render
        engine_asset
        engine_physics
        engine_audio
        engine_navigation
        engine_ui
        engine_debug_gui
        engine_save
        engine_localization
        engine_streaming
        engine_terrain
        engine_vegetation
        engine_cinematic
        engine_plugin
        engine
        EXPORT EngineTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILE_SET public_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install the export targets
    install(EXPORT EngineTargets
        FILE EngineTargets.cmake
        NAMESPACE Engine::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Engine
    )

    # Generate package config files
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EngineConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/EngineConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Engine
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/EngineConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/EngineConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/EngineConfigVersion.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EngineGamePlugin.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Engine
    )

    # Install template project
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/templates/game_template/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/engine/templates/game_template
    )
endif()
