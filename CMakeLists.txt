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

# Fetch stb (for stb_image)
FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

# Fetch Assimp (for mesh loading)
FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG        v5.3.1
        GIT_SHALLOW    TRUE
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

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

# Assimp is external code - disable warnings-as-errors for it
if(TARGET assimp)
    if(MSVC)
        target_compile_options(assimp PRIVATE /W0)
    else()
        target_compile_options(assimp PRIVATE -w)
    endif()
endif()

# stb is header-only, so we just need the include directory
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

# Source files - we'll add these as we create them
set(ENGINE_SOURCES
        src/main.cpp
        src/examples/test_scene.cpp
        src/core/memory.cpp
        src/core/job_system.cpp
        src/core/time.cpp
        src/core/file_watcher.cpp
        src/core/texture_data.cpp
        src/ecs/ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/scene_serializer.cpp
        src/ecs/systems/transform_system.cpp
        src/ecs/systems/camera_system.cpp
        src/ecs/systems/camera_controller.cpp
        src/ecs/systems/render_system.cpp
        src/platform/window.cpp
        src/platform/input.cpp
        src/resources/mesh_manager.cpp
        src/resources/material_manager.cpp
        src/resources/image_loader.cpp
        src/resources/texture_manager.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_swapchain.cpp
        src/renderer/vulkan_renderer.cpp
        src/renderer/vulkan_mesh.cpp
        src/renderer/vulkan_texture.cpp
        src/renderer/vulkan_mipmap_compute.cpp
        src/renderer/mipmap_policy.cpp
        src/renderer/vulkan_pipeline.cpp
        src/renderer/vulkan_descriptors.cpp
        src/renderer/vulkan_render_pass.cpp
        src/renderer/vulkan_depth.cpp
        src/renderer/vulkan_framebuffer.cpp
        src/renderer/vulkan_command_buffer.cpp
        src/renderer/vulkan_buffer.cpp
        src/renderer/vulkan_material_buffer.cpp
        src/renderer/vulkan_staging_pool.cpp
        src/renderer/vulkan_transfer_queue.cpp
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
        ${stb_SOURCE_DIR}
)

# Link GLM, JSON, GLFW, and Assimp
target_link_libraries(engine PRIVATE
        glm::glm
        nlohmann_json::nlohmann_json
        glfw
        assimp
        Vulkan::Vulkan
)

target_compile_definitions(engine PRIVATE ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

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
        src/renderer/vulkan_mipmap_compute.cpp
        src/renderer/vulkan_swapchain.cpp
        src/renderer/vulkan_renderer.cpp
        src/ecs/systems/camera_system.cpp
        tests/stubs/camera_controller_stub.cpp
        src/ecs/systems/render_system.cpp
        src/ecs/entity_manager.cpp
        src/resources/mesh_manager.cpp
        src/renderer/vulkan_mesh.cpp
        src/renderer/vulkan_pipeline.cpp
        src/renderer/vulkan_descriptors.cpp
        src/renderer/vulkan_render_pass.cpp
        src/renderer/vulkan_depth.cpp
        src/renderer/vulkan_framebuffer.cpp
        src/renderer/vulkan_command_buffer.cpp
        src/renderer/vulkan_buffer.cpp
        src/platform/window.cpp
        src/resources/mesh_manager.cpp
        src/core/time.cpp
)

target_include_directories(test_vulkan PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_vulkan PRIVATE
        glfw
        Vulkan::Vulkan
)

