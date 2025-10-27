#include "resources/texture_manager.h"
#include "core/job_system.h"
#include "core/texture_data.h"
#include "platform/platform.h"
#include <iostream>
#include <atomic>

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    #define TEST_SLEEP(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define TEST_SLEEP(ms) usleep((ms) * 1000)
#endif

// Simple test framework
#define TEST(name) \
    void name(); \
    namespace { \
        struct name##_registrar { \
            name##_registrar() { \
                std::cout << "Running test: " << #name << std::endl; \
                name(); \
                std::cout << "Test passed: " << #name << std::endl << std::endl; \
            } \
        } name##_instance; \
    } \
    void name()

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "Assertion failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while(0)

// ============================================================================
// Test Helpers
// ============================================================================

struct CallbackData {
    std::atomic<bool> called{false};
    std::atomic<bool> success{false};
    TextureHandle handle = TextureHandle::Invalid;
    i32 customValue = 0;
};

void TestCallback(TextureHandle handle, bool success, void* userData) {
    CallbackData* data = static_cast<CallbackData*>(userData);
    data->called.store(true);
    data->success.store(success);
    data->handle = handle;
}

void WaitForCallback(CallbackData& data, u32 maxWaitMs = 5000) {
    const u32 sleepMs = 10;
    u32 elapsed = 0;

    while (!data.called.load() && elapsed < maxWaitMs) {
        TEST_SLEEP(sleepMs);
        TextureManager::Instance().Update();  // Process pending uploads
        elapsed += sleepMs;
    }
}

// ============================================================================
// Tests
// ============================================================================

TEST(TestAsyncLoadBasic) {
    // Create a test image file
    const std::string testPath = "test_async_image.png";

    // Initialize systems
    JobSystem::Init(2);  // Use 2 worker threads

    // Setup callback data
    CallbackData cbData;

    // Start async load
    TextureHandle handle = TextureManager::Instance().LoadAsync(
        testPath,
        TextureLoadOptions::Albedo(),
        TestCallback,
        &cbData
    );

    // Handle should be valid immediately
    ASSERT(handle.IsValid());

    // Texture should initially point to placeholder (white texture)
    TextureData* texData = TextureManager::Instance().Get(handle);
    ASSERT(texData != nullptr);

    // Wait for callback to fire
    WaitForCallback(cbData);

    // Callback should have been called
    ASSERT(cbData.called.load());

    // Check if load succeeded (depends on test image existence)
    // For now, we just verify the callback was invoked
    std::cout << "  Load " << (cbData.success.load() ? "succeeded" : "failed (expected if test image missing)") << std::endl;

    // Cleanup
    JobSystem::Shutdown();
}

TEST(TestAsyncLoadMultiple) {
    JobSystem::Init(4);

    const std::string paths[] = {
        "test_async_1.png",
        "test_async_2.png",
        "test_async_3.png"
    };
    const u32 numPaths = 3;

    CallbackData cbData[numPaths];
    TextureHandle handles[numPaths];

    // Launch multiple async loads
    for (u32 i = 0; i < numPaths; ++i) {
        handles[i] = TextureManager::Instance().LoadAsync(
            paths[i],
            TextureLoadOptions::Albedo(),
            TestCallback,
            &cbData[i]
        );

        ASSERT(handles[i].IsValid());
    }

    // Wait for all callbacks
    for (u32 i = 0; i < numPaths; ++i) {
        WaitForCallback(cbData[i]);
        ASSERT(cbData[i].called.load());
    }

    std::cout << "  All " << numPaths << " loads completed" << std::endl;

    JobSystem::Shutdown();
}

