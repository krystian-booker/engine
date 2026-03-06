$input v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_color0, v_clipPos

#include <bgfx_shader.sh>
#include "common/uniforms.sh"
#include "common/pbr.sh"
#include "common/lighting.sh"
#include "common/shadow.sh"

// PBR textures (must be declared even if unused — bgfx uniform binding)
SAMPLER2D(s_albedo,           0);
SAMPLER2D(s_normal,           1);
SAMPLER2D(s_metallicRoughness, 2);
SAMPLER2D(s_ao,               3);
SAMPLER2D(s_emissive,         4);

// IBL textures (unused but keep slots consistent with vs_pbr)
SAMPLERCUBE(s_irradiance,     5);
SAMPLERCUBE(s_prefilter,      6);
SAMPLER2D(s_brdfLUT,          7);

// Shadow maps declared in shadow.sh (slot 8)

void main()
{
    vec3 albedo    = u_albedoColor.xyz;
    float metallic = u_pbrParams.x;
    float roughness = max(u_pbrParams.y, 0.04);
    float ao       = u_pbrParams.z;
    int mode       = int(u_pbrParams.w);

    vec3 N = normalize(v_normal);
    vec3 worldPos = v_worldPos.xyz;
    float viewSpaceDepth = v_worldPos.w;
    vec3 V = normalize(u_cameraPos.xyz - worldPos);

    // Main directional light direction (light 0)
    vec3 mainLightDir = normalize(-u_lights[1].xyz);

    // PBR shadow factor (computed unconditionally to avoid HLSL divergent branch issues)
    float shadowFactor = calculateShadow(worldPos, N, mainLightDir, viewSpaceDepth);

    vec3 color = vec3_splat(0.0);

    if (mode == 1)
    {
        // Mode 1: Grayscale shadow map debug — diffuse shading * shadow factor
        // Shows sphere shapes via NdotL and shadow patterns via PCF-filtered shadow map.
        // Validates the depth pass: incorrect shadow map → wrong shadow shapes.
        float NdotL = max(dot(N, mainLightDir), 0.0);
        color = vec3_splat(NdotL * shadowFactor);
    }
    else if (mode == 2)
    {
        // Mode 2: Binary shadow — single sample, no PCF for hard black/white
        vec3 offsetPos = worldPos + N * u_shadowParams.y;
        int cascade = selectCascade(viewSpaceDepth);
        vec3 shadowCoord = getShadowCoord(offsetPos, cascade);

        // Bounds check — outside shadow map is fully lit
        if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
            shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
            shadowCoord.z < 0.0 || shadowCoord.z > 1.0)
        {
            color = vec3_splat(1.0);
        }
        else
        {
            float bias = calculateBias(N, mainLightDir);
            float storedDepth = texture2DArrayLod(
                s_shadowMap,
                vec3(shadowCoord.xy, float(cascade)),
                0.0
            ).r;
            float shadow = step(shadowCoord.z - bias, storedDepth);
            color = vec3_splat(shadow);
        }
    }
    else if (mode == 3)
    {
        // Mode 3: PBR + shadows (close-up PCF softness test)
        vec3 Lo = evaluateAllLightsWithShadow(worldPos, N, V, albedo, metallic, roughness, shadowFactor);
        color = (vec3_splat(0.05) * albedo) + Lo;
    }
    else
    {
        // Mode 0: Full PBR + Shadows
        vec3 Lo = evaluateAllLightsWithShadow(worldPos, N, V, albedo, metallic, roughness, shadowFactor);
        color = (vec3_splat(0.05) * albedo) + Lo;
    }

    gl_FragColor = vec4(color, 1.0);
}
