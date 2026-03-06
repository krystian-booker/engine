// Light evaluation functions
#ifndef LIGHTING_SH
#define LIGHTING_SH

#include "light_data.sh"

// Light types
#define LIGHT_TYPE_DIRECTIONAL 0.0
#define LIGHT_TYPE_POINT 1.0
#define LIGHT_TYPE_SPOT 2.0

// Calculate light direction and attenuation for a given light
void calculateLightDirAndAttenuation(Light light, vec3 worldPos, out vec3 lightDir, out float attenuation)
{
    if (light.type == LIGHT_TYPE_DIRECTIONAL)
    {
        // Directional light — safe normalize to avoid HLSL div-by-zero warning
        float dirLen = max(length(light.direction), 0.0001);
        lightDir = -light.direction / dirLen;
        attenuation = 1.0;
    }
    else
    {
        // Point or spot light
        vec3 toLight = light.position - worldPos;
        float distance = max(length(toLight), 0.0001);
        lightDir = toLight / distance;

        // UE4-style windowed inverse-square attenuation
        float distanceSqr = distance * distance;
        float rangeSqr = light.range * light.range;
        float ratioSqr = distanceSqr / max(rangeSqr, 0.0001);
        float windowing = saturate(1.0 - ratioSqr * ratioSqr);
        windowing = windowing * windowing;
        attenuation = windowing / (distanceSqr + 1.0);

        // Spot light cone attenuation
        if (light.type == LIGHT_TYPE_SPOT)
        {
            float spotDirLen = max(length(light.direction), 0.0001);
            float cosAngle = dot(-lightDir, light.direction / spotDirLen);
            float innerCos = cos(light.innerAngle);
            float outerCos = cos(light.outerAngle);
            float spotAtten = saturate((cosAngle - outerCos) / max(innerCos - outerCos, 0.0001));
            spotAtten = spotAtten * spotAtten;
            attenuation *= spotAtten;
        }
    }
}

// Evaluate a single light's contribution to PBR shading
vec3 evaluateLight(
    Light light,
    vec3 worldPos,
    vec3 N,
    vec3 V,
    vec3 albedo,
    float metallic,
    float roughness,
    float shadow
)
{
    vec3 lightDir;
    float attenuation;
    calculateLightDirAndAttenuation(light, worldPos, lightDir, attenuation);

    if (attenuation <= 0.0)
        return vec3_splat(0.0);

    // Calculate BRDF
    vec3 brdf = cookTorranceBRDF(N, V, lightDir, albedo, metallic, roughness);

    // Final light contribution
    vec3 radiance = light.color * light.intensity * attenuation * shadow;

    return brdf * radiance;
}

// Evaluate all active lights
vec3 evaluateAllLights(
    vec3 worldPos,
    vec3 N,
    vec3 V,
    vec3 albedo,
    float metallic,
    float roughness
)
{
    vec3 totalLight = vec3_splat(0.0);
    int numLights = int(u_lightCount.x);

    for (int i = 0; i < 8; i++)
    {
        if (i < numLights)
        {
            Light light = getLight(i);
            float shadow = 1.0; // No shadow by default
            totalLight += evaluateLight(light, worldPos, N, V, albedo, metallic, roughness, shadow);
        }
    }

    return totalLight;
}

// Evaluate all active lights with shadow factor applied to the main directional light.
// Light 0 is assumed to be the primary directional light that casts shadows.
// Shadow is applied as a post-multiply to avoid bgfx shader compiler issues where
// conditional branches and function parameters get optimized away.
vec3 evaluateAllLightsWithShadow(
    vec3 worldPos,
    vec3 N,
    vec3 V,
    vec3 albedo,
    float metallic,
    float roughness,
    float mainShadowFactor
)
{
    vec3 totalLight = vec3_splat(0.0);
    int numLights = int(u_lightCount.x);

    // First light (directional) — always apply shadow via post-multiply
    if (numLights > 0)
    {
        Light light = getLight(0);
        vec3 contribution = evaluateLight(light, worldPos, N, V, albedo, metallic, roughness, 1.0);
        totalLight += contribution * mainShadowFactor;
    }

    // Remaining lights — no shadow
    for (int i = 1; i < 8; i++)
    {
        if (i < numLights)
        {
            Light light = getLight(i);
            totalLight += evaluateLight(light, worldPos, N, V, albedo, metallic, roughness, 1.0);
        }
    }

    return totalLight;
}

// Simple ambient term (will be replaced by IBL)
vec3 evaluateAmbient(vec3 albedo, float ao)
{
    vec3 ambient = vec3(0.03, 0.03, 0.03) * albedo * ao;
    return ambient;
}

#endif // LIGHTING_SH