target_compile_definitions(test_vulkan PRIVATE ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

if(WIN32)
    target_sources(test_vulkan PRIVATE src/platform/win32_platform.cpp)
    target_link_libraries(test_vulkan PRIVATE User32 Gdi32)
endif()

add_test(NAME VulkanTests COMMAND test_vulkan)

# Shader compilation helper (uses dxc when available)
set(CUBE_VERT_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/cube.vert)
set(CUBE_FRAG_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/cube.frag)
set(CUBE_VERT_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/cube.vert.spv)
set(CUBE_FRAG_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/cube.frag.spv)
set(MIPGEN_COMMON_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/mipgen_common.hlsl)
set(MIPGEN_COLOR_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/mipgen_color.comp)
set(MIPGEN_COLOR_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/mipgen_color.comp.spv)
set(MIPGEN_NORMAL_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/mipgen_normal.comp)
set(MIPGEN_NORMAL_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/mipgen_normal.comp.spv)
set(MIPGEN_ROUGHNESS_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/mipgen_roughness.comp)
set(MIPGEN_ROUGHNESS_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/mipgen_roughness.comp.spv)
set(MIPGEN_SRGB_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/mipgen_srgb.comp)
set(MIPGEN_SRGB_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/mipgen_srgb.comp.spv)

find_program(DXC_EXECUTABLE dxc)

if(DXC_EXECUTABLE)
    add_custom_command(
            OUTPUT ${CUBE_VERT_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T vs_6_7 -E main
                    ${CUBE_VERT_HLSL} -Fo ${CUBE_VERT_SPV}
            DEPENDS ${CUBE_VERT_HLSL}
            COMMENT "Compiling cube vertex shader to SPIR-V"
            VERBATIM
    )

    add_custom_command(
            OUTPUT ${CUBE_FRAG_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T ps_6_7 -E main
                    ${CUBE_FRAG_HLSL} -Fo ${CUBE_FRAG_SPV}
            DEPENDS ${CUBE_FRAG_HLSL}
            COMMENT "Compiling cube fragment shader to SPIR-V"
            VERBATIM
    )

    add_custom_command(
            OUTPUT ${MIPGEN_COLOR_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main
                    ${MIPGEN_COLOR_HLSL} -Fo ${MIPGEN_COLOR_SPV} -I ${CMAKE_SOURCE_DIR}/assets/shaders
            DEPENDS ${MIPGEN_COLOR_HLSL} ${MIPGEN_COMMON_HLSL}
            COMMENT "Compiling mipgen_color compute shader to SPIR-V"
            VERBATIM
    )

    add_custom_command(
            OUTPUT ${MIPGEN_NORMAL_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main
                    ${MIPGEN_NORMAL_HLSL} -Fo ${MIPGEN_NORMAL_SPV} -I ${CMAKE_SOURCE_DIR}/assets/shaders
            DEPENDS ${MIPGEN_NORMAL_HLSL} ${MIPGEN_COMMON_HLSL}
            COMMENT "Compiling mipgen_normal compute shader to SPIR-V"
            VERBATIM
    )

    add_custom_command(
            OUTPUT ${MIPGEN_ROUGHNESS_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main
                    ${MIPGEN_ROUGHNESS_HLSL} -Fo ${MIPGEN_ROUGHNESS_SPV} -I ${CMAKE_SOURCE_DIR}/assets/shaders
            DEPENDS ${MIPGEN_ROUGHNESS_HLSL} ${MIPGEN_COMMON_HLSL}
            COMMENT "Compiling mipgen_roughness compute shader to SPIR-V"
            VERBATIM
    )

    add_custom_command(
            OUTPUT ${MIPGEN_SRGB_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main
                    ${MIPGEN_SRGB_HLSL} -Fo ${MIPGEN_SRGB_SPV} -I ${CMAKE_SOURCE_DIR}/assets/shaders
            DEPENDS ${MIPGEN_SRGB_HLSL} ${MIPGEN_COMMON_HLSL}
            COMMENT "Compiling mipgen_srgb compute shader to SPIR-V"
            VERBATIM
    )

    add_custom_target(Shaders ALL
            DEPENDS ${CUBE_VERT_SPV} ${CUBE_FRAG_SPV} ${MIPGEN_COLOR_SPV} ${MIPGEN_NORMAL_SPV} ${MIPGEN_ROUGHNESS_SPV} ${MIPGEN_SRGB_SPV}
    )

    add_custom_command(TARGET Shaders POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/assets/shaders
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CUBE_VERT_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/cube.vert.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CUBE_FRAG_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/cube.frag.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIPGEN_COLOR_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/mipgen_color.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIPGEN_NORMAL_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/mipgen_normal.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIPGEN_ROUGHNESS_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/mipgen_roughness.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIPGEN_SRGB_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/mipgen_srgb.comp.spv
    )

    add_dependencies(engine Shaders)
    add_dependencies(test_vulkan Shaders)
else()
    message(WARNING "DXC not found; run compile_shaders.sh or compile_shaders.bat manually to generate SPIR-V binaries.")
endif()

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

# Test executable for left-handed math validation
add_executable(test_left_handed_math
        tests/test_left_handed_math.cpp
)

target_include_directories(test_left_handed_math PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_left_handed_math PRIVATE
        glm::glm
)

add_test(NAME LeftHandedMathTests COMMAND test_left_handed_math)

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
        src/ecs/ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/systems/transform_system.cpp
        src/ecs/systems/camera_system.cpp
        tests/stubs/camera_controller_stub.cpp
)

target_include_directories(test_ecs_coordinator PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_ecs_coordinator PRIVATE
        glm::glm
)

# Register ECS coordinator test with CTest
add_test(NAME ECSCoordinatorTests COMMAND test_ecs_coordinator)

add_executable(test_ecs_queries
        tests/test_ecs_queries.cpp
        src/ecs/ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/systems/transform_system.cpp
        src/ecs/systems/camera_system.cpp
        tests/stubs/camera_controller_stub.cpp
)

target_include_directories(test_ecs_queries PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_ecs_queries PRIVATE
        glm::glm
)

add_test(NAME ECSQueryTests COMMAND test_ecs_queries)

add_executable(test_renderable
        tests/test_renderable.cpp
        src/ecs/ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/systems/transform_system.cpp
        src/ecs/systems/camera_system.cpp
        tests/stubs/camera_controller_stub.cpp
)

target_include_directories(test_renderable PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_renderable PRIVATE
        glm::glm
)

add_test(NAME RenderableTests COMMAND test_renderable)

add_executable(test_render_system
        tests/test_render_system.cpp
        src/ecs/ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/systems/transform_system.cpp
        src/ecs/systems/camera_system.cpp
        tests/stubs/camera_controller_stub.cpp
        src/ecs/systems/render_system.cpp
        src/resources/mesh_manager.cpp
        src/renderer/vulkan_mesh.cpp
        src/renderer/vulkan_buffer.cpp
)

target_include_directories(test_render_system PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_render_system PRIVATE
        glm::glm
        Vulkan::Vulkan
)

add_test(NAME RenderSystemTests COMMAND test_render_system)

add_executable(test_camera
        tests/test_camera.cpp
        src/ecs/ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/systems/transform_system.cpp
        src/ecs/systems/camera_system.cpp
        tests/stubs/camera_controller_stub.cpp
)

target_include_directories(test_camera PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_camera PRIVATE
        glm::glm
)

add_test(NAME CameraSystemTests COMMAND test_camera)

add_executable(test_ecs_deferred
        tests/test_ecs_deferred.cpp
        src/ecs/ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/systems/transform_system.cpp
        src/ecs/systems/camera_system.cpp
        tests/stubs/camera_controller_stub.cpp
)

target_include_directories(test_ecs_deferred PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_ecs_deferred PRIVATE
        glm::glm
)

add_test(NAME ECSDeferredTests COMMAND test_ecs_deferred)

add_executable(test_ecs_versions
        tests/test_ecs_versions.cpp
        src/ecs/ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/systems/transform_system.cpp
        src/ecs/systems/camera_system.cpp
        tests/stubs/camera_controller_stub.cpp
)

target_include_directories(test_ecs_versions PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_ecs_versions PRIVATE
        glm::glm
)

add_test(NAME ECSVersionTests COMMAND test_ecs_versions)

add_executable(test_ecs_signatures
        tests/test_ecs_signatures.cpp
        src/ecs/ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/systems/transform_system.cpp
        src/ecs/systems/camera_system.cpp
        tests/stubs/camera_controller_stub.cpp
)

target_include_directories(test_ecs_signatures PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_ecs_signatures PRIVATE
        glm::glm
)

add_test(NAME ECSSignatureTests COMMAND test_ecs_signatures)

add_executable(test_ecs_parallel
        tests/test_ecs_parallel.cpp
        src/ecs/ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/systems/transform_system.cpp
        src/ecs/systems/camera_system.cpp
        tests/stubs/camera_controller_stub.cpp
        src/core/job_system.cpp
        src/core/memory.cpp
)

if(WIN32)
    target_sources(test_ecs_parallel PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(test_ecs_parallel PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_ecs_parallel PRIVATE
        glm::glm
)

add_test(NAME ECSParallelTests COMMAND test_ecs_parallel)

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
        src/ecs/ecs_coordinator.cpp
        src/ecs/entity_manager.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/scene_serializer.cpp
        src/ecs/systems/transform_system.cpp
        src/ecs/systems/camera_system.cpp
        tests/stubs/camera_controller_stub.cpp
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
        src/renderer/vulkan_mesh.cpp
        src/renderer/vulkan_buffer.cpp
)

target_include_directories(test_resource_handle PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_resource_handle PRIVATE
        glm::glm
        Vulkan::Vulkan
)

# Register resource handle test with CTest
add_test(NAME ResourceHandleTests COMMAND test_resource_handle)

# Example executable for resource handle system
add_executable(resource_handle_example
        examples/resource_handle_example.cpp
        src/resources/mesh_manager.cpp
        src/renderer/vulkan_mesh.cpp
        src/renderer/vulkan_buffer.cpp
)

target_include_directories(resource_handle_example PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(resource_handle_example PRIVATE
        glm::glm
        Vulkan::Vulkan
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

# Example executable for Vulkan buffer usage
add_executable(vulkan_buffer_example
        examples/vulkan_buffer_example.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_mipmap_compute.cpp
        src/renderer/vulkan_buffer.cpp
        src/platform/window.cpp
)

if(WIN32)
    target_sources(vulkan_buffer_example PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(vulkan_buffer_example PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(vulkan_buffer_example PRIVATE
        glfw
        Vulkan::Vulkan
)

if(WIN32)
    target_link_libraries(vulkan_buffer_example PRIVATE User32 Gdi32)
endif()

# Test executable for Vulkan buffer abstraction
add_executable(test_vulkan_buffer
        tests/test_vulkan_buffer.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_mipmap_compute.cpp
        src/renderer/vulkan_buffer.cpp
        src/platform/window.cpp
)

if(WIN32)
    target_sources(test_vulkan_buffer PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(test_vulkan_buffer PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_vulkan_buffer PRIVATE
        glfw
        Vulkan::Vulkan
)

if(WIN32)
    target_link_libraries(test_vulkan_buffer PRIVATE User32 Gdi32)
endif()

add_test(NAME VulkanBufferTests COMMAND test_vulkan_buffer)

# Test executable for texture loading
add_executable(test_texture_loading
        tests/test_texture_loading.cpp
        src/core/texture_data.cpp
        src/resources/image_loader.cpp
)

target_include_directories(test_texture_loading PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
)

target_link_libraries(test_texture_loading PRIVATE
        glm::glm
        Vulkan::Vulkan
)

add_test(NAME TextureLoadingTests COMMAND test_texture_loading)

# Test executable for texture manager
add_executable(test_texture_manager
        tests/test_texture_manager.cpp
        src/core/texture_data.cpp
        src/resources/image_loader.cpp
        src/resources/texture_manager.cpp
)

target_include_directories(test_texture_manager PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
)

target_link_libraries(test_texture_manager PRIVATE
        glm::glm
        Vulkan::Vulkan
)

add_test(NAME TextureManagerTests COMMAND test_texture_manager)

# Test executable for async texture loading
add_executable(test_texture_async
        tests/test_texture_async.cpp
        src/core/texture_data.cpp
        src/core/job_system.cpp
        src/core/memory.cpp
        src/resources/image_loader.cpp
        src/resources/texture_manager.cpp
        src/platform/win32_platform.cpp
)

target_include_directories(test_texture_async PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
)

target_link_libraries(test_texture_async PRIVATE
        glm::glm
        Vulkan::Vulkan
)

add_test(NAME TextureAsyncTests COMMAND test_texture_async)

# Test executable for texture array data validation
add_executable(test_texture_array_data
        tests/test_texture_array_data.cpp
        src/core/texture_data.cpp
)

target_include_directories(test_texture_array_data PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_texture_array_data PRIVATE
        Vulkan::Vulkan
)

add_test(NAME TextureArrayDataTests COMMAND test_texture_array_data)

# Test executable for image loader array functions
add_executable(test_image_loader_array
        tests/test_image_loader_array.cpp
        src/resources/image_loader.cpp
        src/core/texture_data.cpp
)

target_include_directories(test_image_loader_array PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
)

target_link_libraries(test_image_loader_array PRIVATE
        Vulkan::Vulkan
)

add_test(NAME ImageLoaderArrayTests COMMAND test_image_loader_array)

# Test executable for Vulkan texture array creation
add_executable(test_vulkan_texture_array
        tests/test_vulkan_texture_array.cpp
        src/renderer/vulkan_texture.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_mipmap_compute.cpp
        src/renderer/vulkan_buffer.cpp
        src/renderer/mipmap_policy.cpp
        src/core/texture_data.cpp
        src/platform/window.cpp
)

if(WIN32)
    target_sources(test_vulkan_texture_array PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(test_vulkan_texture_array PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_vulkan_texture_array PRIVATE
        glfw
        Vulkan::Vulkan
)

if(WIN32)
    target_link_libraries(test_vulkan_texture_array PRIVATE User32 Gdi32)
endif()

add_test(NAME VulkanTextureArrayTests COMMAND test_vulkan_texture_array)

# Test executable for array texture mipmap generation
add_executable(test_mipmap_array
        tests/test_mipmap_array.cpp
        src/renderer/vulkan_texture.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_mipmap_compute.cpp
        src/renderer/vulkan_buffer.cpp
        src/renderer/mipmap_policy.cpp
        src/core/texture_data.cpp
        src/platform/window.cpp
)

if(WIN32)
    target_sources(test_mipmap_array PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(test_mipmap_array PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_mipmap_array PRIVATE
        glfw
        Vulkan::Vulkan
)

if(WIN32)
    target_link_libraries(test_mipmap_array PRIVATE User32 Gdi32)
endif()

add_test(NAME MipmapArrayTests COMMAND test_mipmap_array)

# Test executable for async array texture loading
add_executable(test_texture_array_async
        tests/test_texture_array_async.cpp
        src/core/texture_data.cpp
        src/core/job_system.cpp
        src/core/memory.cpp
        src/resources/image_loader.cpp
        src/resources/texture_manager.cpp
        src/platform/win32_platform.cpp
)

target_include_directories(test_texture_array_async PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
)

target_link_libraries(test_texture_array_async PRIVATE
        glm::glm
        Vulkan::Vulkan
)

add_test(NAME TextureArrayAsyncTests COMMAND test_texture_array_async)

# Test executable for texture descriptor binding
add_executable(test_texture_descriptors
        tests/test_texture_descriptors.cpp
        src/renderer/vulkan_descriptors.cpp
        src/renderer/vulkan_texture.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_mipmap_compute.cpp
        src/renderer/vulkan_buffer.cpp
        src/renderer/mipmap_policy.cpp
        src/core/texture_data.cpp
        src/platform/window.cpp
)

if(WIN32)
    target_sources(test_texture_descriptors PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(test_texture_descriptors PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_texture_descriptors PRIVATE
        glfw
        Vulkan::Vulkan
)

if(WIN32)
    target_link_libraries(test_texture_descriptors PRIVATE User32 Gdi32)
endif()

add_test(NAME TextureDescriptorTests COMMAND test_texture_descriptors)

# Test executable for sampler settings
add_executable(test_sampler_settings
        tests/test_sampler_settings.cpp
        src/core/texture_data.cpp
)

target_include_directories(test_sampler_settings PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sampler_settings PRIVATE
        Vulkan::Vulkan
)

add_test(NAME SamplerSettingsTests COMMAND test_sampler_settings)

# Test executable for cubemap loading and rendering
add_executable(test_cubemap
        tests/test_cubemap.cpp
        src/renderer/vulkan_texture.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_mipmap_compute.cpp
        src/renderer/vulkan_buffer.cpp
        src/renderer/mipmap_policy.cpp
        src/core/texture_data.cpp
        src/resources/image_loader.cpp
        src/resources/texture_manager.cpp
        src/core/job_system.cpp
        src/core/memory.cpp
        src/platform/window.cpp
)

if(WIN32)
    target_sources(test_cubemap PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(test_cubemap PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
)

target_link_libraries(test_cubemap PRIVATE
        glfw
        glm::glm
        Vulkan::Vulkan
)

if(WIN32)
    target_link_libraries(test_cubemap PRIVATE User32 Gdi32)
endif()

add_test(NAME CubemapTests COMMAND test_cubemap)

# Test executable for format support detection
add_executable(test_format_support
        tests/test_format_support.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_mipmap_compute.cpp
        src/platform/window.cpp
)

if(WIN32)
    target_sources(test_format_support PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(test_format_support PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_format_support PRIVATE
        glfw
        Vulkan::Vulkan
)

if(WIN32)
    target_link_libraries(test_format_support PRIVATE User32 Gdi32)
endif()

add_test(NAME FormatSupportTests COMMAND test_format_support)

add_executable(test_mipmap_policy
        tests/test_mipmap_policy.cpp
        src/renderer/mipmap_policy.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_mipmap_compute.cpp
        src/renderer/vulkan_buffer.cpp
        src/core/texture_data.cpp
        src/platform/window.cpp
)

if(WIN32)
    target_sources(test_mipmap_policy PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(test_mipmap_policy PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_mipmap_policy PRIVATE
        glfw
        Vulkan::Vulkan
)

if(WIN32)
    target_link_libraries(test_mipmap_policy PRIVATE User32 Gdi32)
endif()

add_test(NAME MipmapPolicyTests COMMAND test_mipmap_policy)

add_executable(test_mipgen_compute
        tests/test_mipgen_compute.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_mipmap_compute.cpp
        src/renderer/vulkan_buffer.cpp
        src/platform/window.cpp
)

if(WIN32)
    target_sources(test_mipgen_compute PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(test_mipgen_compute PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_mipgen_compute PRIVATE
        glfw
        Vulkan::Vulkan
)

target_compile_definitions(test_mipgen_compute PRIVATE ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_test(NAME MipgenComputeTests COMMAND test_mipgen_compute)

# Test executable for mesh loading
add_executable(test_mesh_loading
        tests/test_mesh_loading.cpp
        src/resources/mesh_manager.cpp
        src/renderer/vulkan_mesh.cpp
        src/renderer/vulkan_buffer.cpp
)

if(WIN32)
    target_sources(test_mesh_loading PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(test_mesh_loading PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_mesh_loading PRIVATE
        glm::glm
        assimp
        Vulkan::Vulkan
)

if(WIN32)
    target_link_libraries(test_mesh_loading PRIVATE User32 Gdi32)
endif()

target_compile_definitions(test_mesh_loading PRIVATE ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_test(NAME MeshLoadingTests COMMAND test_mesh_loading)

# Material System Test (core data structures only, no Vulkan required)
add_executable(test_material
        tests/test_material.cpp
)

target_include_directories(test_material PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_material PRIVATE
        glm::glm
)

add_test(NAME MaterialSystemTests COMMAND test_material)
