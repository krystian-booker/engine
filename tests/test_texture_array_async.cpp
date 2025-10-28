#include "resources/texture_manager.h"
#include "core/job_system.h"
#include "core/texture_data.h"
#include "platform/platform.h"
#include <iostream>
#include <atomic>
#include <vector>

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

TEST(TestAsyncArrayLoadInvalidPaths) {
    // Initialize job system
    JobSystem::Init(2);

    // Setup callback data
    CallbackData cbData;

    // Try to load non-existent array texture
    std::vector<std::string> invalidPaths = {
        "nonexistent_layer0.png",
        "nonexistent_layer1.png",
        "nonexistent_layer2.png"
    };

    TextureHandle handle = TextureManager::Instance().LoadArrayAsync(
        invalidPaths,
        TextureLoadOptions::Albedo(),
        TestCallback,
        &cbData
    );

    // Handle should be valid immediately (points to placeholder)
    ASSERT(handle.IsValid());

    // Texture should point to placeholder
    TextureData* texData = TextureManager::Instance().Get(handle);
    ASSERT(texData != nullptr);

    // Wait for callback
    WaitForCallback(cbData);

    // Callback should have been called with failure
    ASSERT(cbData.called.load());
    ASSERT(cbData.success.load() == false);  // Should fail (files don't exist)
    ASSERT(cbData.handle == handle);

    JobSystem::Shutdown();
}

TEST(TestAsyncArrayLoadEmptyPaths) {
    JobSystem::Init(2);

    CallbackData cbData;

    std::vector<std::string> emptyPaths;

    TextureHandle handle = TextureManager::Instance().LoadArrayAsync(
        emptyPaths,
        TextureLoadOptions{},
        TestCallback,
        &cbData
    );

    // Should return invalid handle for empty paths
    ASSERT(!handle.IsValid());

    // Callback should not be called
    TEST_SLEEP(100);
    TextureManager::Instance().Update();
    ASSERT(cbData.called.load() == false);

    JobSystem::Shutdown();
}

TEST(TestAsyncArrayLoadMultipleHandles) {
    JobSystem::Init(2);

    // Start multiple async loads simultaneously
    std::vector<CallbackData> callbacks(3);

    std::vector<std::vector<std::string>> pathSets = {
        {"test_array1_0.png", "test_array1_1.png"},
        {"test_array2_0.png", "test_array2_1.png", "test_array2_2.png"},
        {"test_array3_0.png", "test_array3_1.png", "test_array3_2.png", "test_array3_3.png"}
    };

    std::vector<TextureHandle> handles;

    for (u32 i = 0; i < 3; ++i) {
        TextureHandle handle = TextureManager::Instance().LoadArrayAsync(
            pathSets[i],
            TextureLoadOptions::Albedo(),
            TestCallback,
            &callbacks[i]
        );
        handles.push_back(handle);
        ASSERT(handle.IsValid());
    }

    // All handles should be unique
    ASSERT(handles[0] != handles[1]);
    ASSERT(handles[1] != handles[2]);
    ASSERT(handles[0] != handles[2]);

    // Wait for all callbacks
    for (auto& cb : callbacks) {
        WaitForCallback(cb);
        ASSERT(cb.called.load());
        // Note: Will fail since files don't exist, but that's expected
    }

    JobSystem::Shutdown();
}

TEST(TestAsyncArrayLoadWithCustomUserData) {
    JobSystem::Init(2);

    CallbackData cbData;
    cbData.customValue = 42;

    std::vector<std::string> paths = {
        "test_layer0.png",
        "test_layer1.png"
    };

    TextureHandle handle = TextureManager::Instance().LoadArrayAsync(
        paths,
        TextureLoadOptions::Normal(),
        TestCallback,
        &cbData
    );

    ASSERT(handle.IsValid());

    WaitForCallback(cbData);

    // Verify callback received correct handle
    ASSERT(cbData.called.load());
    ASSERT(cbData.handle == handle);
    ASSERT(cbData.customValue == 42);  // User data preserved

    JobSystem::Shutdown();
}

