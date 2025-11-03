#include "lighting_system.h"
#include <algorithm>
#include <cmath>

LightingSystem::LightingSystem(ECSCoordinator* ecs)
    : m_ECS(ecs) {
}

LightingSystem::~LightingSystem() = default;

void LightingSystem::SetActiveCamera(Entity cameraEntity) {
    m_ActiveCamera = cameraEntity;
}

void LightingSystem::Update() {
    if (!m_ECS) {
        return;
    }

    // Reset lighting data
    m_LightingData.numLights = 0;

    // Get camera position
    if (m_ActiveCamera.IsValid() && m_ECS->HasComponent<Transform>(m_ActiveCamera)) {
        Transform& camTransform = m_ECS->GetComponent<Transform>(m_ActiveCamera);
        m_LightingData.cameraPosition = Vec4(camTransform.worldMatrix[3][0],
                                             camTransform.worldMatrix[3][1],
                                             camTransform.worldMatrix[3][2],
                                             1.0f);
    } else {
        // Default camera position if no active camera
        m_LightingData.cameraPosition = Vec4(0.0f, 0.0f, 10.0f, 1.0f);
    }

    // Collect lights from ECS
    u32 lightCount = 0;
    m_ECS->ForEach<Transform, Light>([this, &lightCount](Entity entity, Transform& transform, Light& light) {
        if (lightCount >= kMaxLights) {
            return;  // Max lights reached
        }

        GPULight& gpuLight = m_LightingData.lights[lightCount];

        // Extract world position from transform matrix
        Vec3 worldPos(transform.worldMatrix[3][0],
                     transform.worldMatrix[3][1],
                     transform.worldMatrix[3][2]);

        // Set position/type
        gpuLight.positionAndType = Vec4(worldPos.x, worldPos.y, worldPos.z,
                                        static_cast<f32>(static_cast<u32>(light.type)));

        // Set color and intensity
        gpuLight.colorAndIntensity = Vec4(light.color.x, light.color.y, light.color.z,
                                          light.intensity);

        // Calculate light direction from transform
        // Forward vector in local space is (0, 0, -1), transform to world space
        Vec4 forwardLocal(0.0f, 0.0f, -1.0f, 0.0f);  // Direction vector (w=0)
        Vec4 forwardWorld = transform.worldMatrix * forwardLocal;
        Vec3 direction = Normalize(Vec3(forwardWorld.x, forwardWorld.y, forwardWorld.z));

        // Set direction and range
        gpuLight.directionAndRange = Vec4(direction.x, direction.y, direction.z,
                                          light.range);

        // Set spot light cone angles (convert degrees to cosine)
        f32 innerCos = std::cos(Radians(light.innerConeAngle));
        f32 outerCos = std::cos(Radians(light.outerConeAngle));
        f32 castsShadows = light.castsShadows ? 1.0f : 0.0f;
        f32 shadowMapIndex = 0.0f;  // For now, all directional lights use cascade 0
        gpuLight.spotAngles = Vec4(innerCos, outerCos, castsShadows, shadowMapIndex);

        // Set extended parameters based on light type
        if (light.type == LightType::Area) {
            // Area light parameters
            gpuLight.areaParams = Vec4(light.width, light.height, light.twoSided ? 1.0f : 0.0f, 0.0f);
            gpuLight.tubeParams = Vec4(0.0f, 0.0f, 0.0f, 0.0f);

            // Extract right and up vectors from transform for area light orientation
            Vec4 rightLocal(1.0f, 0.0f, 0.0f, 0.0f);
            Vec4 upLocal(0.0f, 1.0f, 0.0f, 0.0f);
            Vec4 rightWorld = transform.worldMatrix * rightLocal;
            Vec4 upWorld = transform.worldMatrix * upLocal;

            Vec3 right = Normalize(Vec3(rightWorld.x, rightWorld.y, rightWorld.z));
            Vec3 up = Normalize(Vec3(upWorld.x, upWorld.y, upWorld.z));

            gpuLight.hemisphereParams = Vec4(right.x, right.y, right.z, 0.0f);
            gpuLight.hemisphereParams2 = Vec4(up.x, up.y, up.z, 0.0f);
        }
        else if (light.type == LightType::Tube) {
            // Tube light parameters
            gpuLight.areaParams = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gpuLight.tubeParams = Vec4(light.tubeLength, light.tubeRadius, 0.0f, 0.0f);
            gpuLight.hemisphereParams = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gpuLight.hemisphereParams2 = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }
        else if (light.type == LightType::Hemisphere) {
            // Hemisphere light parameters
            gpuLight.areaParams = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gpuLight.tubeParams = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gpuLight.hemisphereParams = Vec4(light.skyColor.x, light.skyColor.y, light.skyColor.z, 0.0f);
            gpuLight.hemisphereParams2 = Vec4(light.groundColor.x, light.groundColor.y, light.groundColor.z, 0.0f);
        }
        else {
            // Default/other light types - clear extended params
            gpuLight.areaParams = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gpuLight.tubeParams = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gpuLight.hemisphereParams = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gpuLight.hemisphereParams2 = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        lightCount++;
    });

    m_LightingData.numLights = lightCount;
}
