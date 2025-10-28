#!/bin/bash
# Requires Microsoft DXC (dxc). Compiles HLSL to SPIR-V for Vulkan.
set -e

# Vertex shader
dxc -spirv -fspv-target-env=vulkan1.2 -T vs_6_7 -E main \
    assets/shaders/cube.vert -Fo assets/shaders/cube.vert.spv
# Fragment shader
dxc -spirv -fspv-target-env=vulkan1.2 -T ps_6_7 -E main \
    assets/shaders/cube.frag -Fo assets/shaders/cube.frag.spv

# Compute shaders
dxc -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main \
    assets/shaders/mipgen_color.comp -Fo assets/shaders/mipgen_color.comp.spv -I assets/shaders
dxc -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main \
    assets/shaders/mipgen_normal.comp -Fo assets/shaders/mipgen_normal.comp.spv -I assets/shaders
dxc -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main \
    assets/shaders/mipgen_roughness.comp -Fo assets/shaders/mipgen_roughness.comp.spv -I assets/shaders
dxc -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main \
    assets/shaders/mipgen_srgb.comp -Fo assets/shaders/mipgen_srgb.comp.spv -I assets/shaders

echo "Shaders compiled"
