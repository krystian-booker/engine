#include "shadow_system.h"
#include <algorithm>
#include <cmath>

ShadowSystem::ShadowSystem(ECSCoordinator* ecs)
    : m_ECS(ecs) {
    // Set default shadow parameters
    m_ShadowUniforms.shadowParams = Vec4(0.005f, 2.0f, 0.0f, 0.0f);  // bias, PCF radius
    m_ShadowUniforms.numPointLightShadows = 0;
    m_ShadowUniforms.numSpotLightShadows = 0;
}

ShadowSystem::~ShadowSystem() = default;

void ShadowSystem::Update(Entity cameraEntity, f32 nearPlane, f32 farPlane) {
    if (!m_ECS) {
        return;
    }

    // Find main directional light (first one with castsShadows)
    m_MainDirectionalLight = Entity::Invalid;

    m_ECS->ForEach<Transform, Light>([this](Entity entity, Transform& /* transform */, Light& light) {
        if (light.type == LightType::Directional && light.castsShadows) {
            if (!m_MainDirectionalLight.IsValid()) {
                m_MainDirectionalLight = entity;
            }
        }
    });

    if (!m_MainDirectionalLight.IsValid() || !cameraEntity.IsValid()) {
        m_ShadowUniforms.cascadeSplits.w = 0.0f;  // No cascades
        return;
    }

    // Get camera matrices
    if (!m_ECS->HasComponent<Camera>(cameraEntity) || !m_ECS->HasComponent<Transform>(cameraEntity)) {
        return;
    }

    Camera& camera = m_ECS->GetComponent<Camera>(cameraEntity);
    //Transform& cameraTransform = m_ECS->GetComponent<Transform>(cameraEntity);  // Not currently used

    Mat4 cameraView = camera.viewMatrix;
    Mat4 cameraProj = camera.projectionMatrix;

    // Get light direction
    Transform& lightTransform = m_ECS->GetComponent<Transform>(m_MainDirectionalLight);
    Vec4 forwardLocal(0.0f, 0.0f, -1.0f, 0.0f);
    Vec4 forwardWorld = lightTransform.worldMatrix * forwardLocal;
    Vec3 lightDir = Normalize(Vec3(forwardWorld.x, forwardWorld.y, forwardWorld.z));

    // Calculate cascade splits
    CalculateCascadeSplits(nearPlane, farPlane);

    // Calculate shadow matrices for each cascade
    CalculateCascadeMatrices(cameraView, cameraProj, lightDir);

    m_ShadowUniforms.cascadeSplits.w = static_cast<f32>(m_CascadeConfig.numCascades);

    // Update shadow filtering parameters
    Light& mainLight = m_ECS->GetComponent<Light>(m_MainDirectionalLight);
    m_ShadowUniforms.shadowParams.z = static_cast<f32>(mainLight.shadowFilterMode);
    m_ShadowUniforms.shadowParams.w = mainLight.shadowSearchRadius;

    // Update EVSM parameters
    m_ShadowUniforms.evsmParams.x = mainLight.evsmPositiveExponent;
    m_ShadowUniforms.evsmParams.y = mainLight.evsmNegativeExponent;
    m_ShadowUniforms.evsmParams.z = mainLight.evsmLightBleedReduction;
    m_ShadowUniforms.evsmParams.w = 0.0f;

    // Update debug parameters
    m_ShadowUniforms.debugParams.x = static_cast<f32>(m_DebugMode);
    m_ShadowUniforms.debugParams.y = 0.0f;
    m_ShadowUniforms.debugParams.z = 0.0f;
    m_ShadowUniforms.debugParams.w = 0.0f;

    // Calculate point and spot light shadows
    CalculatePointLightShadows();
    CalculateSpotLightShadows();
}

