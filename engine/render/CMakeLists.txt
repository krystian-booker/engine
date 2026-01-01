# engine/render/CMakeLists.txt
# engine::render - bgfx wrapper, renderer abstraction

# Include bgfx shader compilation utilities
include(${CMAKE_SOURCE_DIR}/external/bgfx.cmake/cmake/bgfxToolUtils.cmake)

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
)

target_include_directories(engine_render
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_render
    PUBLIC
        engine::core
        bgfx::bgfx
        bgfx::bimg
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_render PROPERTIES
    FOLDER "engine"
)

# Compile shaders at build time
if(TARGET bgfx::shaderc)
    set(BGFX_SHADER_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/external/bgfx.cmake/bgfx/src")
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
    )

    # Copy compiled shaders next to the runtime binaries so the renderer can find them
    add_custom_command(TARGET engine_shaders POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RUNTIME_SHADER_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_OUTPUT_DIR} ${RUNTIME_SHADER_DIR}
        COMMENT "Copying BGFX shaders to ${RUNTIME_SHADER_DIR}"
    )

    add_dependencies(engine_render engine_shaders)
endif()
