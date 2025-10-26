#!/bin/bash
# Requires Microsoft DXC (dxc). Compiles HLSL to SPIR-V for Vulkan.
set -e

# Vertex shader
dxc -spirv -fspv-target-env=vulkan1.2 -T vs_6_7 -E main \
    assets/shaders/cube.vert -Fo assets/shaders/cube.vert.spv
# Fragment shader
dxc -spirv -fspv-target-env=vulkan1.2 -T ps_6_7 -E main \
    assets/shaders/cube.frag -Fo assets/shaders/cube.frag.spv

echo "Shaders compiled"
