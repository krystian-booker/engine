$input v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_tileUV, v_clipPos

#include <bgfx_shader.sh>
#include "common/uniforms.sh"
#include "common/pbr.sh"
#include "common/lighting.sh"
#include "common/shadow.sh"

// Splat map (RGBA = weights for layers 0-3)
SAMPLER2D(s_splatMap,         0);

// Layer 0 textures
SAMPLER2D(s_layer0Albedo,     1);
SAMPLER2D(s_layer0Normal,     2);
SAMPLER2D(s_layer0ARM,        3);  // AO, Roughness, Metallic

// Layer 1 textures
SAMPLER2D(s_layer1Albedo,     4);
SAMPLER2D(s_layer1Normal,     5);
SAMPLER2D(s_layer1ARM,        6);

// Layer 2 textures
SAMPLER2D(s_layer2Albedo,     7);
SAMPLER2D(s_layer2Normal,     8);
SAMPLER2D(s_layer2ARM,        9);

// Layer 3 textures
SAMPLER2D(s_layer3Albedo,     10);
SAMPLER2D(s_layer3Normal,     11);
SAMPLER2D(s_layer3ARM,        12);

// IBL textures
SAMPLERCUBE(s_irradiance,     13);
SAMPLERCUBE(s_prefilter,      14);
SAMPLER2D(s_brdfLUT,          15);

// Terrain-specific uniforms
uniform vec4 u_terrainParams;    // x: tile scale, y: unused, z: unused, w: unused
uniform vec4 u_layer0Params;     // x: uv scale, y: metallic multiplier, z: roughness multiplier, w: ao multiplier
uniform vec4 u_layer1Params;
uniform vec4 u_layer2Params;
uniform vec4 u_layer3Params;

vec3 getNormalFromMap(vec3 normalSample, vec3 N, vec3 T, vec3 B)
{
    vec3 tangentNormal = normalSample * 2.0 - 1.0;
    mat3 TBN = mat3(T, B, N);
    return normalize(mul(TBN, tangentNormal));
}

void main()
{
    // Sample splat map
    vec4 splat = texture2D(s_splatMap, v_texcoord0);

    // Normalize weights (in case they don't sum to 1)
    float weightSum = splat.r + splat.g + splat.b + splat.a;
    if (weightSum > 0.001)
    {
        splat /= weightSum;
    }
    else
    {
        splat = vec4(1.0, 0.0, 0.0, 0.0);  // Default to layer 0
    }

    // Calculate tiled UVs for each layer
    vec2 uv0 = v_tileUV * u_layer0Params.x;
    vec2 uv1 = v_tileUV * u_layer1Params.x;
    vec2 uv2 = v_tileUV * u_layer2Params.x;
    vec2 uv3 = v_tileUV * u_layer3Params.x;

    // Sample layer albedos
    vec3 albedo0 = texture2D(s_layer0Albedo, uv0).rgb;
    vec3 albedo1 = texture2D(s_layer1Albedo, uv1).rgb;
    vec3 albedo2 = texture2D(s_layer2Albedo, uv2).rgb;
    vec3 albedo3 = texture2D(s_layer3Albedo, uv3).rgb;

    // Blend albedo
    vec3 albedo = albedo0 * splat.r + albedo1 * splat.g + albedo2 * splat.b + albedo3 * splat.a;

    // Sample layer normals
    vec3 normal0 = texture2D(s_layer0Normal, uv0).xyz;
    vec3 normal1 = texture2D(s_layer1Normal, uv1).xyz;
    vec3 normal2 = texture2D(s_layer2Normal, uv2).xyz;
    vec3 normal3 = texture2D(s_layer3Normal, uv3).xyz;

    // Blend normals (simple linear blend - works well for terrain)
    vec3 normalSample = normal0 * splat.r + normal1 * splat.g + normal2 * splat.b + normal3 * splat.a;

    // Convert to world space normal
    vec3 N;
    if (length(normalSample) > 0.01)
    {
        N = getNormalFromMap(normalSample, normalize(v_normal), normalize(v_tangent), normalize(v_bitangent));
    }
    else
    {
        N = normalize(v_normal);
    }

    // Sample ARM (AO, Roughness, Metallic) for each layer
    vec3 arm0 = texture2D(s_layer0ARM, uv0).rgb;
    vec3 arm1 = texture2D(s_layer1ARM, uv1).rgb;
    vec3 arm2 = texture2D(s_layer2ARM, uv2).rgb;
    vec3 arm3 = texture2D(s_layer3ARM, uv3).rgb;

    // Apply per-layer multipliers
    arm0 *= vec3(u_layer0Params.w, u_layer0Params.z, u_layer0Params.y);
    arm1 *= vec3(u_layer1Params.w, u_layer1Params.z, u_layer1Params.y);
    arm2 *= vec3(u_layer2Params.w, u_layer2Params.z, u_layer2Params.y);
    arm3 *= vec3(u_layer3Params.w, u_layer3Params.z, u_layer3Params.y);

    // Blend ARM
    vec3 arm = arm0 * splat.r + arm1 * splat.g + arm2 * splat.b + arm3 * splat.a;
    float ao = arm.r;
    float roughness = max(arm.g, 0.04);
    float metallic = arm.b;

    // View direction
    vec3 worldPos = v_worldPos.xyz;
    float viewSpaceDepth = v_worldPos.w;
    vec3 V = normalize(u_cameraPos.xyz - worldPos);

    // Get main directional light direction
    vec3 mainLightDir = -u_lights[1].xyz;

    // Calculate shadow factor
    float shadowFactor = 1.0;
    if (u_shadowParams.x > 0.0)
    {
        shadowFactor = calculateShadow(worldPos, N, mainLightDir, viewSpaceDepth);
    }

    // Calculate direct lighting with shadow
    vec3 Lo = evaluateAllLightsWithShadow(worldPos, N, V, albedo, metallic, roughness, shadowFactor);

    // Calculate ambient/IBL
    vec3 F0 = mix(vec3_splat(0.04), albedo, metallic);
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = vec3_splat(1.0) - kS;
    kD *= (1.0 - metallic);

    // Diffuse IBL
    vec3 irradiance = textureCube(s_irradiance, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;

    // Specular IBL
    vec3 R = reflect(-V, N);
    float maxMipLevel = u_iblParams.z;
    float mipLevel = roughness * maxMipLevel;
    vec3 prefilteredColor = textureCubeLod(s_prefilter, R, mipLevel).rgb;

    // BRDF LUT
    vec2 brdf = texture2D(s_brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

    // Combine ambient
    vec3 ambient = (kD * diffuseIBL + specularIBL) * ao * u_iblParams.x;

    // Final color
    vec3 color = ambient + Lo;

    gl_FragColor = vec4(color, 1.0);
}
