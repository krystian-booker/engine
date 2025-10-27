#include "core/config.h"

#if ECS_ENABLE_SIGNATURES

#include "ecs/ecs_coordinator.h"
#include <iostream>

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

struct SignatureComponent {
    int value;
};

TEST(SignatureBits_UpdateOnAddRemove) {
    ECSCoordinator coordinator;
    coordinator.Init();
    coordinator.RegisterComponent<SignatureComponent>();

    Entity entity = coordinator.CreateEntity();
    coordinator.AddComponent(entity, SignatureComponent{ 7 });

    u32 bitIndex = coordinator.GetComponentRegistry()->GetComponentTypeId<SignatureComponent>();
    EntitySignature signature = coordinator.GetEntityManager()->GetSignature(entity);

    ASSERT((signature & (EntitySignature{1} << bitIndex)) != 0);

    coordinator.RemoveComponent<SignatureComponent>(entity);
    EntitySignature updated = coordinator.GetEntityManager()->GetSignature(entity);
    ASSERT((updated & (EntitySignature{1} << bitIndex)) == 0);

    coordinator.Shutdown();
}

int main() {
    std::cout << "=== ECS Signature Tests ===" << std::endl;
    std::cout << std::endl;

    SignatureBits_UpdateOnAddRemove_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}

#else

#include <iostream>

int main() {
    std::cout << "=== ECS Signature Tests ===" << std::endl;
    std::cout << "Signatures disabled at compile time - skipping tests." << std::endl;
    return 0;
}

#endif
