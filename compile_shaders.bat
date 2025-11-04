@echo off
REM Requires Microsoft DXC (dxc). Compiles HLSL to SPIR-V for Vulkan.

REM Use Vulkan SDK DXC for SPIR-V support
set DXC="C:\VulkanSDK\1.4.328.1\Bin\dxc.exe"

REM Vertex shader
%DXC% -spirv -fspv-target-env=vulkan1.2 -T vs_6_7 -E main ^
    assets/shaders/cube.vert -Fo assets/shaders/cube.vert.spv
IF ERRORLEVEL 1 goto :error

REM Fragment shader
%DXC% -spirv -fspv-target-env=vulkan1.2 -T ps_6_7 -E main ^
    assets/shaders/cube.frag -Fo assets/shaders/cube.frag.spv
IF ERRORLEVEL 1 goto :error

REM Depth prepass shaders
%DXC% -spirv -fspv-target-env=vulkan1.2 -T vs_6_7 -E main ^
    assets/shaders/depth_prepass.vert -Fo assets/shaders/depth_prepass.vert.spv
IF ERRORLEVEL 1 goto :error

%DXC% -spirv -fspv-target-env=vulkan1.2 -T ps_6_7 -E main ^
    assets/shaders/depth_prepass.frag -Fo assets/shaders/depth_prepass.frag.spv
IF ERRORLEVEL 1 goto :error

REM Shadow pass shader
%DXC% -spirv -fspv-target-env=vulkan1.2 -T vs_6_7 -E main ^
    assets/shaders/shadow.vert -Fo assets/shaders/shadow.vert.spv
IF ERRORLEVEL 1 goto :error

REM Compute shaders
%DXC% -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main ^
    assets/shaders/mipgen_color.comp -Fo assets/shaders/mipgen_color.comp.spv -I assets/shaders
IF ERRORLEVEL 1 goto :error

%DXC% -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main ^
    assets/shaders/mipgen_normal.comp -Fo assets/shaders/mipgen_normal.comp.spv -I assets/shaders
IF ERRORLEVEL 1 goto :error

%DXC% -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main ^
    assets/shaders/mipgen_roughness.comp -Fo assets/shaders/mipgen_roughness.comp.spv -I assets/shaders
IF ERRORLEVEL 1 goto :error

%DXC% -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main ^
    assets/shaders/mipgen_srgb.comp -Fo assets/shaders/mipgen_srgb.comp.spv -I assets/shaders
IF ERRORLEVEL 1 goto :error

REM Light culling compute shader
%DXC% -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main ^
    assets/shaders/light_culling.comp -Fo assets/shaders/light_culling.comp.spv
IF ERRORLEVEL 1 goto :error

REM EVSM prefilter compute shader (for shadow filtering)
%DXC% -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main ^
    assets/shaders/evsm_prefilter.comp -Fo assets/shaders/evsm_prefilter.comp.spv
IF ERRORLEVEL 1 goto :error

REM EVSM blur compute shader (Gaussian blur for EVSM moments)
%DXC% -spirv -fspv-target-env=vulkan1.2 -T cs_6_7 -E main ^
    assets/shaders/evsm_blur.comp -Fo assets/shaders/evsm_blur.comp.spv
IF ERRORLEVEL 1 goto :error

REM Forward+ debug visualization shaders
%DXC% -spirv -fspv-target-env=vulkan1.2 -T vs_6_7 -E main ^
    assets/shaders/forward_plus_debug.vert -Fo assets/shaders/forward_plus_debug.vert.spv
IF ERRORLEVEL 1 goto :error

%DXC% -spirv -fspv-target-env=vulkan1.2 -T ps_6_7 -E main ^
    assets/shaders/forward_plus_debug.frag -Fo assets/shaders/forward_plus_debug.frag.spv
IF ERRORLEVEL 1 goto :error

echo Shaders compiled
exit /b 0

:error
echo Shader compilation failed
exit /b 1
