#pragma once

#include "core/types.h"
#include "ecs/entity.h"
#include "ecs/components/camera.h"
#include "ecs/components/transform.h"
#include <unordered_set>

class ECSCoordinator;

class CameraSystem {
public:
    explicit CameraSystem(ECSCoordinator* ecs);

    void Update(u32 windowWidth, u32 windowHeight);

    Entity GetActiveCamera() const { return m_ActiveCamera; }
    Mat4 GetViewMatrix() const;
    Mat4 GetProjectionMatrix() const;
    Vec4 GetClearColor() const;

    void SetActiveCamera(Entity entity);

private:
    ECSCoordinator* m_ECS = nullptr;
    Entity m_ActiveCamera = Entity::Invalid;
    std::unordered_set<u32> m_WarnedMultipleCameras;  // Track entities we've warned about to prevent spam

    void FindActiveCamera();
    void UpdateCameraMatrices(Entity cameraEntity, u32 windowWidth, u32 windowHeight);
};
