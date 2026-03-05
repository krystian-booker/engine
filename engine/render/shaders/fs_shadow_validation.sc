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

    // Shadow map depth for visualization (Mode 1)
    int cascade_index = selectCascade(viewSpaceDepth);
    vec3 cascadeShadowCoord = getShadowCoord(worldPos, cascade_index);
    float cascadeDepthInMap = texture2DArrayLod(
        s_shadowMap,
        vec3(cascadeShadowCoord.xy, float(cascade_index)),
        0.0
    ).r;

    // PBR shadow factor (computed unconditionally to avoid HLSL divergent branch issues)
    float shadowFactor = calculateShadow(worldPos, N, mainLightDir, viewSpaceDepth);

    vec3 color = vec3_splat(0.0);

    if (mode == 1)
    {
        // Mode 1: Shadow map depth visualization — selected cascade
        color = vec3_splat(cascadeDepthInMap);
    }
    else if (mode == 2)
    {
        // Mode 2: Binary shadow test using actual calculateShadow path
        color = vec3_splat(shadowFactor);
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
