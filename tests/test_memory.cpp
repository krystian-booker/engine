#include "core/memory.h"
#include "platform/platform.h"
#include <iostream>
#include <cstring>

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

// ============================================================================
// LinearAllocator Tests
// ============================================================================

TEST(LinearAllocator_BasicAllocation) {
    LinearAllocator allocator;
    allocator.Init(1024);

    void* ptr1 = allocator.Alloc(64);
    ASSERT(ptr1 != nullptr);

    void* ptr2 = allocator.Alloc(128);
    ASSERT(ptr2 != nullptr);
    ASSERT(ptr2 != ptr1);

    // Check that pointers are in order
    ASSERT(reinterpret_cast<uintptr_t>(ptr2) > reinterpret_cast<uintptr_t>(ptr1));

    allocator.Shutdown();
}

TEST(LinearAllocator_AlignedAllocation_16Byte) {
    LinearAllocator allocator;
    allocator.Init(4096);

    // Allocate with 16-byte alignment
    void* ptr = allocator.Alloc(100, 16);
    ASSERT(ptr != nullptr);

    // Check alignment
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    ASSERT((addr & 15) == 0); // Must be 16-byte aligned

    allocator.Shutdown();
}

TEST(LinearAllocator_AlignedAllocation_64Byte) {
    LinearAllocator allocator;
    allocator.Init(4096);

    // Allocate unaligned first to test alignment correction
    void* ptr1 = allocator.Alloc(1, 1);
    ASSERT(ptr1 != nullptr);

    // Allocate with 64-byte alignment
    void* ptr2 = allocator.Alloc(100, 64);
    ASSERT(ptr2 != nullptr);

    // Check alignment
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr2);
    ASSERT((addr & 63) == 0); // Must be 64-byte aligned

    allocator.Shutdown();
}

TEST(LinearAllocator_HighWaterMark) {
    LinearAllocator allocator;
    allocator.Init(1024);

    ASSERT(allocator.GetHighWaterMark() == 0);

    allocator.Alloc(100);
    size_t mark1 = allocator.GetHighWaterMark();
    ASSERT(mark1 >= 100);

    allocator.Alloc(200);
    size_t mark2 = allocator.GetHighWaterMark();
    ASSERT(mark2 >= mark1 + 200);
    ASSERT(mark2 >= 300);

    // Reset should not clear high-water mark
    allocator.Reset();
    ASSERT(allocator.GetHighWaterMark() == mark2);

    // Smaller allocation shouldn't change high-water mark
    allocator.Alloc(50);
    ASSERT(allocator.GetHighWaterMark() == mark2);

    allocator.Shutdown();
}

TEST(LinearAllocator_Reset) {
    LinearAllocator allocator;
    allocator.Init(1024);

    void* ptr1 = allocator.Alloc(100);
    size_t offset1 = allocator.GetCurrentOffset();
    ASSERT(offset1 >= 100);

    allocator.Reset();
    ASSERT(allocator.GetCurrentOffset() == 0);

    // Should be able to allocate again from the start
    void* ptr2 = allocator.Alloc(100);
    ASSERT(ptr2 == ptr1); // Same memory location

    allocator.Shutdown();
}

TEST(LinearAllocator_MultiFrameSimulation) {
    LinearAllocator allocator;
    allocator.Init(4096);

    // Simulate 10 frames
    for (int frame = 0; frame < 10; frame++) {
        // Allocate varying amounts per frame
        for (int i = 0; i < frame + 1; i++) {
            void* ptr = allocator.Alloc(64);
            ASSERT(ptr != nullptr);

            // Write some data to ensure memory is valid
            memset(ptr, 0xAB, 64);
        }

        // Reset for next frame
        allocator.Reset();
    }

    // High-water mark should reflect peak usage (frame 9 had 10 allocations)
    ASSERT(allocator.GetHighWaterMark() >= 640);

    allocator.Shutdown();
}

TEST(LinearAllocator_OverflowDetection) {
    LinearAllocator allocator;
    allocator.Init(256);

    // Fill up the allocator
    void* ptr1 = allocator.Alloc(200);
    ASSERT(ptr1 != nullptr);

    // This should fail (overflow)
    void* ptr2 = allocator.Alloc(100);
    ASSERT(ptr2 == nullptr);

    allocator.Shutdown();
}

// ============================================================================
// PoolAllocator Tests
// ============================================================================

struct TestComponent {
    i32 id;
    f32 data[4];
};

TEST(PoolAllocator_BasicAllocation) {
    PoolAllocator<TestComponent> pool;

    TestComponent* comp1 = pool.Alloc();
    ASSERT(comp1 != nullptr);

    comp1->id = 42;
    ASSERT(comp1->id == 42);

    TestComponent* comp2 = pool.Alloc();
    ASSERT(comp2 != nullptr);
    ASSERT(comp2 != comp1);

    comp2->id = 100;
    ASSERT(comp1->id == 42); // First component unchanged
    ASSERT(comp2->id == 100);
}

