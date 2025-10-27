#include "ecs/systems/camera_system.h"

#include "ecs/ecs_coordinator.h"

#include <algorithm>
#include <iostream>

namespace {
constexpr f32 kMinNearPlane = 0.0001f;
constexpr f32 kMinFarOffset = 0.001f;
constexpr f32 kMinFovDegrees = 1.0f;
constexpr f32 kMaxFovDegrees = 179.0f;
} // namespace

CameraSystem::CameraSystem(ECSCoordinator* ecs)
    : m_ECS(ecs) {
}

void CameraSystem::Update(u32 windowWidth, u32 windowHeight) {
    if (!m_ECS) {
        m_ActiveCamera = Entity::Invalid;
        return;
    }

    // Validate existing active camera (entity alive, has required components, still marked active)
    if (m_ActiveCamera.IsValid()) {
        if (!m_ECS->IsEntityAlive(m_ActiveCamera) || !m_ECS->HasComponent<Camera>(m_ActiveCamera) ||
            !m_ECS->HasComponent<Transform>(m_ActiveCamera)) {
            m_ActiveCamera = Entity::Invalid;
        } else {
            Camera& activeCamera = m_ECS->GetComponent<Camera>(m_ActiveCamera);
            if (!activeCamera.isActive) {
                m_ActiveCamera = Entity::Invalid;
            }
        }
    }

    // Re-scan all cameras to find the active one (handles invalidated camera or first-time setup)
    FindActiveCamera();

    if (!m_ActiveCamera.IsValid()) {
        return;
    }

    UpdateCameraMatrices(m_ActiveCamera, windowWidth, windowHeight);
}

Mat4 CameraSystem::GetViewMatrix() const {
    if (!m_ECS || !m_ActiveCamera.IsValid() || !m_ECS->HasComponent<Camera>(m_ActiveCamera)) {
        return Mat4(1.0f);
    }

    const Camera& camera = m_ECS->GetComponent<Camera>(m_ActiveCamera);
    return camera.viewMatrix;
}

Mat4 CameraSystem::GetProjectionMatrix() const {
    if (!m_ECS || !m_ActiveCamera.IsValid() || !m_ECS->HasComponent<Camera>(m_ActiveCamera)) {
        return Mat4(1.0f);
    }

    const Camera& camera = m_ECS->GetComponent<Camera>(m_ActiveCamera);
    return camera.projectionMatrix;
}

Vec4 CameraSystem::GetClearColor() const {
    if (!m_ECS || !m_ActiveCamera.IsValid() || !m_ECS->HasComponent<Camera>(m_ActiveCamera)) {
        return Vec4(0.1f, 0.1f, 0.1f, 1.0f);
    }

    const Camera& camera = m_ECS->GetComponent<Camera>(m_ActiveCamera);
    return camera.clearColor;
}

void CameraSystem::SetActiveCamera(Entity entity) {
    if (!m_ECS) {
        m_ActiveCamera = Entity::Invalid;
        return;
    }

    const bool canActivate =
        entity.IsValid() && m_ECS->IsEntityAlive(entity) && m_ECS->HasComponent<Camera>(entity);

    if (!canActivate) {
        m_ECS->ForEach<Camera>([](Entity /*unused*/, Camera& camera) {
            camera.isActive = false;
        });
        m_ActiveCamera = Entity::Invalid;
        return;
    }

    m_ECS->ForEach<Camera>([&](Entity other, Camera& camera) {
        camera.isActive = (other == entity);
    });

    m_ActiveCamera = entity;
}

void CameraSystem::FindActiveCamera() {
    if (!m_ECS) {
        m_ActiveCamera = Entity::Invalid;
        return;
    }

    Entity found = Entity::Invalid;

    m_ECS->ForEach<Camera>([&](Entity entity, Camera& camera) {
        if (!camera.isActive) {
            return;
        }

        if (!found.IsValid()) {
            found = entity;
            return;
        }

        if (entity != found) {
            camera.isActive = false;
            // Only warn once per entity to prevent console spam
            if (m_WarnedMultipleCameras.find(entity.index) == m_WarnedMultipleCameras.end()) {
                std::cerr << "CameraSystem: Multiple active cameras detected; "
                             "disabling camera at index "
                          << entity.index << std::endl;
                m_WarnedMultipleCameras.insert(entity.index);
            }
        }
    });

    // Clear warning set when active camera changes (allows re-warning if situation recurs)
    if (found != m_ActiveCamera) {
        m_WarnedMultipleCameras.clear();
    }

    m_ActiveCamera = found;
}

void CameraSystem::UpdateCameraMatrices(Entity cameraEntity, u32 windowWidth, u32 windowHeight) {
    if (!m_ECS->HasComponent<Camera>(cameraEntity)) {
        return;
    }

    if (!m_ECS->HasComponent<Transform>(cameraEntity)) {
        std::cerr << "CameraSystem: Active camera missing Transform component (entity index "
                  << cameraEntity.index << ")" << std::endl;
        return;
    }

    Camera& camera = m_ECS->GetComponent<Camera>(cameraEntity);
    Transform& transform = m_ECS->GetComponent<Transform>(cameraEntity);

    const u32 safeWidth = windowWidth == 0 ? 1 : windowWidth;
    const u32 safeHeight = windowHeight == 0 ? 1 : windowHeight;
    camera.aspectRatio = static_cast<f32>(safeWidth) / static_cast<f32>(safeHeight);

    const f32 clampedFov = std::clamp(camera.fov, kMinFovDegrees, kMaxFovDegrees);
    const f32 nearPlane = std::max(camera.nearPlane, kMinNearPlane);
    const f32 farPlane = std::max(camera.farPlane, nearPlane + kMinFarOffset);

    if (camera.projection == CameraProjection::Perspective) {
        camera.projectionMatrix = Perspective(
            Radians(clampedFov),
            camera.aspectRatio,
            nearPlane,
            farPlane);
        camera.projectionMatrix[1][1] *= -1.0f;
    } else {
        const f32 halfSize = std::max(camera.orthoSize * 0.5f, 0.0001f);
        const f32 halfWidth = halfSize * camera.aspectRatio;
        camera.projectionMatrix = Ortho(
            -halfWidth,
            halfWidth,
            -halfSize,
            halfSize,
            nearPlane,
            farPlane);
    }

    Vec3 position = Vec3(transform.worldMatrix[3]);
    Vec3 forward = -Vec3(transform.worldMatrix[2]);
    Vec3 up = Vec3(transform.worldMatrix[1]);

    if (Length(forward) == 0.0f) {
        forward = Vec3(0.0f, 0.0f, -1.0f);
    }
    if (Length(up) == 0.0f) {
        up = Vec3(0.0f, 1.0f, 0.0f);
    }

    forward = Normalize(forward);
    up = Normalize(up);

    camera.viewMatrix = LookAt(position, position + forward, up);
}
