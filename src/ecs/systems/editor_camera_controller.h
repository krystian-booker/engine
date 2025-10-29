#pragma once
#include "ecs/ecs_coordinator.h"
#include "core/types.h"
#include "core/math.h"

class Window;

struct EditorCameraState {
    f32 moveSpeed = 15.0f;       // Faster than game camera
    f32 fastMoveSpeed = 30.0f;   // Speed when Shift is held
    f32 lookSpeed = 0.15f;       // Slightly faster look
    f32 yaw = -90.0f;            // Looking down -Z
    f32 pitch = 0.0f;
    bool firstMouse = true;
    Vec2 lastMousePos{0, 0};
    bool isEnabled = true;       // Only update when scene viewport is focused
};

// Editor camera controller for free-flying scene view camera
// Only active when scene viewport window is focused
class EditorCameraController {
public:
    EditorCameraController(ECSCoordinator* ecs, Window* window);

    void Update(f32 deltaTime);

    void SetControlledCamera(Entity camera) { m_ControlledCamera = camera; }
    Entity GetControlledCamera() const { return m_ControlledCamera; }

    void SetEnabled(bool enabled) { m_State.isEnabled = enabled; }
    bool IsEnabled() const { return m_State.isEnabled; }

private:
    ECSCoordinator* m_ECS;
    Window* m_Window;
    Entity m_ControlledCamera = Entity::Invalid;

    EditorCameraState m_State;

    void HandleKeyboardInput(f32 deltaTime);
    void HandleMouseInput(f32 deltaTime);
    void UpdateCameraOrientation();
};
