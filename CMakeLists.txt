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

# Source files - we'll add these as we create them
set(ENGINE_SOURCES
        src/main.cpp
        src/core/memory.cpp
        src/core/job_system.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/systems/transform_system.cpp
)

# Platform-specific source files
if(WIN32)
    list(APPEND ENGINE_SOURCES src/platform/win32_platform.cpp)
endif()

# Main executable
add_executable(engine ${ENGINE_SOURCES})

# Include directories
target_include_directories(engine PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# Link GLM
target_link_libraries(engine PRIVATE
        glm::glm
)

# Platform-specific linking
if(WIN32)
    # Windows libraries for platform layer
    target_link_libraries(engine PRIVATE User32 Gdi32)
endif()

# Enable testing
enable_testing()

# Test executable for memory allocators
add_executable(test_memory
        tests/test_memory.cpp
        src/core/memory.cpp
)

# Platform-specific source files for tests
if(WIN32)
    target_sources(test_memory PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(test_memory PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_memory PRIVATE
        glm::glm
)

# Platform-specific linking for tests
if(WIN32)
    target_link_libraries(test_memory PRIVATE User32 Gdi32)
endif()

# Register test with CTest
add_test(NAME MemoryAllocatorTests COMMAND test_memory)

# Test executable for job system
add_executable(test_job_system
        tests/test_job_system.cpp
        src/core/job_system.cpp
        src/core/memory.cpp
)

# Platform-specific source files for job system tests
if(WIN32)
    target_sources(test_job_system PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(test_job_system PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_job_system PRIVATE
        glm::glm
)

# Platform-specific linking for job system tests
if(WIN32)
    target_link_libraries(test_job_system PRIVATE User32 Gdi32)
endif()

# Register job system test with CTest
add_test(NAME JobSystemTests COMMAND test_job_system)

# Test executable for entity system
add_executable(test_entity
        tests/test_entity.cpp
        src/ecs/entity_manager.cpp
)

target_include_directories(test_entity PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_entity PRIVATE
        glm::glm
)

# Register entity test with CTest
add_test(NAME EntitySystemTests COMMAND test_entity)

# Test executable for component array
add_executable(test_component_array
        tests/test_component_array.cpp
        src/ecs/entity_manager.cpp
)

target_include_directories(test_component_array PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_component_array PRIVATE
        glm::glm
)

# Register component array test with CTest
add_test(NAME ComponentArrayTests COMMAND test_component_array)

# Test executable for component array example
add_executable(test_component_example
        tests/test_component_example.cpp
        src/ecs/entity_manager.cpp
)

target_include_directories(test_component_example PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_component_example PRIVATE
        glm::glm
)

# Register component example test with CTest
add_test(NAME ComponentExampleTest COMMAND test_component_example)

# Test executable for component registry
add_executable(test_component_registry
        tests/test_component_registry.cpp
        src/ecs/entity_manager.cpp
)

target_include_directories(test_component_registry PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_component_registry PRIVATE
        glm::glm
)

# Register component registry test with CTest
add_test(NAME ComponentRegistryTests COMMAND test_component_registry)
# Test executable for transform system
add_executable(test_transform
        tests/test_transform.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/systems/transform_system.cpp
)

target_include_directories(test_transform PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transform PRIVATE
        glm::glm
)

# Register transform test with CTest
add_test(NAME TransformSystemTests COMMAND test_transform)

# Test executable for ECS coordinator
add_executable(test_ecs_coordinator
        tests/test_ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/systems/transform_system.cpp
)

target_include_directories(test_ecs_coordinator PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_ecs_coordinator PRIVATE
        glm::glm
)

# Register ECS coordinator test with CTest
add_test(NAME ECSCoordinatorTests COMMAND test_ecs_coordinator)

# Test executable for hierarchy manager
add_executable(test_hierarchy_manager
        tests/test_hierarchy_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/entity_manager.cpp
)

target_include_directories(test_hierarchy_manager PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_hierarchy_manager PRIVATE
        glm::glm
)

# Register hierarchy manager test with CTest
add_test(NAME HierarchyManagerTests COMMAND test_hierarchy_manager)