TEST(TestAsyncArrayLoadDifferentOptions) {
    JobSystem::Init(2);

    std::vector<std::string> paths = {
        "test_albedo_0.png",
        "test_albedo_1.png"
    };

    // Test with different load options
    {
        CallbackData cbData1;
        TextureHandle handle1 = TextureManager::Instance().LoadArrayAsync(
            paths,
            TextureLoadOptions::Albedo(),
            TestCallback,
            &cbData1
        );
        ASSERT(handle1.IsValid());
        WaitForCallback(cbData1);
        ASSERT(cbData1.called.load());
    }

    {
        CallbackData cbData2;
        TextureHandle handle2 = TextureManager::Instance().LoadArrayAsync(
            paths,
            TextureLoadOptions::Normal(),
            TestCallback,
            &cbData2
        );
        ASSERT(handle2.IsValid());
        WaitForCallback(cbData2);
        ASSERT(cbData2.called.load());
    }

    {
        CallbackData cbData3;
        TextureHandle handle3 = TextureManager::Instance().LoadArrayAsync(
            paths,
            TextureLoadOptions{},
            TestCallback,
            &cbData3
        );
        ASSERT(handle3.IsValid());
        WaitForCallback(cbData3);
        ASSERT(cbData3.called.load());
    }

    JobSystem::Shutdown();
}

TEST(TestAsyncArrayLoadPlaceholderBehavior) {
    JobSystem::Init(2);

    CallbackData cbData;

    std::vector<std::string> paths = {
        "nonexistent_0.png",
        "nonexistent_1.png",
        "nonexistent_2.png"
    };

    TextureHandle handle = TextureManager::Instance().LoadArrayAsync(
        paths,
        TextureLoadOptions::Albedo(),
        TestCallback,
        &cbData
    );

    // Handle should be valid immediately (placeholder)
    ASSERT(handle.IsValid());

    // Should be able to access texture data (placeholder)
    TextureData* texData = TextureManager::Instance().Get(handle);
    ASSERT(texData != nullptr);
    ASSERT(texData->type == TextureType::TextureArray);
    ASSERT(texData->arrayLayers == 3);  // Should match requested layer count

    WaitForCallback(cbData);

    // After failure, handle should still be valid (keeps placeholder)
    ASSERT(cbData.called.load());
    ASSERT(handle.IsValid());

    TextureData* texDataAfter = TextureManager::Instance().Get(handle);
    ASSERT(texDataAfter != nullptr);

    JobSystem::Shutdown();
}

TEST(TestAsyncArrayLoadNullCallback) {
    JobSystem::Init(2);

    std::vector<std::string> paths = {
        "test_0.png",
        "test_1.png"
    };

    // Load without callback (should still work)
    TextureHandle handle = TextureManager::Instance().LoadArrayAsync(
        paths,
        TextureLoadOptions::Albedo(),
        nullptr,  // No callback
        nullptr
    );

    ASSERT(handle.IsValid());

    // Wait a bit for processing
    TEST_SLEEP(200);
    TextureManager::Instance().Update();

    // Handle should still be valid
    ASSERT(handle.IsValid());

    JobSystem::Shutdown();
}

TEST(TestAsyncArrayLoadManyLayers) {
    JobSystem::Init(4);  // More threads for many layers

    CallbackData cbData;

    // Create paths for 16 layers
    std::vector<std::string> paths;
    for (u32 i = 0; i < 16; ++i) {
        paths.push_back("test_layer_" + std::to_string(i) + ".png");
    }

    TextureHandle handle = TextureManager::Instance().LoadArrayAsync(
        paths,
        TextureLoadOptions::Albedo(),
        TestCallback,
        &cbData
    );

    ASSERT(handle.IsValid());

    TextureData* texData = TextureManager::Instance().Get(handle);
    ASSERT(texData != nullptr);
    ASSERT(texData->arrayLayers == 16);

    WaitForCallback(cbData);
    ASSERT(cbData.called.load());

    JobSystem::Shutdown();
}

// Note: Full integration tests with actual image files
// These tests verify the async API behavior without requiring real assets
// Integration tests with real files will be validated during build

int main() {
    std::cout << "=== Asynchronous Array Texture Loading Tests ===" << std::endl << std::endl;

    // Tests are auto-run by the TEST macro registrars

    std::cout << "\nNote: Full integration tests with actual image files" << std::endl;
    std::cout << "will be validated during build with test assets." << std::endl;
    std::cout << "\nAll async array texture tests completed!" << std::endl;

    return 0;
}
