#pragma once
#include "ecs/ecs_coordinator.h"
#include "core/types.h"
#include "core/math.h"

class Window;

struct CameraControllerState {
    f32 moveSpeed = 5.0f;
    f32 lookSpeed = 0.1f;
    f32 yaw = -90.0f;    // Looking down -Z
    f32 pitch = 0.0f;
    bool firstMouse = true;
    bool hasUsedMouseLook = false;  // Track if user has used mouse look
    Vec2 lastMousePos{0, 0};
};

class CameraController {
public:
    CameraController(ECSCoordinator* ecs, Window* window);

    void Update(f32 deltaTime);

    void SetControlledCamera(Entity camera) { m_ControlledCamera = camera; }
    Entity GetControlledCamera() const { return m_ControlledCamera; }

private:
    ECSCoordinator* m_ECS;
    Window* m_Window;
    Entity m_ControlledCamera = Entity::Invalid;

    CameraControllerState m_State;

    void HandleKeyboardInput(f32 deltaTime);
    void HandleMouseInput(f32 deltaTime);
    void UpdateCameraOrientation();
    void InitializeFromCurrentRotation();
};