TEST(PoolAllocator_FreeAndReuse) {
    PoolAllocator<TestComponent> pool;

    TestComponent* comp1 = pool.Alloc();
    ASSERT(comp1 != nullptr);

    TestComponent* comp2 = pool.Alloc();
    ASSERT(comp2 != nullptr);

    // Free the first component
    pool.Free(comp1);

    // Next allocation should reuse the freed slot
    TestComponent* comp3 = pool.Alloc();
    ASSERT(comp3 == comp1); // Should be same address

    ASSERT(comp2 != comp3); // comp2 still valid
}

TEST(PoolAllocator_MultiBlockGrowth) {
    PoolAllocator<TestComponent, 4> pool; // Small block size for testing

    TestComponent* components[20];

    // Allocate more than one block's worth
    for (int i = 0; i < 20; i++) {
        components[i] = pool.Alloc();
        ASSERT(components[i] != nullptr);
        components[i]->id = i;
    }

    // Verify all components are unique
    for (int i = 0; i < 20; i++) {
        for (int j = i + 1; j < 20; j++) {
            ASSERT(components[i] != components[j]);
        }
    }

    // Verify data integrity
    for (int i = 0; i < 20; i++) {
        ASSERT(components[i]->id == i);
    }
}

TEST(PoolAllocator_FreelistCorrectness) {
    PoolAllocator<TestComponent, 8> pool;

    TestComponent* components[8];

    // Fill first block
    for (int i = 0; i < 8; i++) {
        components[i] = pool.Alloc();
        ASSERT(components[i] != nullptr);
    }

    // Free every other component
    pool.Free(components[1]);
    pool.Free(components[3]);
    pool.Free(components[5]);
    pool.Free(components[7]);

    // Allocate 4 more - should reuse freed slots
    TestComponent* reused[4];
    for (int i = 0; i < 4; i++) {
        reused[i] = pool.Alloc();
        ASSERT(reused[i] != nullptr);
    }

    // Check that we reused the freed slots
    bool found[4] = {false, false, false, false};
    for (int i = 0; i < 4; i++) {
        if (reused[i] == components[1]) found[0] = true;
        if (reused[i] == components[3]) found[1] = true;
        if (reused[i] == components[5]) found[2] = true;
        if (reused[i] == components[7]) found[3] = true;
    }

    ASSERT(found[0] && found[1] && found[2] && found[3]);
}

TEST(PoolAllocator_GenerationCounter) {
    PoolAllocator<TestComponent> pool;

    u32 gen0 = pool.GetGeneration();
    ASSERT(gen0 == 0);

    TestComponent* comp = pool.Alloc();
    ASSERT(comp != nullptr);

    pool.Free(comp);
    u32 gen1 = pool.GetGeneration();
    ASSERT(gen1 == 1);

    pool.Free(comp); // Free again (bad practice but tests counter)
    u32 gen2 = pool.GetGeneration();
    ASSERT(gen2 == 2);
}

TEST(PoolAllocator_LargeAllocation) {
    PoolAllocator<TestComponent, 64> pool;

    TestComponent* components[200];

    // Allocate a large number of components
    for (int i = 0; i < 200; i++) {
        components[i] = pool.Alloc();
        ASSERT(components[i] != nullptr);
        components[i]->id = i;
    }

    // Free half of them
    for (int i = 0; i < 100; i++) {
        pool.Free(components[i * 2]);
    }

    // Reallocate - should reuse freed slots
    for (int i = 0; i < 100; i++) {
        TestComponent* comp = pool.Alloc();
        ASSERT(comp != nullptr);
        comp->id = 1000 + i;
    }

    // Verify remaining original components
    for (int i = 0; i < 100; i++) {
        ASSERT(components[i * 2 + 1]->id == i * 2 + 1);
    }
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    std::cout << "=== Memory Allocator Unit Tests ===" << std::endl;
    std::cout << std::endl;

    std::cout << "--- LinearAllocator Tests ---" << std::endl;
    LinearAllocator_BasicAllocation_runner();
    LinearAllocator_AlignedAllocation_16Byte_runner();
    LinearAllocator_AlignedAllocation_64Byte_runner();
    LinearAllocator_HighWaterMark_runner();
    LinearAllocator_Reset_runner();
    LinearAllocator_MultiFrameSimulation_runner();
    LinearAllocator_OverflowDetection_runner();

    std::cout << std::endl;
    std::cout << "--- PoolAllocator Tests ---" << std::endl;
    PoolAllocator_BasicAllocation_runner();
    PoolAllocator_FreeAndReuse_runner();
    PoolAllocator_MultiBlockGrowth_runner();
    PoolAllocator_FreelistCorrectness_runner();
    PoolAllocator_GenerationCounter_runner();
    PoolAllocator_LargeAllocation_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
