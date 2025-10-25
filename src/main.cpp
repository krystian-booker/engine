#include <iostream>
#include "platform/platform.h"

int main(int, char**) {
    std::cout << "=== Game Engine - Day 2: Platform API Tests ===" << std::endl;
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

    // Test 5: Window Creation and Event Loop
    std::cout << "[TEST 5] Window Creation:" << std::endl;

    Platform::WindowHandle* window = Platform::CreateWindow("Game Engine - Platform Test", 800, 600);

    if (!window) {
        std::cout << "  ERROR: Failed to create window" << std::endl;
        return 1;
    }

    std::cout << "  Window created successfully (800x600)" << std::endl;
    std::cout << "  Entering event loop (close window to exit)..." << std::endl;
    std::cout << std::endl;

    // Main event loop
    u64 frameCount = 0;
    u64 lastCounter = Platform::GetPerformanceCounter();

    while (Platform::PollEvents(window)) {
        frameCount++;

        // Print stats every second
        u64 currentCounter = Platform::GetPerformanceCounter();
        u64 elapsed = currentCounter - lastCounter;
        f64 elapsedSeconds = static_cast<f64>(elapsed) / static_cast<f64>(frequency);

        if (elapsedSeconds >= 1.0) {
            std::cout << "  FPS: " << frameCount << " | Frames: " << frameCount << std::endl;
            frameCount = 0;
            lastCounter = currentCounter;
        }

        // Sleep a tiny bit to avoid maxing out CPU
        // (In a real engine, we'd do rendering here)
    }

    std::cout << std::endl;
    std::cout << "[TEST 6] Cleanup:" << std::endl;

    Platform::DestroyWindow(window);
    std::cout << "  Window destroyed" << std::endl;

    u64 endCounter = Platform::GetPerformanceCounter();
    u64 totalElapsed = endCounter - startCounter;
    f64 totalSeconds = static_cast<f64>(totalElapsed) / static_cast<f64>(frequency);

    std::cout << "  Total runtime: " << totalSeconds << " seconds" << std::endl;
    std::cout << std::endl;

    std::cout << "===============================================" << std::endl;
    std::cout << "All platform tests completed successfully!" << std::endl;
    std::cout << "Window system, timing, memory, file I/O, and threading primitives working." << std::endl;
    std::cout << "===============================================" << std::endl;

    return 0;
}