void ShadowSystem::CalculateCascadeSplits(f32 nearPlane, f32 farPlane) {
    f32 range = farPlane - nearPlane;
    f32 ratio = farPlane / nearPlane;

    for (u32 i = 0; i < m_CascadeConfig.numCascades; ++i) {
        f32 p = static_cast<f32>(i + 1) / static_cast<f32>(m_CascadeConfig.numCascades);

        // Logarithmic split
        f32 log = nearPlane * std::pow(ratio, p);

        // Linear split
        f32 linear = nearPlane + range * p;

        // Blend between linear and logarithmic using lambda
        f32 d = m_CascadeConfig.splitLambda * (log - linear) + linear;

        // Store absolute distance
        if (i < 3) {  // Only first 3 splits (4th is always far plane)
            m_ShadowUniforms.cascadeSplits[i] = d;
        }
    }

    // Last split is always the far plane
    m_ShadowUniforms.cascadeSplits[kMaxCascades - 1] = farPlane;
}

void ShadowSystem::CalculateCascadeMatrices(const Mat4& cameraView, const Mat4& cameraProj,
                                            const Vec3& lightDir) {
    f32 lastSplitDist = 0.0f;

    for (u32 cascade = 0; cascade < m_CascadeConfig.numCascades; ++cascade) {
        f32 splitDist = m_ShadowUniforms.cascadeSplits[cascade];

        // Create sub-frustum projection matrix for this cascade
        Mat4 cascadeProj = cameraProj;
        // Update near and far planes for this cascade
        // This is a simplified approach - proper implementation would modify projection matrix

        // Get frustum corners for this cascade in world space
        Mat4 invViewProj = glm::inverse(cascadeProj * cameraView);
        std::vector<Vec4> frustumCorners = GetFrustumCornersWorldSpace(invViewProj);

        // Calculate frustum center
        Vec3 frustumCenter(0.0f);
        for (const auto& corner : frustumCorners) {
            frustumCenter += Vec3(corner.x, corner.y, corner.z);
        }
        frustumCenter /= static_cast<f32>(frustumCorners.size());

        // Calculate light view matrix looking at frustum center
        Mat4 lightView = CalculateLightViewMatrix(lightDir, frustumCenter);

        // Calculate tight orthographic projection matrix
        Mat4 lightProj = CalculateLightProjMatrix(frustumCorners, lightView);

        // Store combined view-projection matrix
        m_ShadowUniforms.cascadeViewProj[cascade] = lightProj * lightView;

        lastSplitDist = splitDist;
    }
}

std::vector<Vec4> ShadowSystem::GetFrustumCornersWorldSpace(const Mat4& viewProj) {
    std::vector<Vec4> corners;
    corners.reserve(8);

    // NDC cube corners
    for (u32 x = 0; x < 2; ++x) {
        for (u32 y = 0; y < 2; ++y) {
            for (u32 z = 0; z < 2; ++z) {
                Vec4 pt = viewProj * Vec4(
                    2.0f * x - 1.0f,
                    2.0f * y - 1.0f,
                    2.0f * z - 1.0f,
                    1.0f
                );
                corners.push_back(pt / pt.w);
            }
        }
    }

    return corners;
}

Mat4 ShadowSystem::CalculateLightViewMatrix(const Vec3& lightDir, const Vec3& frustumCenter) {
    // Position light looking at frustum center
    // Use arbitrary "up" vector, adjust if parallel to light direction
    Vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(Dot(lightDir, up)) > 0.99f) {
        up = Vec3(1.0f, 0.0f, 0.0f);
    }

    return glm::lookAt(frustumCenter - lightDir * 10.0f, frustumCenter, up);
}

Mat4 ShadowSystem::CalculateLightProjMatrix(const std::vector<Vec4>& frustumCorners,
                                             const Mat4& lightView) {
    // Transform frustum corners to light space
    Vec3 minExtents(std::numeric_limits<f32>::max());
    Vec3 maxExtents(std::numeric_limits<f32>::lowest());

    for (const auto& corner : frustumCorners) {
        Vec4 lightSpaceCorner = lightView * corner;
        Vec3 lsc(lightSpaceCorner.x, lightSpaceCorner.y, lightSpaceCorner.z);

        minExtents = glm::min(minExtents, lsc);
        maxExtents = glm::max(maxExtents, lsc);
    }

    // Add padding to reduce edge artifacts
    f32 padding = 2.0f;
    minExtents -= Vec3(padding);
    maxExtents += Vec3(padding);

    // Create orthographic projection
    return glm::ortho(minExtents.x, maxExtents.x,
                      minExtents.y, maxExtents.y,
                      minExtents.z, maxExtents.z);
}

