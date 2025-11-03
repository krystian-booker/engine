#pragma once

#include "ecs/ecs_coordinator.h"
#include "ecs/components/transform.h"
#include "ecs/components/light.h"
#include "ecs/components/camera.h"
#include "renderer/uniform_buffers.h"
#include "core/math.h"

#include <vector>

// System responsible for collecting lights from the ECS and preparing lighting data for the GPU
class LightingSystem {
public:
    LightingSystem(ECSCoordinator* ecs);
    ~LightingSystem();

    // Update lighting data from ECS (call once per frame before rendering)
    void Update();

    // Get lighting uniform buffer data (ready for GPU upload)
    const LightingUniformBuffer& GetLightingData() const { return m_LightingData; }

    // Set active camera for lighting calculations
    void SetActiveCamera(Entity cameraEntity);

private:
    ECSCoordinator* m_ECS = nullptr;
    Entity m_ActiveCamera = Entity::Invalid;
    LightingUniformBuffer m_LightingData = {};
};
