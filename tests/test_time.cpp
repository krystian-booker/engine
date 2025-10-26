#include "core/time.h"
#include <iostream>
#include <GLFW/glfw3.h>
#include <cmath>

// Test result tracking
static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) \
    static void name(); \
    static void name##_runner() { \
        testsRun++; \
        std::cout << "Running " << #name << "... "; \
        try { \
            name(); \
            testsPassed++; \
            std::cout << "PASSED" << std::endl; \
        } catch (...) { \
            testsFailed++; \
            std::cout << "FAILED (exception)" << std::endl; \
        } \
    } \
    static void name()

#define ASSERT(expr) \
    if (!(expr)) { \
        std::cout << "FAILED at line " << __LINE__ << ": " << #expr << std::endl; \
        testsFailed++; \
        testsRun++; \
        return; \
    }

// Helper function to sleep for a specific duration (in seconds)
void SleepSeconds(f64 seconds) {
    f64 start = glfwGetTime();
    while ((glfwGetTime() - start) < seconds) {
        // Busy wait
    }
}

// ============================================================================
// Time Manager Tests
// ============================================================================

TEST(Time_Initialization) {
    Time::Init();

    // After initialization, time should be valid
    ASSERT(Time::FrameCount() == 0);
    ASSERT(Time::TotalTime() >= 0.0f);
    ASSERT(Time::DeltaTime() == 0.0f);  // No frame yet
    ASSERT(Time::FPS() == 0.0f);        // No FPS calculated yet
    ASSERT(Time::TimeScale() == 1.0f);  // Default time scale
    ASSERT(Time::FixedDeltaTime() > 0.0f);  // Should have default value
}

TEST(Time_DeltaTimeCalculation) {
    Time::Init();

    // Wait a bit before first update
    SleepSeconds(0.016);  // ~16ms (60 FPS)

    Time::Update();

    // Delta time should be approximately 16ms
    f32 dt = Time::DeltaTime();
    ASSERT(dt > 0.0f);
    ASSERT(dt < 0.1f);  // Should be clamped below 0.1s

    // Delta time in milliseconds should be consistent
    f32 dtMs = Time::DeltaTimeMs();
    ASSERT(std::abs(dtMs - (dt * 1000.0f)) < 0.001f);
}

TEST(Time_FrameCounter) {
    Time::Init();

    ASSERT(Time::FrameCount() == 0);

    Time::Update();
    ASSERT(Time::FrameCount() == 1);

    Time::Update();
    ASSERT(Time::FrameCount() == 2);

    Time::Update();
    ASSERT(Time::FrameCount() == 3);
}

TEST(Time_TotalTimeTracking) {
    Time::Init();

    f32 startTime = Time::TotalTime();
    ASSERT(startTime == 0.0f);  // Should be 0 after init

    // Simulate some frames
    SleepSeconds(0.05);  // 50ms
    Time::Update();

    f32 currentTime = Time::TotalTime();
    ASSERT(currentTime > startTime);
    ASSERT((currentTime - startTime) >= 0.03f);  // At least 30ms passed (more lenient)
}

TEST(Time_TimeScale) {
    Time::Init();

    // Test default time scale
    ASSERT(Time::TimeScale() == 1.0f);

    // Set time scale to 0.5 (slow motion)
    Time::SetTimeScale(0.5f);
    ASSERT(Time::TimeScale() == 0.5f);

    SleepSeconds(0.02);  // 20ms
    Time::Update();

    f32 dt = Time::DeltaTime();
    // With 0.5 time scale, delta time should be roughly halved
    ASSERT(dt < 0.015f);  // Should be less than the sleep time

    // Set time scale to 0.0 (pause)
    Time::SetTimeScale(0.0f);
    ASSERT(Time::TimeScale() == 0.0f);

    SleepSeconds(0.02);
    Time::Update();

    dt = Time::DeltaTime();
    ASSERT(dt == 0.0f);  // Delta time should be 0 when paused

    // Reset time scale
    Time::SetTimeScale(1.0f);
}

TEST(Time_DeltaTimeClamping) {
    Time::Init();
    Time::Update();

    // Simulate a very long frame (e.g., breakpoint hit)
    SleepSeconds(0.2);  // 200ms
    Time::Update();

    f32 dt = Time::DeltaTime();
    // Delta time should be clamped to 0.1s (100ms)
    ASSERT(dt <= 0.1f);
}

TEST(Time_FixedDeltaTime) {
    Time::Init();

    // Default should be 1/60 = 0.0166...
    f32 fixedDt = Time::FixedDeltaTime();
    ASSERT(std::abs(fixedDt - (1.0f / 60.0f)) < 0.0001f);

    // Set custom fixed delta time
    Time::SetFixedDeltaTime(1.0f / 30.0f);  // 30 FPS
    ASSERT(std::abs(Time::FixedDeltaTime() - (1.0f / 30.0f)) < 0.0001f);

    // Fixed delta time should not be affected by actual frame time
    Time::Update();
    ASSERT(std::abs(Time::FixedDeltaTime() - (1.0f / 30.0f)) < 0.0001f);
}

TEST(Time_FPSCalculation) {
    Time::Init();

    // FPS should be 0 initially
    ASSERT(Time::FPS() == 0.0f);

    // Simulate frames at 60 FPS for over 1 second
    for (int i = 0; i < 70; i++) {
        SleepSeconds(0.016);  // ~16ms per frame
        Time::Update();
    }

    // After 1 second, FPS should be calculated
    f32 fps = Time::FPS();
    ASSERT(fps > 0.0f);
    ASSERT(fps >= 50.0f && fps <= 70.0f);  // Should be roughly 60 FPS
}

TEST(Time_MultipleUpdates) {
    Time::Init();

    // Simulate multiple frames
    for (int i = 0; i < 10; i++) {
        SleepSeconds(0.01);  // 10ms
        Time::Update();

        ASSERT(Time::FrameCount() == static_cast<u64>(i + 1));
        ASSERT(Time::DeltaTime() > 0.0f);
        ASSERT(Time::TotalTime() > 0.0f);
    }
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    std::cout << "=== Time Manager Tests ===" << std::endl;
    std::cout << std::endl;

    // Initialize GLFW for timing functions
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return 1;
    }

    // Run all tests
    Time_Initialization_runner();
    Time_DeltaTimeCalculation_runner();
    Time_FrameCounter_runner();
    Time_TotalTimeTracking_runner();
    Time_TimeScale_runner();
    Time_DeltaTimeClamping_runner();
    Time_FixedDeltaTime_runner();
    Time_FPSCalculation_runner();
    Time_MultipleUpdates_runner();

    // Print summary
    std::cout << std::endl;
    std::cout << "===============================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "===============================================" << std::endl;

    // Terminate GLFW
    glfwTerminate();

    return (testsFailed == 0) ? 0 : 1;
}
