#include "time.h"
#include <GLFW/glfw3.h>
#include <iostream>

// Static initialization
f64 Time::s_LastFrameTime = 0.0;
f32 Time::s_DeltaTime = 0.0f;
f32 Time::s_FixedDeltaTime = 1.0f / 60.0f;  // 60 fps default
f32 Time::s_TotalTime = 0.0f;
f64 Time::s_StartTime = 0.0;
u64 Time::s_FrameCount = 0;
f32 Time::s_FPS = 0.0f;
f32 Time::s_TimeScale = 1.0f;
f32 Time::s_FPSTimer = 0.0f;
u32 Time::s_FPSFrameCount = 0;

void Time::Init() {
    s_StartTime = glfwGetTime();
    s_LastFrameTime = s_StartTime;
    s_DeltaTime = 0.0f;
    s_TotalTime = 0.0f;
    s_FrameCount = 0;
    s_FPS = 0.0f;
    s_TimeScale = 1.0f;
    s_FixedDeltaTime = 1.0f / 60.0f;
    s_FPSTimer = 0.0f;
    s_FPSFrameCount = 0;
    std::cout << "Time system initialized" << std::endl;
}

void Time::Update() {
    f64 currentTime = glfwGetTime();
    f32 rawDeltaTime = static_cast<f32>(currentTime - s_LastFrameTime);
    s_LastFrameTime = currentTime;

    // Apply time scale
    s_DeltaTime = rawDeltaTime * s_TimeScale;

    // Clamp delta time to prevent "spiral of death"
    // (e.g., breakpoint hit, computer went to sleep, etc.)
    if (s_DeltaTime > 0.1f) {
        s_DeltaTime = 0.1f;
    }

    s_TotalTime = static_cast<f32>(currentTime - s_StartTime);
    s_FrameCount++;

    // Calculate FPS (update once per second)
    s_FPSTimer += rawDeltaTime;
    s_FPSFrameCount++;

    if (s_FPSTimer >= 1.0f) {
        s_FPS = static_cast<f32>(s_FPSFrameCount) / s_FPSTimer;
        s_FPSTimer = 0.0f;
        s_FPSFrameCount = 0;
    }
}