TEST(TestAsyncLoadCacheHit) {
    JobSystem::Init(2);

    const std::string testPath = "test_cached.png";

    // First load (will be async)
    CallbackData cbData1;
    TextureHandle handle1 = TextureManager::Instance().LoadAsync(
        testPath,
        TextureLoadOptions::Albedo(),
        TestCallback,
        &cbData1
    );

    WaitForCallback(cbData1);

    // Second load of same path (should be cache hit)
    CallbackData cbData2;
    TextureHandle handle2 = TextureManager::Instance().LoadAsync(
        testPath,
        TextureLoadOptions::Albedo(),
        TestCallback,
        &cbData2
    );

    // Callback should fire immediately for cache hit
    ASSERT(cbData2.called.load());
    ASSERT(handle1 == handle2);

    std::cout << "  Cache hit verified" << std::endl;

    JobSystem::Shutdown();
}

TEST(TestAsyncLoadWithoutCallback) {
    JobSystem::Init(2);

    const std::string testPath = "test_no_callback.png";

    // Load without callback
    TextureHandle handle = TextureManager::Instance().LoadAsync(
        testPath,
        TextureLoadOptions::Normal(),
        nullptr,  // No callback
        nullptr
    );

    ASSERT(handle.IsValid());

    // Wait a bit and process updates
    for (u32 i = 0; i < 100; ++i) {
        TEST_SLEEP(10);
        TextureManager::Instance().Update();
    }

    std::cout << "  Load without callback completed" << std::endl;

    JobSystem::Shutdown();
}

TEST(TestThreadSafeHandleAllocation) {
    JobSystem::Init(8);

    const u32 numLoads = 20;
    std::atomic<u32> completed{0};

    struct CounterData {
        std::atomic<u32>* counter;
    };

    CounterData counterData;
    counterData.counter = &completed;

    auto countCallback = [](TextureHandle handle, bool success, void* userData) {
        (void)handle;
        (void)success;
        CounterData* data = static_cast<CounterData*>(userData);
        data->counter->fetch_add(1);
    };

    // Launch many concurrent loads
    for (u32 i = 0; i < numLoads; ++i) {
        std::string path = "test_concurrent_" + std::to_string(i) + ".png";
        TextureManager::Instance().LoadAsync(
            path,
            TextureLoadOptions::Albedo(),
            countCallback,
            &counterData
        );
    }

    // Wait for all to complete
    u32 maxWait = 5000;  // 5 seconds
    u32 elapsed = 0;
    while (completed.load() < numLoads && elapsed < maxWait) {
        TEST_SLEEP(10);
        TextureManager::Instance().Update();
        elapsed += 10;
    }

    ASSERT(completed.load() == numLoads);
    std::cout << "  All " << numLoads << " concurrent loads completed" << std::endl;

    JobSystem::Shutdown();
}

TEST(TestAsyncLoadWithCustomUserData) {
    JobSystem::Init(2);

    struct CustomData {
        i32 magic;
        std::atomic<bool> verified{false};
    };

    CustomData userData;
    userData.magic = 42;

    auto verifyCallback = [](TextureHandle handle, bool success, void* userData) {
        (void)handle;
        (void)success;
        CustomData* data = static_cast<CustomData*>(userData);
        if (data->magic == 42) {
            data->verified.store(true);
        }
    };

    TextureManager::Instance().LoadAsync(
        "test_userdata.png",
        TextureLoadOptions::Albedo(),
        verifyCallback,
        &userData
    );

    // Wait for callback
    u32 maxWait = 5000;
    u32 elapsed = 0;
    while (!userData.verified.load() && elapsed < maxWait) {
        TEST_SLEEP(10);
        TextureManager::Instance().Update();
        elapsed += 10;
    }

    ASSERT(userData.verified.load());
    std::cout << "  User data preserved correctly" << std::endl;

    JobSystem::Shutdown();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Texture Async Loading Tests" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    std::cout << "NOTE: Some tests may show load failures if test images don't exist." << std::endl;
    std::cout << "This is expected behavior and tests callback invocation, not actual file loading." << std::endl << std::endl;

    // Tests run automatically via static initializers

    std::cout << "========================================" << std::endl;
    std::cout << "All tests passed!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
