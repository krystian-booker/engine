#include "camera_controller.h"
#include "ecs/components/transform.h"
#include "ecs/components/camera.h"
#include "platform/input.h"
#include "platform/window.h"
#include "core/math.h"
#include <algorithm>
#include <cmath>

CameraController::CameraController(ECSCoordinator* ecs, Window* window)
    : m_ECS(ecs), m_Window(window) {

    // Center cursor initially
    m_State.lastMousePos = Vec2(window->GetWidth() / 2.0f, window->GetHeight() / 2.0f);
}

void CameraController::Update(f32 deltaTime) {
    if (!m_ControlledCamera.IsValid()) {
        return;
    }

    HandleKeyboardInput(deltaTime);
    HandleMouseInput(deltaTime);

    // Only update orientation if user has used mouse look
    if (m_State.hasUsedMouseLook) {
        UpdateCameraOrientation();
    }
}

void CameraController::HandleKeyboardInput(f32 deltaTime) {
    if (!m_ECS->HasComponent<Transform>(m_ControlledCamera)) {
        return;
    }

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

    // Space/Shift for up/down
    if (Input::IsKeyDown(KeyCode::Space)) {
        movement += up;
    }
    if (Input::IsKeyDown(KeyCode::LeftShift)) {
        movement -= up;
    }

    // Apply movement
    if (Length(movement) > 0.001f) {
        movement = Normalize(movement);
        transform.localPosition += movement * m_State.moveSpeed * deltaTime;
        transform.MarkDirty();
    }
}

void CameraController::HandleMouseInput(f32 deltaTime) {
    (void)deltaTime;  // Unused for mouse look

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

    // Initialize yaw/pitch from camera's current rotation on first use
    if (!m_State.hasUsedMouseLook) {
        InitializeFromCurrentRotation();
        m_State.hasUsedMouseLook = true;
    }

    // Apply mouse look
    m_State.yaw += mouseDelta.x * m_State.lookSpeed;
    m_State.pitch -= mouseDelta.y * m_State.lookSpeed;  // Inverted Y

    // Clamp pitch to avoid gimbal lock
    m_State.pitch = Clamp(m_State.pitch, -89.0f, 89.0f);
}

void CameraController::UpdateCameraOrientation() {
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

void CameraController::InitializeFromCurrentRotation() {
    if (!m_ECS->HasComponent<Transform>(m_ControlledCamera)) {
        return;
    }

    Transform& transform = m_ECS->GetComponent<Transform>(m_ControlledCamera);

    // Extract forward vector from current rotation
    Vec3 forward = -Vec3(transform.worldMatrix[2]);  // -Z (left-handed)

    // Calculate yaw and pitch from forward vector
    // Yaw is rotation around Y axis
    m_State.yaw = Degrees(atan2(forward.x, -forward.z));

    // Pitch is rotation around X axis
    m_State.pitch = Degrees(asin(forward.y));
}