void ShadowSystem::CalculatePointLightShadows() {
    m_PointLightShadows.clear();
    m_ShadowUniforms.numPointLightShadows = 0;

    u32 shadowIndex = 0;

    // Find all point lights with castsShadows enabled
    m_ECS->ForEach<Transform, Light>([this, &shadowIndex](Entity entity, Transform& transform, Light& light) {
        if (light.type == LightType::Point && light.castsShadows && shadowIndex < kMaxPointLightShadows) {
            m_PointLightShadows.push_back(entity);

            // Get light position from world matrix
            Vec3 lightPos(transform.worldMatrix[3][0], transform.worldMatrix[3][1], transform.worldMatrix[3][2]);

            // Create 6 view-projection matrices for each cube face
            // Use 90 degree FOV and aspect ratio of 1.0 for cubemap faces
            f32 nearPlane = 0.1f;
            f32 farPlane = light.range > 0.0f ? light.range : 25.0f;
            Mat4 projection = glm::perspective(Radians(90.0f), 1.0f, nearPlane, farPlane);

            // Store light position and far plane
            m_ShadowUniforms.pointLightShadows[shadowIndex].lightPosAndFar = Vec4(lightPos, farPlane);

            // Calculate view matrix for each cube face
            // Order: +X, -X, +Y, -Y, +Z, -Z
            for (u32 face = 0; face < 6; ++face) {
                Mat4 view = CalculateCubeFaceViewMatrix(lightPos, face);
                m_ShadowUniforms.pointLightShadows[shadowIndex].viewProj[face] = projection * view;
            }

            shadowIndex++;
        }
    });

    m_ShadowUniforms.numPointLightShadows = shadowIndex;
}

void ShadowSystem::CalculateSpotLightShadows() {
    m_SpotLightShadows.clear();
    m_ShadowUniforms.numSpotLightShadows = 0;

    u32 shadowIndex = 0;

    // Find all spot lights with castsShadows enabled
    m_ECS->ForEach<Transform, Light>([this, &shadowIndex](Entity entity, Transform& transform, Light& light) {
        if (light.type == LightType::Spot && light.castsShadows && shadowIndex < kMaxSpotLightShadows) {
            m_SpotLightShadows.push_back(entity);

            // Get light position from world matrix
            Vec3 lightPos(transform.worldMatrix[3][0], transform.worldMatrix[3][1], transform.worldMatrix[3][2]);

            // Get light direction
            Vec4 forwardLocal(0.0f, 0.0f, -1.0f, 0.0f);
            Vec4 forwardWorld = transform.worldMatrix * forwardLocal;
            Vec3 lightDir = Normalize(Vec3(forwardWorld.x, forwardWorld.y, forwardWorld.z));

            // Create perspective projection based on outer cone angle
            // Add some margin to the FOV to ensure coverage
            f32 fov = light.outerConeAngle * 2.0f * 1.2f;  // Double the cone angle + 20% margin
            f32 nearPlane = 0.1f;
            f32 farPlane = light.range > 0.0f ? light.range : 25.0f;

            Mat4 projection = glm::perspective(Radians(fov), 1.0f, nearPlane, farPlane);

            // Create view matrix looking along light direction
            Vec3 up(0.0f, 1.0f, 0.0f);
            if (std::abs(Dot(lightDir, up)) > 0.99f) {
                up = Vec3(1.0f, 0.0f, 0.0f);
            }

            Mat4 view = glm::lookAt(lightPos, lightPos + lightDir, up);

            // Store view-projection matrix
            m_ShadowUniforms.spotLightShadows[shadowIndex].viewProj = projection * view;
            m_ShadowUniforms.spotLightShadows[shadowIndex].params = Vec4(0.005f, 0.0f, 0.0f, 0.0f);  // shadow bias

            shadowIndex++;
        }
    });

    m_ShadowUniforms.numSpotLightShadows = shadowIndex;
}

