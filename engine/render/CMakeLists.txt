# engine/render/CMakeLists.txt
# engine::render - bgfx wrapper, renderer abstraction

# Include bgfx shader compilation utilities
include(${bgfx_cmake_SOURCE_DIR}/cmake/bgfxToolUtils.cmake)

add_library(engine_render STATIC)
add_library(engine::render ALIAS engine_render)

target_sources(engine_render
    PRIVATE
        src/device.cpp
        src/bgfx_renderer.cpp
        src/debug_draw.cpp
        src/shadow_system.cpp
        src/ssao.cpp
        src/post_process.cpp
        src/skeleton.cpp
        src/animation.cpp
        src/volumetric.cpp
        src/render_pipeline.cpp
        src/particle_system.cpp
        src/animation_graph.cpp
        src/blend_tree.cpp
        src/animation_state_machine.cpp
        src/root_motion.cpp
        src/blend_shapes.cpp
        src/ik.cpp
        src/lod.cpp
        src/decal_system.cpp
        src/ssr.cpp
        src/motion_blur.cpp
        src/dof.cpp
        src/light_probes.cpp
        src/reflection_probes.cpp
        src/instancing.cpp
        src/occlusion_culling.cpp
        src/camera_effects.cpp
        src/animation_events.cpp
        src/render_systems.cpp
        src/billboard.cpp
        src/render_to_texture.cpp
        src/material_instance.cpp
        src/oit.cpp
        src/render_component_registration.cpp
        src/water_renderer.cpp
        src/tp_camera.cpp
        src/bone_mask_presets.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/render/device.hpp
            include/engine/render/render.hpp
            include/engine/render/renderer.hpp
            include/engine/render/render_target.hpp
            include/engine/render/pbr_material.hpp
            include/engine/render/shadow_system.hpp
            include/engine/render/ssao.hpp
            include/engine/render/post_process.hpp
            include/engine/render/skeleton.hpp
            include/engine/render/animation.hpp
            include/engine/render/animation_graph.hpp
            include/engine/render/blend_tree.hpp
            include/engine/render/animation_state_machine.hpp
            include/engine/render/root_motion.hpp
            include/engine/render/blend_shapes.hpp
            include/engine/render/ik.hpp
            include/engine/render/lod.hpp
            include/engine/render/volumetric.hpp
            include/engine/render/render_pipeline.hpp
            include/engine/render/types.hpp
            include/engine/render/debug_draw.hpp
            include/engine/render/particle_system.hpp
            include/engine/render/decal_system.hpp
            include/engine/render/ssr.hpp
            include/engine/render/motion_blur.hpp
            include/engine/render/dof.hpp
            include/engine/render/light_probes.hpp
            include/engine/render/reflection_probes.hpp
            include/engine/render/instancing.hpp
            include/engine/render/occlusion_culling.hpp
            include/engine/render/camera_effects.hpp
            include/engine/render/animation_events.hpp
            include/engine/render/render_systems.hpp
            include/engine/render/billboard.hpp
            include/engine/render/render_to_texture.hpp
            include/engine/render/material_instance.hpp
            include/engine/render/oit.hpp
            include/engine/render/water_renderer.hpp
            include/engine/render/tp_camera.hpp
            include/engine/render/bone_mask_presets.hpp
)

target_include_directories(engine_render
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_render
    PUBLIC
        engine::core
        engine::scene
        bgfx::bgfx
        bgfx::bimg
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_render PROPERTIES
    FOLDER "engine"
)

# Option to skip shader compilation (for debugging C++ build issues)
option(ENGINE_COMPILE_SHADERS "Compile bgfx shaders at build time" ON)

