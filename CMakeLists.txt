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

# Define _DEBUG for debug builds (for ImGui and other debug-only code)
# Use generator expression to support multi-config generators (Visual Studio, Xcode)
add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)

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

# Fetch Dear ImGui (docking branch) - needed for project picker and debug UI
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.92.4-docking
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# Fetch portable-file-dialogs (single-header file dialog library)
# Use URL download to avoid CMake compatibility issues with their CMakeLists.txt
FetchContent_Declare(
        portable_file_dialogs
        URL https://github.com/samhocevar/portable-file-dialogs/archive/refs/heads/main.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(portable_file_dialogs)
if(NOT portable_file_dialogs_POPULATED)
    FetchContent_Populate(portable_file_dialogs)
endif()

# Vulkan
find_package(Vulkan REQUIRED)

# OpenGL (for ImGui project picker with OpenGL backend)
find_package(OpenGL REQUIRED)

# Fetch GLEW (OpenGL extension wrangler for ImGui project picker)
FetchContent_Declare(
        glew
        URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(glew)
if(NOT glew_POPULATED)
    FetchContent_Populate(glew)
endif()

# Create GLEW static library manually
add_library(glew_static STATIC ${glew_SOURCE_DIR}/src/glew.c)
target_include_directories(glew_static PUBLIC ${glew_SOURCE_DIR}/include)
target_link_libraries(glew_static PUBLIC OpenGL::GL)
target_compile_definitions(glew_static PUBLIC GLEW_STATIC)
if(WIN32)
    target_compile_definitions(glew_static PRIVATE GLEW_BUILD)
endif()

# Suppress warnings for GLEW (external code)
if(MSVC)
    target_compile_options(glew_static PRIVATE /W0)
else()
    target_compile_options(glew_static PRIVATE -w)
endif()

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

# ImGui is external code - we'll disable warnings for its source files after target creation
# (handled below after add_executable)

# stb is header-only, so we just need the include directory
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

# ==============================================================================
# Component Libraries
# ==============================================================================

# -------------------- Platform Library --------------------
add_library(engine_platform STATIC
        src/platform/window.cpp
        src/platform/input.cpp
)

if(WIN32)
    target_sources(engine_platform PRIVATE src/platform/win32_platform.cpp)
endif()

target_include_directories(engine_platform PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(engine_platform PUBLIC
        glm::glm
        glfw
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
        src/core/texture_data.cpp
)

target_include_directories(engine_core PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${portable_file_dialogs_SOURCE_DIR}
)

target_link_libraries(engine_core PUBLIC
        glm::glm
        nlohmann_json::nlohmann_json
        Vulkan::Vulkan
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
        src/ecs/systems/camera_system.cpp
        src/ecs/systems/camera_controller.cpp
        src/ecs/systems/editor_camera_controller.cpp
        src/ecs/systems/render_system.cpp
        src/ecs/systems/shadow_system.cpp
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

# -------------------- Renderer Library --------------------
add_library(engine_renderer STATIC
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_swapchain.cpp
        src/renderer/vulkan_renderer.cpp
        src/renderer/vulkan_mesh.cpp
        src/renderer/vulkan_texture.cpp
        src/renderer/vulkan_mipmap_compute.cpp
        src/renderer/mipmap_policy.cpp
        src/renderer/vulkan_pipeline.cpp
        src/renderer/vulkan_descriptors.cpp
        src/renderer/vulkan_descriptor_pools.cpp
        src/renderer/vulkan_render_pass.cpp
        src/renderer/vulkan_depth.cpp
        src/renderer/vulkan_framebuffer.cpp
        src/renderer/vulkan_render_target.cpp
        src/renderer/viewport.cpp
        src/renderer/viewport_manager.cpp
        src/renderer/vulkan_command_buffer.cpp
        src/renderer/vulkan_buffer.cpp
        src/renderer/vulkan_material_buffer.cpp
        src/renderer/vulkan_staging_pool.cpp
        src/renderer/vulkan_transfer_queue.cpp
        src/renderer/vulkan_shadow_map.cpp
        src/renderer/vulkan_shadow_atlas.cpp
        src/renderer/vulkan_evsm_shadow.cpp
        src/renderer/vulkan_shadow_renderer.cpp
        src/renderer/shadow_profiler.cpp
        src/renderer/vulkan_light_culling.cpp
        src/renderer/ltc_tables.cpp
        src/renderer/imgui_layer.cpp
)

target_include_directories(engine_renderer PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(engine_renderer PUBLIC
        glm::glm
        Vulkan::Vulkan
        engine_core
        engine_platform
)

target_compile_definitions(engine_renderer PUBLIC ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

target_compile_features(engine_renderer PUBLIC cxx_std_20)

# -------------------- Resources Library --------------------
add_library(engine_resources STATIC
        src/resources/mesh_manager.cpp
        src/resources/material_manager.cpp
        src/resources/material_converter.cpp
        src/resources/image_loader.cpp
        src/resources/texture_manager.cpp
)

target_include_directories(engine_resources PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
)

target_link_libraries(engine_resources PUBLIC
        glm::glm
        assimp
        nlohmann_json::nlohmann_json
        engine_core
        engine_renderer
)

target_compile_features(engine_resources PUBLIC cxx_std_20)

# -------------------- ImGui Library --------------------
# Dear ImGui source files - always needed for project picker
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(imgui_lib STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_lib PUBLIC
        glfw
        OpenGL::GL
        Vulkan::Vulkan
)

# Suppress warnings for ImGui (external code)
if(MSVC)
    target_compile_options(imgui_lib PRIVATE /W0)
else()
    target_compile_options(imgui_lib PRIVATE -w)
endif()

target_compile_features(imgui_lib PUBLIC cxx_std_20)

# -------------------- UI Library --------------------
add_library(engine_ui STATIC
        src/ui/imgui_project_picker.cpp
)

target_include_directories(engine_ui PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(engine_ui PUBLIC
        imgui_lib
        glew_static
        engine_core
)

target_compile_features(engine_ui PUBLIC cxx_std_20)

# ==============================================================================
# Main Engine Executable
# ==============================================================================

# Main executable - now just links component libraries
add_executable(engine
        src/main.cpp
        src/examples/test_scene.cpp
)

target_include_directories(engine PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# Link all component libraries
target_link_libraries(engine PRIVATE
        engine_core
        engine_ecs
        engine_renderer
        engine_resources
        engine_ui
        imgui_lib
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
    # Sanitizers for MSVC (limited support)
    if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(engine PRIVATE /fsanitize=address)
    endif()
else()
    # GCC/Clang/MinGW
    target_compile_options(engine PRIVATE -Wall -Wextra -Wpedantic -Werror)
    # Sanitizers for GCC/Clang
    if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-fsanitize=address" HAS_ASAN)
        check_cxx_compiler_flag("-fsanitize=undefined" HAS_UBSAN)
        if(HAS_ASAN)
            target_compile_options(engine PRIVATE -fsanitize=address)
            target_link_options(engine PRIVATE -fsanitize=address)
        endif()
        if(HAS_UBSAN)
            target_compile_options(engine PRIVATE -fsanitize=undefined)
            target_link_options(engine PRIVATE -fsanitize=undefined)
        endif()
    endif()
endif()

# Platform detection compile definitions
if(WIN32)
    target_compile_definitions(engine PRIVATE PLATFORM_WINDOWS)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(engine PRIVATE PLATFORM_LINUX)
elseif(APPLE)
    target_compile_definitions(engine PRIVATE PLATFORM_MACOS)
endif()

# Define _DEBUG for debug builds (for ImGui and other debug-only code)
target_compile_definitions(engine PRIVATE $<$<CONFIG:Debug>:_DEBUG>)

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
        ${stb_SOURCE_DIR}
        ${portable_file_dialogs_SOURCE_DIR}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )

    # Link component libraries
    foreach(comp ${ARG_COMPONENTS})
        target_link_libraries(${TEST_NAME} PRIVATE engine_${comp})
    endforeach()

    # Link additional libraries
    target_link_libraries(${TEST_NAME} PRIVATE ${ARG_LIBS})

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
    else()
        # Auto-detect label from components
        if("${ARG_COMPONENTS}" MATCHES "ecs")
            set_tests_properties(${TEST_NAME} PROPERTIES LABELS "ecs")
        elseif("${ARG_COMPONENTS}" MATCHES "renderer")
            set_tests_properties(${TEST_NAME} PROPERTIES LABELS "renderer")
        elseif("${ARG_COMPONENTS}" MATCHES "resources")
            set_tests_properties(${TEST_NAME} PROPERTIES LABELS "resources")
        elseif("${ARG_COMPONENTS}" MATCHES "core")
            set_tests_properties(${TEST_NAME} PROPERTIES LABELS "core")
        endif()
    endif()

    # Set timeout (default 30 seconds for unit tests, 60 for integration tests)
    if(ARG_TIMEOUT)
        set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT ${ARG_TIMEOUT})
    else()
        if("${ARG_COMPONENTS}" MATCHES "renderer")
            set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 60)  # Renderer tests may be slower
        else()
            set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 30)
        endif()
    endif()
endfunction()

# Helper function for shader compilation
function(add_spirv_shader OUTPUT_VAR SHADER_TYPE SHADER_FILE)
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    set(SHADER_HLSL ${CMAKE_SOURCE_DIR}/${SHADER_FILE})
    set(SHADER_SPV ${CMAKE_SOURCE_DIR}/${SHADER_FILE}.spv)

    # Determine shader model based on type
    if(SHADER_TYPE STREQUAL "vertex")
        set(SHADER_MODEL "vs_6_7")
    elseif(SHADER_TYPE STREQUAL "fragment" OR SHADER_TYPE STREQUAL "pixel")
        set(SHADER_MODEL "ps_6_7")
    elseif(SHADER_TYPE STREQUAL "compute")
        set(SHADER_MODEL "cs_6_7")
    else()
        message(FATAL_ERROR "Unknown shader type: ${SHADER_TYPE}")
    endif()

    # Add custom command to compile shader
    add_custom_command(
        OUTPUT ${SHADER_SPV}
        COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T ${SHADER_MODEL} -E main
                ${SHADER_HLSL} -Fo ${SHADER_SPV} -I ${CMAKE_SOURCE_DIR}/assets/shaders
        DEPENDS ${SHADER_HLSL}
        COMMENT "Compiling ${SHADER_NAME} shader to SPIR-V"
        VERBATIM
    )

    # Set output variable in parent scope
    set(${OUTPUT_VAR} ${SHADER_SPV} PARENT_SCOPE)
endfunction()

# ==============================================================================
# Tests and Examples
# ==============================================================================

add_executable(test_vulkan
        tests/test_vulkan.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_mipmap_compute.cpp
        src/renderer/vulkan_swapchain.cpp
        src/renderer/vulkan_renderer.cpp
        src/ecs/systems/camera_system.cpp
        tests/stubs/camera_controller_stub.cpp
        src/ecs/systems/render_system.cpp
        src/ecs/systems/shadow_system.cpp
        src/ecs/entity_manager.cpp
        src/resources/mesh_manager.cpp
        src/resources/material_manager.cpp
        src/resources/material_converter.cpp
        src/resources/image_loader.cpp
        src/resources/texture_manager.cpp
        src/renderer/vulkan_mesh.cpp
        src/renderer/vulkan_texture.cpp
        src/renderer/mipmap_policy.cpp
        src/renderer/vulkan_pipeline.cpp
        src/renderer/vulkan_descriptors.cpp
        src/renderer/vulkan_descriptor_pools.cpp
        src/renderer/vulkan_render_pass.cpp
        src/renderer/vulkan_depth.cpp
        src/renderer/vulkan_framebuffer.cpp
        src/renderer/vulkan_render_target.cpp
        src/renderer/viewport.cpp
        src/renderer/viewport_manager.cpp
        src/renderer/vulkan_command_buffer.cpp
        src/renderer/vulkan_buffer.cpp
        src/renderer/vulkan_staging_pool.cpp
        src/renderer/vulkan_transfer_queue.cpp
        src/renderer/vulkan_material_buffer.cpp
        src/renderer/vulkan_light_culling.cpp
        src/renderer/vulkan_shadow_map.cpp
        src/renderer/vulkan_shadow_atlas.cpp
        src/renderer/vulkan_evsm_shadow.cpp
        src/renderer/vulkan_shadow_renderer.cpp
        src/renderer/shadow_profiler.cpp
        src/renderer/imgui_layer.cpp
        src/platform/window.cpp
        src/core/time.cpp
        src/core/texture_data.cpp
        src/core/job_system.cpp
        src/core/memory.cpp
        src/core/file_watcher.cpp
        src/core/scene_manager.cpp
        src/core/engine_settings.cpp
        src/core/file_dialog.cpp
        src/ecs/hierarchy_manager.cpp
        src/ecs/ecs_coordinator.cpp
        src/ecs/systems/transform_system.cpp
        src/ecs/scene_serializer.cpp
)

target_include_directories(test_vulkan PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
        ${portable_file_dialogs_SOURCE_DIR}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(test_vulkan PRIVATE
        glfw
        glm::glm
        nlohmann_json::nlohmann_json
        Vulkan::Vulkan
        assimp
)

target_compile_definitions(test_vulkan PRIVATE ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

if(WIN32)
    target_sources(test_vulkan PRIVATE src/platform/win32_platform.cpp)
    target_link_libraries(test_vulkan PRIVATE User32 Gdi32 Ole32 Shell32)
endif()

# Add ImGui sources for test_vulkan
target_sources(test_vulkan PRIVATE
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

# Suppress warnings for ImGui in test_vulkan
if(MSVC)
    set_source_files_properties(
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
            PROPERTIES COMPILE_OPTIONS "/W0"
    )
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
set(EVSM_PREFILTER_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/evsm_prefilter.comp)
set(EVSM_PREFILTER_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/evsm_prefilter.comp.spv)
set(EVSM_BLUR_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/evsm_blur.comp)
set(EVSM_BLUR_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/evsm_blur.comp.spv)
set(LIGHT_CULLING_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/light_culling.comp)
set(LIGHT_CULLING_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/light_culling.comp.spv)
set(DEPTH_PREPASS_VERT_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/depth_prepass.vert)
set(DEPTH_PREPASS_VERT_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/depth_prepass.vert.spv)
set(DEPTH_PREPASS_FRAG_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/depth_prepass.frag)
set(DEPTH_PREPASS_FRAG_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/depth_prepass.frag.spv)
set(SHADOW_VERT_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/shadow.vert)
set(SHADOW_VERT_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/shadow.vert.spv)
set(FORWARD_PLUS_DEBUG_VERT_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/forward_plus_debug.vert)
set(FORWARD_PLUS_DEBUG_VERT_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/forward_plus_debug.vert.spv)
set(FORWARD_PLUS_DEBUG_FRAG_HLSL ${CMAKE_SOURCE_DIR}/assets/shaders/forward_plus_debug.frag)
set(FORWARD_PLUS_DEBUG_FRAG_SPV ${CMAKE_SOURCE_DIR}/assets/shaders/forward_plus_debug.frag.spv)

# Find DXC compiler - prefer Vulkan SDK version with SPIR-V support
# Check Vulkan SDK locations first (VULKAN_SDK, VK_SDK_PATH environment variables)
find_program(DXC_EXECUTABLE
    NAMES dxc dxc.exe
    PATHS
        ENV VULKAN_SDK
        ENV VK_SDK_PATH
    PATH_SUFFIXES
        Bin
        bin
    NO_DEFAULT_PATH
)

# If not found in Vulkan SDK, fall back to system PATH (but warn if SPIR-V is unsupported)
if(NOT DXC_EXECUTABLE)
    find_program(DXC_EXECUTABLE dxc)
endif()

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

    add_custom_command(
            OUTPUT ${EVSM_PREFILTER_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main
                    ${EVSM_PREFILTER_HLSL} -Fo ${EVSM_PREFILTER_SPV}
            DEPENDS ${EVSM_PREFILTER_HLSL}
            COMMENT "Compiling EVSM prefilter compute shader to SPIR-V"
            VERBATIM
    )

    add_custom_command(
            OUTPUT ${EVSM_BLUR_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main
                    ${EVSM_BLUR_HLSL} -Fo ${EVSM_BLUR_SPV}
            DEPENDS ${EVSM_BLUR_HLSL}
            COMMENT "Compiling EVSM blur compute shader to SPIR-V"
            VERBATIM
    )

    add_custom_command(
            OUTPUT ${LIGHT_CULLING_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main
                    ${LIGHT_CULLING_HLSL} -Fo ${LIGHT_CULLING_SPV}
            DEPENDS ${LIGHT_CULLING_HLSL}
            COMMENT "Compiling light culling compute shader to SPIR-V"
            VERBATIM
    )

    add_custom_command(
            OUTPUT ${DEPTH_PREPASS_VERT_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T vs_6_7 -E main
                    ${DEPTH_PREPASS_VERT_HLSL} -Fo ${DEPTH_PREPASS_VERT_SPV}
            DEPENDS ${DEPTH_PREPASS_VERT_HLSL}
            COMMENT "Compiling depth prepass vertex shader to SPIR-V"
            VERBATIM
    )

    add_custom_command(
            OUTPUT ${DEPTH_PREPASS_FRAG_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T ps_6_7 -E main
                    ${DEPTH_PREPASS_FRAG_HLSL} -Fo ${DEPTH_PREPASS_FRAG_SPV}
            DEPENDS ${DEPTH_PREPASS_FRAG_HLSL}
            COMMENT "Compiling depth prepass fragment shader to SPIR-V"
            VERBATIM
    )

    add_custom_command(
            OUTPUT ${SHADOW_VERT_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T vs_6_7 -E main
                    ${SHADOW_VERT_HLSL} -Fo ${SHADOW_VERT_SPV}
            DEPENDS ${SHADOW_VERT_HLSL}
            COMMENT "Compiling shadow vertex shader to SPIR-V"
            VERBATIM
    )

    add_custom_command(
            OUTPUT ${FORWARD_PLUS_DEBUG_VERT_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T vs_6_7 -E main
                    ${FORWARD_PLUS_DEBUG_VERT_HLSL} -Fo ${FORWARD_PLUS_DEBUG_VERT_SPV}
            DEPENDS ${FORWARD_PLUS_DEBUG_VERT_HLSL}
            COMMENT "Compiling forward_plus_debug vertex shader to SPIR-V"
            VERBATIM
    )

    add_custom_command(
            OUTPUT ${FORWARD_PLUS_DEBUG_FRAG_SPV}
            COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.2 -T ps_6_7 -E main
                    ${FORWARD_PLUS_DEBUG_FRAG_HLSL} -Fo ${FORWARD_PLUS_DEBUG_FRAG_SPV}
            DEPENDS ${FORWARD_PLUS_DEBUG_FRAG_HLSL}
            COMMENT "Compiling forward_plus_debug fragment shader to SPIR-V"
            VERBATIM
    )

    add_custom_target(Shaders ALL
            DEPENDS ${CUBE_VERT_SPV} ${CUBE_FRAG_SPV} ${MIPGEN_COLOR_SPV} ${MIPGEN_NORMAL_SPV} ${MIPGEN_ROUGHNESS_SPV} ${MIPGEN_SRGB_SPV} ${EVSM_PREFILTER_SPV} ${EVSM_BLUR_SPV} ${LIGHT_CULLING_SPV} ${DEPTH_PREPASS_VERT_SPV} ${DEPTH_PREPASS_FRAG_SPV} ${SHADOW_VERT_SPV} ${FORWARD_PLUS_DEBUG_VERT_SPV} ${FORWARD_PLUS_DEBUG_FRAG_SPV}
    )

    add_custom_command(TARGET Shaders POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/assets/shaders
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CUBE_VERT_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/cube.vert.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CUBE_FRAG_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/cube.frag.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIPGEN_COLOR_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/mipgen_color.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIPGEN_NORMAL_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/mipgen_normal.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIPGEN_ROUGHNESS_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/mipgen_roughness.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIPGEN_SRGB_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/mipgen_srgb.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EVSM_PREFILTER_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/evsm_prefilter.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EVSM_BLUR_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/evsm_blur.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIGHT_CULLING_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/light_culling.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DEPTH_PREPASS_VERT_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/depth_prepass.vert.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DEPTH_PREPASS_FRAG_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/depth_prepass.frag.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADOW_VERT_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/shadow.vert.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FORWARD_PLUS_DEBUG_VERT_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/forward_plus_debug.vert.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FORWARD_PLUS_DEBUG_FRAG_SPV} ${CMAKE_BINARY_DIR}/assets/shaders/forward_plus_debug.frag.spv
    )

    # Copy shaders to executable output directory for runtime access
    add_custom_command(TARGET Shaders POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CUBE_VERT_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/cube.vert.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CUBE_FRAG_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/cube.frag.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIPGEN_COLOR_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/mipgen_color.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIPGEN_NORMAL_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/mipgen_normal.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIPGEN_ROUGHNESS_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/mipgen_roughness.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIPGEN_SRGB_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/mipgen_srgb.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EVSM_PREFILTER_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/evsm_prefilter.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EVSM_BLUR_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/evsm_blur.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIGHT_CULLING_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/light_culling.comp.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DEPTH_PREPASS_VERT_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/depth_prepass.vert.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DEPTH_PREPASS_FRAG_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/depth_prepass.frag.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADOW_VERT_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/shadow.vert.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FORWARD_PLUS_DEBUG_VERT_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/forward_plus_debug.vert.spv
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FORWARD_PLUS_DEBUG_FRAG_SPV} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/forward_plus_debug.frag.spv
            COMMENT "Copying shaders to executable output directory"
    )

    add_dependencies(engine Shaders)
    add_dependencies(test_vulkan Shaders)
else()
    message(WARNING "DXC not found; run compile_shaders.sh or compile_shaders.bat manually to generate SPIR-V binaries.")
endif()

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
    SOURCES tests/stubs/camera_controller_stub.cpp
)

# Test executable for ECS queries
add_engine_test(test_ecs_queries
    COMPONENTS ecs
    SOURCES tests/stubs/camera_controller_stub.cpp
)

# Test executable for renderable components
add_engine_test(test_renderable
    COMPONENTS ecs
    SOURCES tests/stubs/camera_controller_stub.cpp
)

# Test executable for render system
add_engine_test(test_render_system
    COMPONENTS ecs renderer resources
    SOURCES tests/stubs/camera_controller_stub.cpp
)

# Test executable for camera system
add_engine_test(test_camera
    COMPONENTS ecs
    SOURCES tests/stubs/camera_controller_stub.cpp
)

# Test executable for ECS deferred operations
add_engine_test(test_ecs_deferred
    COMPONENTS ecs
    SOURCES tests/stubs/camera_controller_stub.cpp
)

# Test executable for ECS versioning
add_engine_test(test_ecs_versions
    COMPONENTS ecs
    SOURCES tests/stubs/camera_controller_stub.cpp
)

# Test executable for ECS signatures
add_engine_test(test_ecs_signatures
    COMPONENTS ecs
    SOURCES tests/stubs/camera_controller_stub.cpp
)

# Test executable for ECS parallel operations
add_engine_test(test_ecs_parallel
    COMPONENTS ecs core platform
    SOURCES tests/stubs/camera_controller_stub.cpp
)

# Test executable for hierarchy manager
add_engine_test(test_hierarchy_manager
    SOURCES src/ecs/hierarchy_manager.cpp src/ecs/entity_manager.cpp
)

# Test executable for scene serializer
add_engine_test(test_scene_serializer
    COMPONENTS ecs renderer resources platform
    SOURCES tests/stubs/camera_controller_stub.cpp
)

# Test executable for time system
add_engine_test(test_time
    COMPONENTS core
    SOURCES src/core/time.cpp
)

# Test executable for resource handle system
add_engine_test(test_resource_handle
    COMPONENTS renderer resources platform
)

# Example executable for resource handle system
add_executable(resource_handle_example
        examples/resource_handle_example.cpp
)

target_include_directories(resource_handle_example PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(resource_handle_example PRIVATE
        engine_renderer
        engine_resources
        engine_platform
)

# Test executable for file watcher system
add_engine_test(test_file_watcher
    SOURCES src/core/file_watcher.cpp
)

# Example executable for file watcher system
add_executable(file_watcher_example
        examples/file_watcher_example.cpp
)

target_include_directories(file_watcher_example PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(file_watcher_example PRIVATE
        engine_core
        engine_platform
)

# Example executable for Vulkan buffer usage
add_executable(vulkan_buffer_example
        examples/vulkan_buffer_example.cpp
)

target_include_directories(vulkan_buffer_example PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(vulkan_buffer_example PRIVATE
        engine_renderer
        engine_platform
)

# Test executable for Vulkan buffer abstraction
add_engine_test(test_vulkan_buffer
    COMPONENTS renderer platform
)

# Test executable for texture loading
add_engine_test(test_texture_loading
    SOURCES src/core/texture_data.cpp src/resources/image_loader.cpp
)

# Test executable for texture manager
add_engine_test(test_texture_manager
    COMPONENTS renderer resources platform
)

# Test executable for async texture loading
add_engine_test(test_texture_async
    COMPONENTS renderer resources platform
)

# Test executable for texture array data validation
add_engine_test(test_texture_array_data
    SOURCES src/core/texture_data.cpp
    LIBS Vulkan::Vulkan
)

# Test executable for image loader array functions
add_engine_test(test_image_loader_array
    SOURCES src/resources/image_loader.cpp src/core/texture_data.cpp
    LIBS Vulkan::Vulkan
)

# Test executable for Vulkan texture array creation
add_engine_test(test_vulkan_texture_array
    COMPONENTS renderer platform
)

# Test executable for array texture mipmap generation
add_engine_test(test_mipmap_array
    COMPONENTS renderer platform
)

# Test executable for async array texture loading
add_engine_test(test_texture_array_async
    COMPONENTS renderer resources platform
)

# Test executable for texture descriptor binding
add_engine_test(test_texture_descriptors
    COMPONENTS renderer platform
)

# Test executable for sampler settings
add_engine_test(test_sampler_settings
    SOURCES src/core/texture_data.cpp
    LIBS Vulkan::Vulkan
)

# Test executable for cubemap loading and rendering
add_engine_test(test_cubemap
    COMPONENTS renderer resources platform
)

# Test executable for format support detection
add_engine_test(test_format_support
    COMPONENTS renderer platform
)

# Test executable for mipmap policy
add_engine_test(test_mipmap_policy
    COMPONENTS renderer platform
    SOURCES src/core/texture_data.cpp
)

# Test executable for mipgen compute
add_engine_test(test_mipgen_compute
    COMPONENTS renderer platform
)

# Test executable for mesh loading
add_engine_test(test_mesh_loading
    COMPONENTS renderer resources platform
)

# Test executable for material system
add_engine_test(test_material
    LIBS Vulkan::Vulkan
)
