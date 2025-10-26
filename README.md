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