# Compile shaders at build time
if(TARGET bgfx::shaderc AND ENGINE_COMPILE_SHADERS)
    set(BGFX_SHADER_INCLUDE_PATH "${bgfx_cmake_SOURCE_DIR}/bgfx/src")
    set(SHADER_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/common")
    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    set(RUNTIME_SHADER_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")

    # Default shaders (simple)
    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_default.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR VS_DEFAULT_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_default.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_DEFAULT_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # PBR shaders
    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_pbr.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_pbr.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR VS_PBR_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH} ${SHADER_COMMON_DIR}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_pbr.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_pbr.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_PBR_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH} ${SHADER_COMMON_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    )

    # Shadow shaders
    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_shadow.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_shadow.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR VS_SHADOW_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_shadow.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_shadow.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_SHADOW_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # Fullscreen vertex shader (used by all post-processing)
    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_fullscreen.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_fullscreen.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR VS_FULLSCREEN_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # SSAO shaders
    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_ssao.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_fullscreen.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_SSAO_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_ssao_blur.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_fullscreen.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_SSAO_BLUR_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # Bloom shaders
    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_bloom_downsample.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_fullscreen.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_BLOOM_DOWN_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_bloom_upsample.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_fullscreen.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_BLOOM_UP_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # Tonemapping shader
    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_tonemap.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_fullscreen.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_TONEMAP_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # Skinned PBR shaders (for skeletal animation)
    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_skinned_pbr.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_skinned_pbr.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR VS_SKINNED_PBR_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH} ${SHADER_COMMON_DIR}
    )

    # Volumetric fog shader
    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_volumetric.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_fullscreen.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_VOLUMETRIC_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # Debug line shaders
    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_debug.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_debug.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR VS_DEBUG_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_debug.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_debug.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_DEBUG_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # Skybox shaders
    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_skybox.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_skybox.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR VS_SKYBOX_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_skybox.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_skybox.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_SKYBOX_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # Billboard shaders
    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_billboard.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_billboard.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR VS_BILLBOARD_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_billboard.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_billboard.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_BILLBOARD_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # Blit (fullscreen texture copy) shaders
    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_blit.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_blit.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR VS_BLIT_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_blit.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_blit.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_BLIT_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # OIT shaders
    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_oit_accumulate.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_fullscreen.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_OIT_ACCUM_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_oit_composite.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_fullscreen.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_OIT_COMPOSITE_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # SSR shaders
    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_ssr_hiz.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_ssr.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_SSR_HIZ_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_ssr_trace.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_ssr.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_SSR_TRACE_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_ssr_resolve.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_ssr.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_SSR_RESOLVE_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_ssr_composite.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying_ssr.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        OUT_FILES_VAR FS_SSR_COMPOSITE_OUTPUTS
        INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_PATH}
    )

    # Create custom target for shaders
    add_custom_target(engine_shaders DEPENDS
        ${VS_DEFAULT_OUTPUTS} ${FS_DEFAULT_OUTPUTS}
        ${VS_PBR_OUTPUTS} ${FS_PBR_OUTPUTS}
        ${VS_SHADOW_OUTPUTS} ${FS_SHADOW_OUTPUTS}
        ${VS_FULLSCREEN_OUTPUTS}
        ${FS_SSAO_OUTPUTS} ${FS_SSAO_BLUR_OUTPUTS}
        ${FS_BLOOM_DOWN_OUTPUTS} ${FS_BLOOM_UP_OUTPUTS}
        ${FS_TONEMAP_OUTPUTS}
        ${VS_SKINNED_PBR_OUTPUTS}
        ${FS_VOLUMETRIC_OUTPUTS}
        ${VS_DEBUG_OUTPUTS} ${FS_DEBUG_OUTPUTS}
        ${VS_SKYBOX_OUTPUTS} ${FS_SKYBOX_OUTPUTS}
        ${VS_BILLBOARD_OUTPUTS} ${FS_BILLBOARD_OUTPUTS}
        ${VS_BLIT_OUTPUTS} ${FS_BLIT_OUTPUTS}
        ${FS_OIT_ACCUM_OUTPUTS} ${FS_OIT_COMPOSITE_OUTPUTS}
        ${FS_SSR_HIZ_OUTPUTS} ${FS_SSR_TRACE_OUTPUTS}
        ${FS_SSR_RESOLVE_OUTPUTS} ${FS_SSR_COMPOSITE_OUTPUTS}
    )

    # Copy compiled shaders next to the runtime binaries so the renderer can find them
    add_custom_command(TARGET engine_shaders POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RUNTIME_SHADER_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_OUTPUT_DIR} ${RUNTIME_SHADER_DIR}
        COMMENT "Copying BGFX shaders to ${RUNTIME_SHADER_DIR}"
    )

    add_dependencies(engine_render engine_shaders)
endif()

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()
