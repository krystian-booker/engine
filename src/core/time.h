#pragma once
#include "core/types.h"

class Time {
public:
    static void Init();
    static void Update();  // Call at start of each frame

    // Delta time (frame time in seconds)
    static f32 DeltaTime() { return s_DeltaTime; }
    static f32 DeltaTimeMs() { return s_DeltaTime * 1000.0f; }

    // Fixed timestep (for physics/gameplay - deterministic)
    static f32 FixedDeltaTime() { return s_FixedDeltaTime; }
    static void SetFixedDeltaTime(f32 dt) { s_FixedDeltaTime = dt; }

    // Total time since start
    static f32 TotalTime() { return s_TotalTime; }

    // Frame count
    static u64 FrameCount() { return s_FrameCount; }

    // FPS (frames per second)
    static f32 FPS() { return s_FPS; }

    // Time scale (for slow-mo, pause, etc.)
    static f32 TimeScale() { return s_TimeScale; }
    static void SetTimeScale(f32 scale) { s_TimeScale = scale; }

private:
    Time() = default;

    static f64 s_LastFrameTime;
    static f32 s_DeltaTime;
    static f32 s_FixedDeltaTime;
    static f32 s_TotalTime;
    static f64 s_StartTime;
    static u64 s_FrameCount;
    static f32 s_FPS;
    static f32 s_TimeScale;

    // FPS calculation (rolling average)
    static f32 s_FPSTimer;
    static u32 s_FPSFrameCount;
};
