#pragma once

#include "core/types.h"
#include "ecs/entity.h"
#include "ecs/components/camera.h"
#include "ecs/components/transform.h"

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

    void FindActiveCamera();
    void UpdateCameraMatrices(Entity cameraEntity, u32 windowWidth, u32 windowHeight);
};
