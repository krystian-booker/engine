#include "editor_camera_controller.h"
#include "ecs/components/transform.h"
#include "ecs/components/camera.h"
#include "platform/input.h"
#include "platform/window.h"
#include "core/math.h"
#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include <imgui.h>
#endif

EditorCameraController::EditorCameraController(ECSCoordinator* ecs, Window* window)
    : m_ECS(ecs), m_Window(window) {

    // Center cursor initially
    m_State.lastMousePos = Vec2(window->GetWidth() / 2.0f, window->GetHeight() / 2.0f);
}

void EditorCameraController::Update(f32 deltaTime) {
    if (!m_ControlledCamera.IsValid() || !m_State.isEnabled) {
        return;
    }

    HandleKeyboardInput(deltaTime);
    HandleMouseInput(deltaTime);
    UpdateCameraOrientation();
}

void EditorCameraController::HandleKeyboardInput(f32 deltaTime) {
    if (!m_ECS->HasComponent<Transform>(m_ControlledCamera)) {
        return;
    }

#ifdef _DEBUG
    // Don't process input if ImGui wants to capture keyboard (e.g., typing in console)
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        return;
    }
#endif

    Transform& transform = m_ECS->GetComponent<Transform>(m_ControlledCamera);

    // Calculate camera vectors from current rotation
    Vec3 forward = -Vec3(transform.worldMatrix[2]);  // -Z (left-handed)
    Vec3 right = -Vec3(transform.worldMatrix[0]);    // -X (negated for left-handed)
    Vec3 up = Vec3(transform.worldMatrix[1]);        // Y

    Vec3 movement(0, 0, 0);

    // WASD movement
    if (Input::IsKeyDown(KeyCode::W)) {
        movement += forward;
    }
    if (Input::IsKeyDown(KeyCode::S)) {
        movement -= forward;
    }
    if (Input::IsKeyDown(KeyCode::A)) {
        movement -= right;
    }
    if (Input::IsKeyDown(KeyCode::D)) {
        movement += right;
    }

    // E/Q for up/down (different from game camera for editor comfort)
    if (Input::IsKeyDown(KeyCode::E)) {
        movement += up;
    }
    if (Input::IsKeyDown(KeyCode::Q)) {
        movement -= up;
    }

    // Apply movement with speed multiplier
    if (Length(movement) > 0.001f) {
        movement = Normalize(movement);

        // Use fast move speed when Shift is held
        f32 speed = Input::IsKeyDown(KeyCode::LeftShift) ? m_State.fastMoveSpeed : m_State.moveSpeed;

        transform.localPosition += movement * speed * deltaTime;
        transform.MarkDirty();
    }
}

void EditorCameraController::HandleMouseInput(f32 deltaTime) {
    (void)deltaTime;  // Unused for mouse look

#ifdef _DEBUG
    // Don't process input if ImGui wants to capture mouse (e.g., clicking on UI elements)
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        m_Window->SetCursorMode(false);  // Unlock cursor
        return;
    }
#endif

    Vec2 mousePos = Input::GetMousePosition();

    if (m_State.firstMouse) {
        m_State.lastMousePos = mousePos;
        m_State.firstMouse = false;
        return;
    }

    Vec2 mouseDelta = mousePos - m_State.lastMousePos;
    m_State.lastMousePos = mousePos;

    // Only look if right mouse button is held
    if (!Input::IsMouseButtonDown(MouseButton::Right)) {
        m_Window->SetCursorMode(false);  // Unlock cursor
        return;
    }

    // Lock and hide cursor when right mouse is held
    m_Window->SetCursorMode(true);

    // Apply mouse look
    m_State.yaw += mouseDelta.x * m_State.lookSpeed;
    m_State.pitch -= mouseDelta.y * m_State.lookSpeed;  // Inverted Y

    // Clamp pitch to avoid gimbal lock
    m_State.pitch = Clamp(m_State.pitch, -89.0f, 89.0f);
}

void EditorCameraController::UpdateCameraOrientation() {
    if (!m_ECS->HasComponent<Transform>(m_ControlledCamera)) {
        return;
    }

    Transform& transform = m_ECS->GetComponent<Transform>(m_ControlledCamera);

    // Convert yaw/pitch to quaternion
    Quat yawQuat = QuatFromAxisAngle(Vec3(0, 1, 0), Radians(m_State.yaw));
    Quat pitchQuat = QuatFromAxisAngle(Vec3(1, 0, 0), Radians(m_State.pitch));

    transform.localRotation = yawQuat * pitchQuat;
    transform.MarkDirty();
}
