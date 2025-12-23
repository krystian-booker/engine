cmake_minimum_required(VERSION 3.20)
project(engine)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX)  # Warning level 4, warnings as errors
    add_compile_options(/MP)       # Multi-processor compilation
else()
    # GCC/Clang/MinGW
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Platform detection
if(WIN32)
    add_compile_definitions(PLATFORM_WINDOWS)
elseif(UNIX AND NOT APPLE)
    add_compile_definitions(PLATFORM_LINUX)
elseif(APPLE)
    add_compile_definitions(PLATFORM_MACOS)
endif()

# Fetch GLM
include(FetchContent)
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
        GIT_SHALLOW    TRUE
)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

# Fetch nlohmann/json
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

# Fetch portable-file-dialogs (single-header file dialog library)
FetchContent_Declare(
        portable_file_dialogs
        URL https://github.com/samhocevar/portable-file-dialogs/archive/refs/heads/main.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(portable_file_dialogs)
if(NOT portable_file_dialogs_POPULATED)
    FetchContent_Populate(portable_file_dialogs)
endif()

# ==============================================================================
# Component Libraries
# ==============================================================================

# -------------------- Platform Library --------------------
add_library(engine_platform STATIC
        src/platform/platform.h
)

# Make it header-only for now (no window/input in headless mode)
set_target_properties(engine_platform PROPERTIES LINKER_LANGUAGE CXX)

if(WIN32)
    target_sources(engine_platform PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(engine_platform PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(engine_platform PUBLIC
        glm::glm
)

if(WIN32)
    target_link_libraries(engine_platform PUBLIC User32 Gdi32)
endif()

target_compile_features(engine_platform PUBLIC cxx_std_20)

# -------------------- Core Library --------------------
add_library(engine_core STATIC
        src/core/memory.cpp
        src/core/job_system.cpp
        src/core/time.cpp
        src/core/file_watcher.cpp
        src/core/file_dialog.cpp
        src/core/scene_manager.cpp
        src/core/project_manager.cpp
        src/core/engine_settings.cpp
)

target_include_directories(engine_core PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${portable_file_dialogs_SOURCE_DIR}
)

target_link_libraries(engine_core PUBLIC
        glm::glm
        nlohmann_json::nlohmann_json
        engine_platform
)

target_compile_features(engine_core PUBLIC cxx_std_20)

# -------------------- ECS Library --------------------
add_library(engine_ecs STATIC
        src/ecs/ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/scene_serializer.cpp
        src/ecs/systems/transform_system.cpp
)

target_include_directories(engine_ecs PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(engine_ecs PUBLIC
        glm::glm
        nlohmann_json::nlohmann_json
        engine_core
)

target_compile_definitions(engine_ecs PUBLIC ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

target_compile_features(engine_ecs PUBLIC cxx_std_20)

# ==============================================================================
# Main Engine Executable
# ==============================================================================

add_executable(engine
        src/main.cpp
)

target_include_directories(engine PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# Link all component libraries
target_link_libraries(engine PRIVATE
        engine_core
        engine_ecs
)

target_compile_definitions(engine PRIVATE ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Platform-specific linking
if(WIN32)
    # Windows libraries for file dialogs (COM)
    target_link_libraries(engine PRIVATE Ole32 Shell32)
endif()

# Apply target-specific compiler flags
if(MSVC)
    target_compile_options(engine PRIVATE /W4 /WX /MP)
else()
    target_compile_options(engine PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Platform detection compile definitions
if(WIN32)
    target_compile_definitions(engine PRIVATE PLATFORM_WINDOWS)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(engine PRIVATE PLATFORM_LINUX)
elseif(APPLE)
    target_compile_definitions(engine PRIVATE PLATFORM_MACOS)
endif()

# Enable testing
enable_testing()

# ==============================================================================
# Helper Functions
# ==============================================================================

# Helper function to add engine tests with minimal boilerplate
function(add_engine_test TEST_NAME)
    cmake_parse_arguments(ARG "" "TEST_FILE;LABEL;TIMEOUT" "SOURCES;LIBS;COMPONENTS" ${ARGN})

    # Default test file path
    if(NOT ARG_TEST_FILE)
        set(ARG_TEST_FILE "tests/${TEST_NAME}.cpp")
    endif()

    # Create test executable
    add_executable(${TEST_NAME}
        ${ARG_TEST_FILE}
        ${ARG_SOURCES}
    )

    # Include directories
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${portable_file_dialogs_SOURCE_DIR}
    )

    # Link component libraries
    foreach(comp ${ARG_COMPONENTS})
        target_link_libraries(${TEST_NAME} PRIVATE engine_${comp})
    endforeach()

    # Link additional libraries
    target_link_libraries(${TEST_NAME} PRIVATE ${ARG_LIBS})

    # Always link GLM for tests
    target_link_libraries(${TEST_NAME} PRIVATE glm::glm)

    # Apply compiler flags
    if(MSVC)
        target_compile_options(${TEST_NAME} PRIVATE /W4 /WX /MP)
    else()
        target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    # Platform detection
    if(WIN32)
        target_compile_definitions(${TEST_NAME} PRIVATE PLATFORM_WINDOWS)
    elseif(UNIX AND NOT APPLE)
        target_compile_definitions(${TEST_NAME} PRIVATE PLATFORM_LINUX)
    elseif(APPLE)
        target_compile_definitions(${TEST_NAME} PRIVATE PLATFORM_MACOS)
    endif()

    # Add ENGINE_SOURCE_DIR for tests that need it
    target_compile_definitions(${TEST_NAME} PRIVATE ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

    # Register with CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    # Set test properties
    if(ARG_LABEL)
        set_tests_properties(${TEST_NAME} PROPERTIES LABELS "${ARG_LABEL}")
    endif()

    # Set timeout (default 30 seconds)
    if(ARG_TIMEOUT)
        set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT ${ARG_TIMEOUT})
    else()
        set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 30)
    endif()
endfunction()

# ==============================================================================
# Tests
# ==============================================================================

# Test executable for memory allocators
add_engine_test(test_memory
    COMPONENTS platform
    SOURCES src/core/memory.cpp
)

# Test executable for job system
add_engine_test(test_job_system
    COMPONENTS core platform
)

# Test executable for entity system
add_engine_test(test_entity
    SOURCES src/ecs/entity_manager.cpp
)

# Test executable for component array
add_engine_test(test_component_array
    SOURCES src/ecs/entity_manager.cpp
)

# Test executable for component array example
add_engine_test(test_component_example
    SOURCES src/ecs/entity_manager.cpp
)

# Test executable for component registry
add_engine_test(test_component_registry
    SOURCES src/ecs/entity_manager.cpp
)

# Test executable for left-handed math validation
add_engine_test(test_left_handed_math)

# Test executable for transform system
add_engine_test(test_transform
    SOURCES src/ecs/entity_manager.cpp src/ecs/hierarchy_manager.cpp src/ecs/systems/transform_system.cpp
)

# Test executable for ECS coordinator
add_engine_test(test_ecs_coordinator
    COMPONENTS ecs
)

# Test executable for ECS queries
add_engine_test(test_ecs_queries
    COMPONENTS ecs
)

# Test executable for ECS deferred operations
add_engine_test(test_ecs_deferred
    COMPONENTS ecs
)

# Test executable for ECS versioning
add_engine_test(test_ecs_versions
    COMPONENTS ecs
)

# Test executable for ECS signatures
add_engine_test(test_ecs_signatures
    COMPONENTS ecs
)

# Test executable for ECS parallel operations
add_engine_test(test_ecs_parallel
    COMPONENTS ecs core platform
)

# Test executable for hierarchy manager
add_engine_test(test_hierarchy_manager
    SOURCES src/ecs/hierarchy_manager.cpp src/ecs/entity_manager.cpp
)

# Test executable for time system
add_engine_test(test_time
    COMPONENTS core
    SOURCES src/core/time.cpp
)

# Test executable for file watcher system
add_engine_test(test_file_watcher
    SOURCES src/core/file_watcher.cpp
)
