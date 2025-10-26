// Example: Using the Time Manager for frame-rate independent movement
// This demonstrates how to use Time::DeltaTime() for smooth, consistent movement

#include "core/time.h"
#include "core/math.h"
#include "platform/window.h"
#include <iostream>

int main() {
    // Initialize GLFW window
    WindowProperties props;
    props.title = "Time Manager Example";
    props.width = 1280;
    props.height = 720;
    props.vsync = true;
    Window window(props);

    // Initialize Time system
    Time::Init();

    // Game state
    Vec3 position(0.0f, 0.0f, 0.0f);
    Vec3 velocity(1.0f, 0.0f, 0.0f);  // Moving right at 1 unit/sec
    f32 speed = 5.0f;

    std::cout << "Time Manager Demo" << std::endl;
    std::cout << "=================" << std::endl;
    std::cout << "The cube will move at a constant speed regardless of frame rate" << std::endl;
    std::cout << std::endl;

    // Main loop
    while (!window.ShouldClose()) {
        Time::Update();
        window.PollEvents();

        // Frame-rate independent movement
        // This ensures the object moves at the same speed on all machines
        f32 dt = Time::DeltaTime();
        position += velocity * speed * dt;

        // Update window title with FPS and frame time every 60 frames
        if (Time::FrameCount() % 60 == 0) {
            f32 fps = Time::FPS();
            f32 deltaMs = Time::DeltaTimeMs();
            std::string title = "Time Manager Example - FPS: " + std::to_string(static_cast<i32>(fps)) +
                                " | Frame Time: " + std::to_string(deltaMs) + "ms";
            window.SetTitle(title);

            std::cout << "Position: (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
            std::cout << "  FPS: " << fps << " | Frame Time: " << deltaMs << "ms" << std::endl;
        }

        // Example: Time scaling (slow motion)
        // Uncomment to slow down time to 50%
        // Time::SetTimeScale(0.5f);

        // Example: Pause
        // Uncomment to pause the simulation
        // Time::SetTimeScale(0.0f);

        // Example: Using fixed timestep for physics
        // f32 fixedDt = Time::FixedDeltaTime();
        // RunPhysicsSimulation(fixedDt);  // Always runs at fixed rate (e.g., 60 Hz)
    }

    std::cout << std::endl;
    std::cout << "Summary:" << std::endl;
    std::cout << "  Total Runtime: " << Time::TotalTime() << " seconds" << std::endl;
    std::cout << "  Total Frames: " << Time::FrameCount() << std::endl;
    std::cout << "  Average FPS: " << Time::FPS() << std::endl;

    return 0;
}
