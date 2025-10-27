#include "core/math.h"
#include "core/types.h"
#include "ecs/components/camera.h"
#include "ecs/components/transform.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/systems/camera_system.h"

#include <algorithm>
#include <cmath>
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
        return; \
    }

static bool FloatEqual(f32 a, f32 b, f32 epsilon = 0.0001f) {
    return std::fabs(a - b) < epsilon;
}

static bool Mat4Equal(const Mat4& a, const Mat4& b, f32 epsilon = 0.0001f) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (!FloatEqual(a[i][j], b[i][j], epsilon)) {
                return false;
            }
        }
    }
    return true;
}

TEST(CameraSystem_ComputesPerspectiveProjection) {
    ECSCoordinator ecs;
    ecs.Init();

    CameraSystem* cameraSystem = ecs.GetCameraSystem();
    ASSERT(cameraSystem != nullptr);

    Entity cameraEntity = ecs.CreateEntity();

    Transform transform{};
    transform.localPosition = Vec3(0.0f, 0.0f, 0.0f);
    transform.MarkDirty();
    ecs.AddComponent(cameraEntity, transform);

    Camera camera{};
    camera.isActive = true;
    camera.fov = 75.0f;
    camera.nearPlane = 0.5f;
    camera.farPlane = 250.0f;
    ecs.AddComponent(cameraEntity, camera);

    ecs.Update(0.0f);
    cameraSystem->Update(1920, 1080);

    const Camera& storedCamera = ecs.GetComponent<Camera>(cameraEntity);

    ASSERT(FloatEqual(storedCamera.aspectRatio, 1920.0f / 1080.0f));

    Mat4 expectedProjection = Perspective(Radians(storedCamera.fov), storedCamera.aspectRatio, camera.nearPlane, camera.farPlane);
    expectedProjection[1][1] *= -1.0f;

    ASSERT(Mat4Equal(storedCamera.projectionMatrix, expectedProjection));

    ecs.Shutdown();
}

TEST(CameraSystem_ComputesViewFromTransform) {
    ECSCoordinator ecs;
    ecs.Init();

    CameraSystem* cameraSystem = ecs.GetCameraSystem();
    ASSERT(cameraSystem != nullptr);

    Entity cameraEntity = ecs.CreateEntity();

    Transform transform{};
    transform.localPosition = Vec3(2.0f, 3.0f, -5.0f);
    transform.localRotation = QuatFromAxisAngle(Vec3(0.0f, 1.0f, 0.0f), Radians(90.0f));
    transform.localScale = Vec3(1.0f, 1.0f, 1.0f);
    transform.MarkDirty();
    ecs.AddComponent(cameraEntity, transform);

    Camera camera{};
    camera.isActive = true;
    ecs.AddComponent(cameraEntity, camera);

    ecs.Update(0.0f);
    cameraSystem->Update(1280, 720);

    const Transform& updatedTransform = ecs.GetComponent<Transform>(cameraEntity);
    const Camera& updatedCamera = ecs.GetComponent<Camera>(cameraEntity);

    Vec3 position = Vec3(updatedTransform.worldMatrix[3]);
    Vec3 forward = -Vec3(updatedTransform.worldMatrix[2]);
    Vec3 up = Vec3(updatedTransform.worldMatrix[1]);
    forward = Normalize(forward);
    up = Normalize(up);

    Mat4 expectedView = LookAt(position, position + forward, up);
    ASSERT(Mat4Equal(updatedCamera.viewMatrix, expectedView));

    ecs.Shutdown();
}

TEST(CameraSystem_ActiveCameraUniqueness) {
    ECSCoordinator ecs;
    ecs.Init();

    CameraSystem* cameraSystem = ecs.GetCameraSystem();
    ASSERT(cameraSystem != nullptr);

    Entity cameraA = ecs.CreateEntity();
    Entity cameraB = ecs.CreateEntity();

    Transform transform{};
    transform.MarkDirty();

    Camera camera{};
    camera.isActive = true;

    ecs.AddComponent(cameraA, transform);
    ecs.AddComponent(cameraA, camera);

    ecs.AddComponent(cameraB, transform);
    ecs.AddComponent(cameraB, camera);

    ecs.Update(0.0f);
    cameraSystem->Update(1024, 768);

    ASSERT(cameraSystem->GetActiveCamera() == cameraA);
    ASSERT(ecs.GetComponent<Camera>(cameraA).isActive);
    ASSERT(!ecs.GetComponent<Camera>(cameraB).isActive);

    cameraSystem->SetActiveCamera(cameraB);
    ASSERT(cameraSystem->GetActiveCamera() == cameraB);
    ASSERT(!ecs.GetComponent<Camera>(cameraA).isActive);
    ASSERT(ecs.GetComponent<Camera>(cameraB).isActive);

    cameraSystem->SetActiveCamera(Entity::Invalid);
    ASSERT(!cameraSystem->GetActiveCamera().IsValid());
    ASSERT(!ecs.GetComponent<Camera>(cameraA).isActive);
    ASSERT(!ecs.GetComponent<Camera>(cameraB).isActive);

    ecs.Shutdown();
}

TEST(CameraSystem_OrthographicClampAndAspect) {
    ECSCoordinator ecs;
    ecs.Init();

    CameraSystem* cameraSystem = ecs.GetCameraSystem();
    ASSERT(cameraSystem != nullptr);

    Entity cameraEntity = ecs.CreateEntity();

    Transform transform{};
    transform.MarkDirty();
    ecs.AddComponent(cameraEntity, transform);

    Camera camera{};
    camera.isActive = true;
    camera.projection = CameraProjection::Orthographic;
    camera.orthoSize = 0.0f;
    camera.nearPlane = -5.0f;
    camera.farPlane = 0.0f;
    ecs.AddComponent(cameraEntity, camera);

    ecs.Update(0.0f);
    cameraSystem->Update(800, 0);

    const Camera& storedCamera = ecs.GetComponent<Camera>(cameraEntity);

    ASSERT(FloatEqual(storedCamera.aspectRatio, 800.0f));

    const f32 expectedNear = std::max(camera.nearPlane, 0.0001f);
    const f32 expectedFar = std::max(camera.farPlane, expectedNear + 0.001f);
    const f32 halfSize = std::max(camera.orthoSize * 0.5f, 0.0001f);
    const f32 halfWidth = halfSize * storedCamera.aspectRatio;

    Mat4 expectedProjection = Ortho(-halfWidth, halfWidth, -halfSize, halfSize, expectedNear, expectedFar);
    ASSERT(Mat4Equal(storedCamera.projectionMatrix, expectedProjection));

    ecs.Shutdown();
}

int main() {
    std::cout << "=== Camera System Tests ===" << std::endl << std::endl;

    CameraSystem_ComputesPerspectiveProjection_runner();
    CameraSystem_ComputesViewFromTransform_runner();
    CameraSystem_ActiveCameraUniqueness_runner();
    CameraSystem_OrthographicClampAndAspect_runner();

    std::cout << std::endl
              << "Tests passed: " << testsPassed << "/" << testsRun
              << " | Failed: " << testsFailed << std::endl;

    return testsFailed == 0 ? 0 : 1;
}
