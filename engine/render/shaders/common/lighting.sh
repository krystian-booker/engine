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
        // Directional light
        lightDir = -normalize(light.direction);
        attenuation = 1.0;
    }
    else
    {
        // Point or spot light
        vec3 toLight = light.position - worldPos;
        float distance = length(toLight);
        lightDir = toLight / max(distance, 0.0001);

        // Distance attenuation (inverse square with range falloff)
        float rangeAtten = 1.0 - saturate(distance / max(light.range, 0.0001));
        rangeAtten = rangeAtten * rangeAtten;
        attenuation = rangeAtten;

        // Spot light cone attenuation
        if (light.type == LIGHT_TYPE_SPOT)
        {
            float cosAngle = dot(-lightDir, normalize(light.direction));
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

    for (int i = 0; i < numLights && i < 8; i++)
    {
        Light light = getLight(i);
        float shadow = 1.0; // No shadow by default

        totalLight += evaluateLight(light, worldPos, N, V, albedo, metallic, roughness, shadow);
    }

    return totalLight;
}

// Evaluate all active lights with shadow factor for main directional light
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

    for (int i = 0; i < numLights && i < 8; i++)
    {
        Light light = getLight(i);

        // Apply shadow only to main directional light (index 0)
        float shadow = 1.0;
        if (i == 0 && light.type == LIGHT_TYPE_DIRECTIONAL)
        {
            shadow = mainShadowFactor;
        }

        totalLight += evaluateLight(light, worldPos, N, V, albedo, metallic, roughness, shadow);
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
