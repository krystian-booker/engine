#pragma once

#include "ecs/ecs_coordinator.h"
#include "ecs/components/transform.h"
#include "ecs/components/light.h"
#include "ecs/components/camera.h"
#include "renderer/uniform_buffers.h"
#include "core/math.h"

#include <vector>

// Shadow cascade configuration
struct CascadeConfig {
    f32 splitLambda = 0.75f;  // Linear (0.0) to logarithmic (1.0) split scheme
    f32 cascadeSplits[kMaxCascades] = {0.1f, 0.25f, 0.5f, 1.0f};  // Normalized split distances
    u32 numCascades = 4;
};

// System responsible for calculating shadow cascade matrices and managing shadow rendering
class ShadowSystem {
public:
    ShadowSystem(ECSCoordinator* ecs);
    ~ShadowSystem();

    // Calculate shadow cascades for directional lights
    void Update(Entity cameraEntity, f32 nearPlane, f32 farPlane);

    // Get shadow uniform data
    const ShadowUniforms& GetShadowUniforms() const { return m_ShadowUniforms; }

    // Get main directional light (first light with castsShadows)
    Entity GetMainDirectionalLight() const { return m_MainDirectionalLight; }

    // Configuration
    void SetCascadeConfig(const CascadeConfig& config) { m_CascadeConfig = config; }
    const CascadeConfig& GetCascadeConfig() const { return m_CascadeConfig; }

private:
    void CalculateCascadeSplits(f32 nearPlane, f32 farPlane);
    void CalculateCascadeMatrices(const Mat4& cameraView, const Mat4& cameraProj,
                                   const Vec3& lightDir);
    std::vector<Vec4> GetFrustumCornersWorldSpace(const Mat4& viewProj);
    Mat4 CalculateLightViewMatrix(const Vec3& lightDir, const Vec3& frustumCenter);
    Mat4 CalculateLightProjMatrix(const std::vector<Vec4>& frustumCorners,
                                    const Mat4& lightView);

    ECSCoordinator* m_ECS = nullptr;
    Entity m_MainDirectionalLight = Entity::Invalid;
    CascadeConfig m_CascadeConfig;
    ShadowUniforms m_ShadowUniforms = {};
};
