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

# Fetch nlohmann/json
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

# Fetch GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_LIBRARY_TYPE "STATIC" CACHE STRING "" FORCE)
FetchContent_MakeAvailable(glfw)

# Vulkan
find_package(Vulkan REQUIRED)

# GLFW is external code - disable warnings-as-errors for it
if(TARGET glfw)
    # Remove -Werror/-WX from GLFW target
    get_target_property(GLFW_COMPILE_OPTIONS glfw COMPILE_OPTIONS)
    if(GLFW_COMPILE_OPTIONS)
        list(REMOVE_ITEM GLFW_COMPILE_OPTIONS "-Werror" "/WX")
        set_target_properties(glfw PROPERTIES COMPILE_OPTIONS "${GLFW_COMPILE_OPTIONS}")
    endif()
    # Also suppress warnings entirely for GLFW
    if(MSVC)
        target_compile_options(glfw PRIVATE /W0)
    else()
        target_compile_options(glfw PRIVATE -w)
    endif()
endif()

# Source files - we'll add these as we create them
set(ENGINE_SOURCES
        src/main.cpp
        src/core/memory.cpp
        src/core/job_system.cpp
        src/core/time.cpp
        src/core/file_watcher.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/scene_serializer.cpp
        src/ecs/systems/transform_system.cpp
        src/platform/window.cpp
        src/platform/input.cpp
        src/resources/mesh_manager.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_swapchain.cpp
        src/renderer/vulkan_renderer.cpp
        src/renderer/vulkan_render_pass.cpp
        src/renderer/vulkan_framebuffers.cpp
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

# Link GLM, JSON, and GLFW
target_link_libraries(engine PRIVATE
        glm::glm
        nlohmann_json::nlohmann_json
        glfw
        Vulkan::Vulkan
)

target_compile_features(engine PRIVATE cxx_std_20)

# Platform-specific linking
if(WIN32)
    # Windows libraries for platform layer
    target_link_libraries(engine PRIVATE User32 Gdi32)
endif()

# Enable testing
enable_testing()

add_executable(test_vulkan
        tests/test_vulkan.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_swapchain.cpp
        src/renderer/vulkan_renderer.cpp
        src/renderer/vulkan_render_pass.cpp
        src/renderer/vulkan_framebuffers.cpp
        src/platform/window.cpp
)

target_include_directories(test_vulkan PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_vulkan PRIVATE
        glfw
        Vulkan::Vulkan
)

if(WIN32)
    target_sources(test_vulkan PRIVATE src/platform/win32_platform.cpp)
    target_link_libraries(test_vulkan PRIVATE User32 Gdi32)
endif()

add_test(NAME VulkanTests COMMAND test_vulkan)

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

# Test executable for scene serializer
add_executable(test_scene_serializer
        tests/test_scene_serializer.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/scene_serializer.cpp
        src/ecs/systems/transform_system.cpp
)

target_include_directories(test_scene_serializer PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_scene_serializer PRIVATE
        glm::glm
        nlohmann_json::nlohmann_json
)

# Register scene serializer test with CTest
add_test(NAME SceneSerializerTests COMMAND test_scene_serializer)

# Test executable for time system
add_executable(test_time
        tests/test_time.cpp
        src/core/time.cpp
)

target_include_directories(test_time PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_time PRIVATE
        glm::glm
        glfw
)

# Register time system test with CTest
add_test(NAME TimeSystemTests COMMAND test_time)

# Test executable for resource handle system
add_executable(test_resource_handle
        tests/test_resource_handle.cpp
        src/resources/mesh_manager.cpp
)

target_include_directories(test_resource_handle PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_resource_handle PRIVATE
        glm::glm
)

# Register resource handle test with CTest
add_test(NAME ResourceHandleTests COMMAND test_resource_handle)

# Example executable for resource handle system
add_executable(resource_handle_example
        examples/resource_handle_example.cpp
        src/resources/mesh_manager.cpp
)

target_include_directories(resource_handle_example PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(resource_handle_example PRIVATE
        glm::glm
)

# Test executable for file watcher system
add_executable(test_file_watcher
        tests/test_file_watcher.cpp
        src/core/file_watcher.cpp
)

target_include_directories(test_file_watcher PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_file_watcher PRIVATE
        glm::glm
)

# Register file watcher test with CTest
add_test(NAME FileWatcherTests COMMAND test_file_watcher)

# Example executable for file watcher system
add_executable(file_watcher_example
        examples/file_watcher_example.cpp
        src/core/file_watcher.cpp
        src/core/time.cpp
        src/platform/window.cpp
        src/platform/input.cpp
)

# Platform-specific source files for file watcher example
if(WIN32)
    target_sources(file_watcher_example PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(file_watcher_example PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(file_watcher_example PRIVATE
        glm::glm
        glfw
)

# Platform-specific linking for file watcher example
if(WIN32)
    target_link_libraries(file_watcher_example PRIVATE User32 Gdi32)
endif()
