# Engine

C++20 game engine. Rendering (bgfx), physics (Jolt), audio (miniaudio), scripting (Lua/sol2), navigation (Recast), ECS (EnTT), Qt6 editor.

## Prerequisites

- Visual Studio 2026 with C++ desktop workload
- Qt6 for the editor (auto-detected from `C:\Qt\`)

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
