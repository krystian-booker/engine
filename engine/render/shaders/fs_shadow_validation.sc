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

    // 1. Coordinate-space audit:
    //    - v_worldPos.w is linear view-space depth from vs_pbr.sc
    //    - getShadowCoord() uses u_shadowMatrix[cascade] for the active sun light
    int cascade_index = selectCascade(viewSpaceDepth);
    vec3 cascadeShadowCoord = getShadowCoord(worldPos, cascade_index);
    float cascadeDepthInMap = texture2DArrayLod(
        s_shadowMap,
        vec3(cascadeShadowCoord.xy, float(cascade_index)),
        0.0
    ).r;

    // 2. MANUAL COMPARISON (The 'Auditor')
    // Sample cascade 0 directly to isolate projection data from cascade selection logic.
    const int manualCascade = 0;
    vec3 shadowCoord = getShadowCoord(worldPos, manualCascade);
    float depthInMap = texture2DArrayLod(s_shadowMap, vec3(shadowCoord.xy, 0.0), 0.0).r;

    // Compare pixel's light-space depth vs map depth + manual bias
    float ndotl = clamp(dot(N, mainLightDir), 0.0, 1.0);
    float manualBias = max(0.005 * (1.0 - ndotl), 0.0005);
    float inBounds = step(0.0, shadowCoord.x) * step(shadowCoord.x, 1.0)
                   * step(0.0, shadowCoord.y) * step(shadowCoord.y, 1.0)
                   * step(0.0, shadowCoord.z) * step(shadowCoord.z, 1.0);
    float litTest = step(shadowCoord.z, depthInMap + manualBias);
    float isLit = mix(1.0, litTest, inBounds);

    // Compute real PBR shadow factor unconditionally to avoid HLSL compiler bug in divergent branches
    float shadowFactor = calculateShadow(worldPos, N, mainLightDir, viewSpaceDepth);

    vec3 color = vec3_splat(0.0);

    if (mode == 1)
    {
        // Mode 1: Visualize the light's view of the world
        // We use pow to boost the deep depth mapping so it looks cleanly grayscale
        color = vec3_splat(pow(max(cascadeDepthInMap, 0.0), 80.0));
    }
    else if (mode == 2)
    {
        // Mode 2: If this is black/white and matches the spheres,
        // your World-to-Light transform is fixed!
        color = vec3_splat(isLit);
    }
    else
    {
        // Mode 0/3: Full PBR using the engine's built-in shadow factor
        vec3 Lo = evaluateAllLightsWithShadow(worldPos, N, V, albedo, metallic, roughness, shadowFactor);
        color = (vec3_splat(0.05) * albedo) + Lo;
    }

    gl_FragColor = vec4(color, 1.0);
}