Mat4 ShadowSystem::CalculateCubeFaceViewMatrix(const Vec3& lightPos, u32 faceIndex) {
    // Define target and up vectors for each cube face
    // Face order: +X, -X, +Y, -Y, +Z, -Z
    Vec3 target, up;

    switch (faceIndex) {
        case 0: // +X
            target = lightPos + Vec3(1.0f, 0.0f, 0.0f);
            up = Vec3(0.0f, -1.0f, 0.0f);
            break;
        case 1: // -X
            target = lightPos + Vec3(-1.0f, 0.0f, 0.0f);
            up = Vec3(0.0f, -1.0f, 0.0f);
            break;
        case 2: // +Y
            target = lightPos + Vec3(0.0f, 1.0f, 0.0f);
            up = Vec3(0.0f, 0.0f, 1.0f);
            break;
        case 3: // -Y
            target = lightPos + Vec3(0.0f, -1.0f, 0.0f);
            up = Vec3(0.0f, 0.0f, -1.0f);
            break;
        case 4: // +Z
            target = lightPos + Vec3(0.0f, 0.0f, 1.0f);
            up = Vec3(0.0f, -1.0f, 0.0f);
            break;
        case 5: // -Z
            target = lightPos + Vec3(0.0f, 0.0f, -1.0f);
            up = Vec3(0.0f, -1.0f, 0.0f);
            break;
        default:
            target = lightPos + Vec3(0.0f, 0.0f, 1.0f);
            up = Vec3(0.0f, 1.0f, 0.0f);
            break;
    }

    return glm::lookAt(lightPos, target, up);
}

// ============================================================================
// Shadow Filtering Configuration Methods
// ============================================================================

void ShadowSystem::SetGlobalFilterMode(ShadowFilterMode mode) {
    m_GlobalFilterMode = mode;

    // Apply to all shadow-casting lights
    if (m_ECS) {
        m_ECS->ForEach<Light>([mode](Entity /* entity */, Light& light) {
            if (light.castsShadows) {
                light.shadowFilterMode = mode;
            }
        });
    }
}

void ShadowSystem::SetEVSMParameters(f32 positiveExp, f32 negativeExp, f32 lightBleedReduction) {
    m_EVSMPositiveExp = positiveExp;
    m_EVSMNegativeExp = negativeExp;
    m_EVSMLightBleedReduction = lightBleedReduction;

    // Apply to all shadow-casting lights
    if (m_ECS) {
        m_ECS->ForEach<Light>([=](Entity /* entity */, Light& light) {
            if (light.castsShadows) {
                light.evsmPositiveExponent = positiveExp;
                light.evsmNegativeExponent = negativeExp;
                light.evsmLightBleedReduction = lightBleedReduction;
            }
        });
    }
}

void ShadowSystem::GetEVSMParameters(f32& positiveExp, f32& negativeExp, f32& lightBleedReduction) const {
    positiveExp = m_EVSMPositiveExp;
    negativeExp = m_EVSMNegativeExp;
    lightBleedReduction = m_EVSMLightBleedReduction;
}

void ShadowSystem::SetLightFilterMode(Entity lightEntity, ShadowFilterMode mode) {
    if (!m_ECS || !lightEntity.IsValid()) {
        return;
    }

    if (m_ECS->HasComponent<Light>(lightEntity)) {
        Light& light = m_ECS->GetComponent<Light>(lightEntity);
        light.shadowFilterMode = mode;
    }
}

ShadowFilterMode ShadowSystem::GetLightFilterMode(Entity lightEntity) const {
    if (!m_ECS || !lightEntity.IsValid()) {
        return ShadowFilterMode::PCF;
    }

    if (m_ECS->HasComponent<Light>(lightEntity)) {
        const Light& light = m_ECS->GetComponent<Light>(lightEntity);
        return light.shadowFilterMode;
    }

    return ShadowFilterMode::PCF;
}
