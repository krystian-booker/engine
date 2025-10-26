@echo off
REM Requires Microsoft DXC (dxc). Compiles HLSL to SPIR-V for Vulkan.

REM Vertex shader
dxc -spirv -fspv-target-env=vulkan1.2 -T vs_6_7 -E main ^
    assets/shaders/triangle.vert -Fo assets/shaders/triangle.vert.spv
IF ERRORLEVEL 1 goto :error

REM Fragment shader
dxc -spirv -fspv-target-env=vulkan1.2 -T ps_6_7 -E main ^
    assets/shaders/triangle.frag -Fo assets/shaders/triangle.frag.spv
IF ERRORLEVEL 1 goto :error

echo Shaders compiled
exit /b 0

:error
echo Shader compilation failed
exit /b 1
