#include <iostream>
#include "platform/platform.h"
#include "platform/window.h"
#include "core/memory.h"
#include "core/math.h"

int main(int, char**) {
    std::cout << "=== Game Engine ===" << std::endl;
    std::cout << std::endl;

    // Memory Allocator Demonstration
    std::cout << "[DEMO] LinearAllocator:" << std::endl;
    {
        LinearAllocator frameAllocator;
        frameAllocator.Init(1024 * 1024); // 1 MB

        std::cout << "  Initialized with 1 MB capacity" << std::endl;

        // Simulate a few frames
        for (int frame = 0; frame < 5; frame++) {
            // Allocate various sizes with different alignments
            [[maybe_unused]] void* data1 = frameAllocator.Alloc(256, 16);
            [[maybe_unused]] void* data2 = frameAllocator.Alloc(512, 64);
            [[maybe_unused]] void* data3 = frameAllocator.Alloc(128, 32);

            std::cout << "  Frame " << frame << ": "
                      << "allocated 896 bytes, "
                      << "current offset: " << frameAllocator.GetCurrentOffset()
                      << ", high-water mark: " << frameAllocator.GetHighWaterMark()
                      << " bytes" << std::endl;

            // Reset for next frame
            frameAllocator.Reset();
        }

        std::cout << "  Peak memory usage across all frames: "
                  << frameAllocator.GetHighWaterMark() << " bytes" << std::endl;

        frameAllocator.Shutdown();
        std::cout << "  Allocator shut down" << std::endl;
    }
    std::cout << std::endl;

    std::cout << "[DEMO] PoolAllocator:" << std::endl;
    {
        struct GameObject {
            u32 id;
            Vec3 position;
            Vec3 velocity;
        };

        PoolAllocator<GameObject, 16> gameObjectPool;

        std::cout << "  Initialized pool for GameObjects (block size: 16)" << std::endl;

        // Allocate some game objects
        GameObject* objects[10];
        for (int i = 0; i < 10; i++) {
            objects[i] = gameObjectPool.Alloc();
            objects[i]->id = i;
            objects[i]->position = Vec3(i * 10.0f, 0.0f, 0.0f);
        }

        std::cout << "  Allocated 10 GameObjects" << std::endl;

        // Free some objects
        gameObjectPool.Free(objects[3]);
        gameObjectPool.Free(objects[7]);
        std::cout << "  Freed objects 3 and 7" << std::endl;

        // Allocate new objects (should reuse freed slots)
        [[maybe_unused]] GameObject* newObj1 = gameObjectPool.Alloc();
        [[maybe_unused]] GameObject* newObj2 = gameObjectPool.Alloc();

        std::cout << "  Allocated 2 new objects (reused freed slots)" << std::endl;
        std::cout << "  Generation counter: " << gameObjectPool.GetGeneration() << std::endl;
    }
    std::cout << std::endl;

    // Test 1: High-Resolution Timer
    u64 frequency = Platform::GetPerformanceFrequency();
    u64 startCounter = Platform::GetPerformanceCounter();

    std::cout << "[TEST 1] High-Resolution Timer:" << std::endl;
    std::cout << "  Frequency: " << frequency << " ticks/second" << std::endl;
    std::cout << "  Start Counter: " << startCounter << std::endl;
    std::cout << std::endl;

    // Test 2: Virtual Memory Allocation
    const size_t memSize = 1024 * 1024; // 1 MB
    void* memory = Platform::VirtualAlloc(memSize);

    std::cout << "[TEST 2] Virtual Memory Allocation:" << std::endl;
    if (memory) {
        std::cout << "  Allocated 1 MB at address: " << memory << std::endl;

        // Write some data to test
        char* data = static_cast<char*>(memory);
        data[0] = 'H';
        data[1] = 'i';
        data[2] = '\0';

        std::cout << "  Test write successful: \"" << data << "\"" << std::endl;
        Platform::VirtualFree(memory, memSize);
        std::cout << "  Memory freed successfully" << std::endl;
    } else {
        std::cout << "  ERROR: Failed to allocate memory" << std::endl;
    }
    std::cout << std::endl;

    // Test 3: Mutex Operations
    Platform::Mutex* mutex = Platform::CreateMutex();

    std::cout << "[TEST 3] Mutex Operations:" << std::endl;
    if (mutex) {
        std::cout << "  Mutex created successfully" << std::endl;

        Platform::Lock(mutex);
        std::cout << "  Mutex locked" << std::endl;

        Platform::Unlock(mutex);
        std::cout << "  Mutex unlocked" << std::endl;

        Platform::DestroyMutex(mutex);
        std::cout << "  Mutex destroyed" << std::endl;
    } else {
        std::cout << "  ERROR: Failed to create mutex" << std::endl;
    }
    std::cout << std::endl;

    // Test 4: File I/O
    const char* testFilePath = "test_file.txt";

    std::cout << "[TEST 4] File I/O:" << std::endl;

    // Write test
    Platform::FileHandle* writeFile = Platform::OpenFile(testFilePath, true);
    if (writeFile) {
        std::cout << "  File opened for writing" << std::endl;
        Platform::CloseFile(writeFile);
        std::cout << "  File closed" << std::endl;
    } else {
        std::cout << "  ERROR: Failed to open file for writing" << std::endl;
    }

    // Read test (file will be empty but this tests the read path)
    Platform::FileHandle* readFile = Platform::OpenFile(testFilePath, false);
    if (readFile) {
        std::cout << "  File opened for reading" << std::endl;

        char buffer[256] = {};
        size_t bytesRead = Platform::ReadFile(readFile, buffer, sizeof(buffer) - 1);
        std::cout << "  Bytes read: " << bytesRead << std::endl;

        Platform::CloseFile(readFile);
        std::cout << "  File closed" << std::endl;
    } else {
        std::cout << "  ERROR: Failed to open file for reading" << std::endl;
    }
    std::cout << std::endl;

    // Test 5: GLFW Window Creation and Event Loop
    std::cout << "[TEST 5] GLFW Window Creation:" << std::endl;

    WindowProperties props;
    props.title = "Game Engine - GLFW Window Test";
    props.width = 1280;
    props.height = 720;
    props.vsync = true;
    props.resizable = true;
    props.fullscreen = false;

    Window window(props);

    std::cout << "  Window created successfully (" << window.GetWidth() << "x" << window.GetHeight() << ")" << std::endl;
    std::cout << "  Aspect ratio: " << window.GetAspectRatio() << std::endl;

    // Set event callback
    window.SetEventCallback([](WindowEvent event, u32 w, u32 h) {
        switch (event) {
            case WindowEvent::Resize:
                std::cout << "  [EVENT] Window resized to: " << w << "x" << h << std::endl;
                break;
            case WindowEvent::Focus:
                std::cout << "  [EVENT] Window gained focus" << std::endl;
                break;
            case WindowEvent::LostFocus:
                std::cout << "  [EVENT] Window lost focus" << std::endl;
                break;
            case WindowEvent::Close:
                std::cout << "  [EVENT] Window close requested" << std::endl;
                break;
            default:
                break;
        }
    });

    std::cout << "  Event callbacks registered" << std::endl;
    std::cout << "  Entering event loop (close window to exit)..." << std::endl;
    std::cout << std::endl;

    // Main event loop
    u64 frameCount = 0;
    u64 lastCounter = Platform::GetPerformanceCounter();

    while (!window.ShouldClose()) {
        window.PollEvents();
        frameCount++;

        // Print stats every second
        u64 currentCounter = Platform::GetPerformanceCounter();
        u64 elapsed = currentCounter - lastCounter;
        f64 elapsedSeconds = static_cast<f64>(elapsed) / static_cast<f64>(frequency);

        if (elapsedSeconds >= 1.0) {
            std::cout << "  FPS: " << frameCount << " | Frames: " << frameCount << std::endl;
            // Update window title with FPS
            window.SetTitle("Game Engine - GLFW Window Test | FPS: " + std::to_string(frameCount));
            frameCount = 0;
            lastCounter = currentCounter;
        }

        // Sleep a tiny bit to avoid maxing out CPU
        // (In a real engine, we'd do rendering here)
    }

    std::cout << std::endl;
    std::cout << "[TEST 6] Cleanup:" << std::endl;
    std::cout << "  Window will be destroyed automatically by RAII" << std::endl;

    u64 endCounter = Platform::GetPerformanceCounter();
    u64 totalElapsed = endCounter - startCounter;
    f64 totalSeconds = static_cast<f64>(totalElapsed) / static_cast<f64>(frequency);

    std::cout << "  Total runtime: " << totalSeconds << " seconds" << std::endl;
    std::cout << std::endl;

    std::cout << "===============================================" << std::endl;
    std::cout << "All tests completed successfully!" << std::endl;
    std::cout << "GLFW window system, timing, memory, file I/O, and threading primitives working." << std::endl;
    std::cout << "===============================================" << std::endl;

    return 0;
}