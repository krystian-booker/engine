#include "core/math.h"

#include <cmath>
#include <iostream>

// Lightweight test tracking
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

static bool FloatEqual(f32 a, f32 b, f32 epsilon = 0.0001f) {
    return std::fabs(a - b) < epsilon;
}

TEST(LookAtProducesPositiveZInViewSpace) {
    const Vec3 eye(0.0f, 0.0f, -5.0f);
    const Vec3 center(0.0f, 0.0f, 0.0f);
    const Vec3 up(0.0f, 1.0f, 0.0f);
    const Mat4 view = LookAt(eye, center, up);

    const Vec4 worldOrigin(0.0f, 0.0f, 0.0f, 1.0f);
    const Vec4 viewSpace = view * worldOrigin;

    ASSERT(viewSpace.z > 0.0f);
    ASSERT(FloatEqual(viewSpace.z, 5.0f, 0.001f));
}

TEST(PerspectiveMapsNearFarToZeroOne) {
    const f32 fov = Radians(60.0f);
    const f32 aspect = 1.0f;
    const f32 nearPlane = 0.1f;
    const f32 farPlane = 100.0f;
    const Mat4 projection = Perspective(fov, aspect, nearPlane, farPlane);

    const Vec4 nearPoint(0.0f, 0.0f, nearPlane, 1.0f);
    const Vec4 farPoint(0.0f, 0.0f, farPlane, 1.0f);

    const Vec4 nearClip = projection * nearPoint;
    const Vec4 farClip = projection * farPoint;

    const f32 nearNdc = nearClip.z / nearClip.w;
    const f32 farNdc = farClip.z / farClip.w;

    ASSERT(FloatEqual(nearNdc, 0.0f, 0.0001f));
    ASSERT(FloatEqual(farNdc, 1.0f, 0.0001f));
}

int main() {
    LookAtProducesPositiveZInViewSpace_runner();
    PerspectiveMapsNearFarToZeroOne_runner();

    std::cout << "Tests run: " << testsRun << ", Passed: " << testsPassed << ", Failed: " << testsFailed << std::endl;
    return testsFailed == 0 ? 0 : 1;
}
