# Engine

Data-driven game engine experiment targeting modern rendering backends.

## Prerequisites

- CMake 3.20+ and a C++20 capable compiler (MSVC, Clang, or GCC).
- [Vulkan SDK](https://vulkan.lunarg.com/) 1.3+.
  - On Windows set the following environment variables (installer does this by default):
    - `VULKAN_SDK=C:\VulkanSDK\1.4.328.1`
    - `VK_SDK_PATH=C:\VulkanSDK\1.4.328.1`
    - Append `C:\VulkanSDK\1.4.328.1\Bin` to `PATH`.
  - Validation layers are enabled automatically for Debug builds.
- GLFW, GLM, and nlohmann/json are fetched automatically via CMake.

## Configure & Build

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Executables will be placed in `build/bin` (or `build/bin/Debug` on multi-config generators).

## Tests

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

## Running

After building, launch `build/bin/engine.exe` (or `build/bin/Debug/engine.exe` depending on generator).

## Vulkan Renderer

The default runtime now initializes a Vulkan context, swapchain, and a render loop that uploads a cube mesh into device-local vertex/index buffers. The graphics pipeline consumes the `Vertex` layout (position/normal/color/UV) and renders the indexed cube with face-dependent colors. Resize or minimize the window to exercise swapchain recreation; the renderer blocks until valid dimensions are available and resumes presenting without restarting the application.

## Render System

The ECS-driven render path assembles draw calls by querying entities that own both `Transform` and `Renderable` components. `RenderSystem::Update` collects a list of `RenderData` entries (world matrix + mesh handle), lazily uploads meshes through `MeshManager`, and exposes cached `VulkanMesh` instances for the renderer. The main loop now updates transforms each frame, invokes `RenderSystem::Update`, then asks `VulkanRenderer` to iterate that list and issue draw calls; a fallback path draws the legacy rotating cube when no renderable entities are available.

## Shaders

Shader sources live in `assets/shaders`. After editing `triangle.vert` or `triangle.frag`, rebuild the SPIR-V binaries using:

```powershell
.\compile_shaders.bat
```

The script expects the Microsoft DXC compiler (`dxc.exe`) from the Vulkan SDK to be available on `PATH`.
