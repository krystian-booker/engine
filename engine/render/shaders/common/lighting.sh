// Light evaluation functions
#ifndef LIGHTING_SH
#define LIGHTING_SH

#include "uniforms.sh"

// Light types
#define LIGHT_TYPE_DIRECTIONAL 0.0
#define LIGHT_TYPE_POINT 1.0
#define LIGHT_TYPE_SPOT 2.0

// Light data structure (unpacked from uniforms)
struct Light
{
    vec3 position;
    float type;
    vec3 direction;
    float range;
    vec3 color;
    float intensity;
    float innerAngle;
    float outerAngle;
    int shadowIndex;
};

// Unpack light from uniform array
Light getLight(int index)
{
    Light light;
    int base = index * 4;

    vec4 data0 = u_lights[base + 0];
    vec4 data1 = u_lights[base + 1];
    vec4 data2 = u_lights[base + 2];
    vec4 data3 = u_lights[base + 3];

    light.position = data0.xyz;
    light.type = data0.w;
    light.direction = data1.xyz;
    light.range = data1.w;
    light.color = data2.xyz;
    light.intensity = data2.w;
    light.innerAngle = data3.x;
    light.outerAngle = data3.y;
    light.shadowIndex = int(data3.z);

    return light;
}

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

        // Distance attenuation (inverse square with range falloff)
        float rangeAtten = 1.0 - saturate(distance / max(light.range, 0.0001));
        rangeAtten = rangeAtten * rangeAtten;
        attenuation = rangeAtten;

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
