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

    // Get point/spot light shadow entities
    const std::vector<Entity>& GetPointLightShadows() const { return m_PointLightShadows; }
    const std::vector<Entity>& GetSpotLightShadows() const { return m_SpotLightShadows; }

    // Configuration
    void SetCascadeConfig(const CascadeConfig& config) { m_CascadeConfig = config; }
    const CascadeConfig& GetCascadeConfig() const { return m_CascadeConfig; }

    // Shadow filtering configuration
    void SetGlobalFilterMode(ShadowFilterMode mode);
    ShadowFilterMode GetGlobalFilterMode() const { return m_GlobalFilterMode; }

    void SetShadowSearchRadius(f32 radius) { m_ShadowSearchRadius = radius; }
    f32 GetShadowSearchRadius() const { return m_ShadowSearchRadius; }

    void SetEVSMParameters(f32 positiveExp, f32 negativeExp, f32 lightBleedReduction);
    void GetEVSMParameters(f32& positiveExp, f32& negativeExp, f32& lightBleedReduction) const;

    // Per-light filter mode configuration
    void SetLightFilterMode(Entity lightEntity, ShadowFilterMode mode);
    ShadowFilterMode GetLightFilterMode(Entity lightEntity) const;

    // Debug visualization
    void SetDebugMode(u32 mode) { m_DebugMode = mode; }
    u32 GetDebugMode() const { return m_DebugMode; }

private:
    void CalculateCascadeSplits(f32 nearPlane, f32 farPlane);
    void CalculateCascadeMatrices(const Mat4& cameraView, const Mat4& cameraProj,
                                   const Vec3& lightDir);
    std::vector<Vec4> GetFrustumCornersWorldSpace(const Mat4& viewProj);
    Mat4 CalculateLightViewMatrix(const Vec3& lightDir, const Vec3& frustumCenter);
    Mat4 CalculateLightProjMatrix(const std::vector<Vec4>& frustumCorners,
                                    const Mat4& lightView);

    // Point and spot light shadow calculations
    void CalculatePointLightShadows();
    void CalculateSpotLightShadows();
    Mat4 CalculateCubeFaceViewMatrix(const Vec3& lightPos, u32 faceIndex);

    ECSCoordinator* m_ECS = nullptr;
    Entity m_MainDirectionalLight = Entity::Invalid;
    std::vector<Entity> m_PointLightShadows;
    std::vector<Entity> m_SpotLightShadows;
    CascadeConfig m_CascadeConfig;
    ShadowUniforms m_ShadowUniforms = {};

    // Shadow filtering configuration
    ShadowFilterMode m_GlobalFilterMode = ShadowFilterMode::PCF;
    f32 m_ShadowSearchRadius = 5.0f;
    f32 m_EVSMPositiveExp = 40.0f;
    f32 m_EVSMNegativeExp = 40.0f;
    f32 m_EVSMLightBleedReduction = 0.3f;

    // Debug visualization
    u32 m_DebugMode = 0;  // 0=off, 1=cascades, 2=blocker depth, 3=penumbra size
};
