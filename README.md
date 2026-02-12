# Engine

C++20 game engine. Rendering (bgfx), physics (Jolt), audio (miniaudio), scripting (Lua/sol2), navigation (Recast), ECS (EnTT), Qt6 editor.

## Prerequisites

- Visual Studio 2026 with C++ desktop workload
- Qt6 for the editor (auto-detected from `C:\Qt\`)

## Setup

Ensure vcpkg is installed:

```
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
cd ..
```

CMake picks up vcpkg automatically from `vcpkg integrate install`. No env vars needed.

If you already have vcpkg elsewhere, set `VCPKG_ROOT` or pass `-DCMAKE_TOOLCHAIN_FILE=...` manually.

## Build

```
build.bat              # Debug
build.bat release      # Release
build.bat clean        # Clean rebuild
build.bat tests        # Build + run tests
```

Or directly:

```
cmake --preset Qt-Debug
cmake --build out/build/Qt-Debug
```

First configure is slow (vcpkg installs packages + bgfx clones from git). After that it's fast.

Output: `out/build/Qt-Debug/bin/` and `lib/`.

## Tests

```
build.bat tests
```

## Project layout

Modules live under `engine/`, each with `include/`, `src/`, `tests/`. CMake targets are `engine::{module}`.

```
engine/core/         engine/render/       engine/physics/
engine/scene/        engine/audio/        engine/navigation/
engine/script/       engine/asset/        engine/ui/
engine/reflect/      engine/terrain/      engine/debug_gui/
...
```

## CMake options

| Option | Default | |
|---|---|---|
| `ENGINE_BUILD_EDITOR` | ON | Qt6 editor |
| `ENGINE_BUILD_SAMPLES` | ON | Sample apps |
| `ENGINE_BUILD_TESTS` | OFF | Catch2 tests |
| `ENGINE_INSTALL` | OFF | Install targets |

## Samples

`samples/spinning_cube`, `samples/bouncing_balls`, `samples/environment_demo`, `samples/ai_demo`

## Editor

Qt6 editor in `editor/`. Hierarchy, Inspector, Asset Browser, Console, Viewport panels.

## Game plugins

Games are DLL plugins via `add_game_dll()`. Starter template in `templates/game_template/`.
