#!/bin/bash
# Requires Microsoft DXC (dxc). Compiles HLSL to SPIR-V for Vulkan.
set -e

# Vertex shader
dxc -spirv -fspv-target-env=vulkan1.2 -T vs_6_7 -E main \
    assets/shaders/triangle.vert -Fo assets/shaders/triangle.vert.spv
# Fragment shader
dxc -spirv -fspv-target-env=vulkan1.2 -T ps_6_7 -E main \
    assets/shaders/triangle.frag -Fo assets/shaders/triangle.frag.spv

echo "Shaders compiled"
