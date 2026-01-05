$input v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_color0, v_clipPos

#include <bgfx_shader.sh>
#include "common/uniforms.sh"
#include "common/pbr.sh"
#include "common/lighting.sh"
#include "common/shadow.sh"

// Foliage textures
SAMPLER2D(s_albedo,           0);  // Base color (RGB) + opacity (A)
SAMPLER2D(s_normal,           1);  // Normal map

// IBL textures
SAMPLERCUBE(s_irradiance,     5);  // Diffuse IBL (irradiance map)
SAMPLERCUBE(s_prefilter,      6);  // Specular IBL (prefiltered environment)
SAMPLER2D(s_brdfLUT,          7);  // BRDF integration LUT

uniform vec4 u_foliageParams; // x=unused, y=unused, z=alpha_cutoff, w=fade_start

void main()
{
    // Sample albedo texture
    vec4 albedoSample = texture2D(s_albedo, v_texcoord0);
    vec4 albedo = albedoSample * u_albedoColor * v_color0;

    // Alpha test for leaves
    float alphaCutoff = u_foliageParams.z;
    if (alphaCutoff <= 0.0)
    {
        alphaCutoff = 0.5;  // Default cutoff
    }
    if (albedo.a < alphaCutoff)
    {
        discard;
    }

    // Sample normal map
    vec3 normalSample = texture2D(s_normal, v_texcoord0).xyz;
    vec3 N;
    if (length(normalSample) > 0.01)
    {
        N = getNormalFromMap(normalSample, v_normal, v_tangent, v_bitangent);
    }
    else
    {
        N = normalize(v_normal);
    }

    // Foliage uses low metallic, moderate roughness
    float metallic = 0.0;
    float roughness = 0.6;

    // View direction
    vec3 worldPos = v_worldPos.xyz;
    float viewSpaceDepth = v_worldPos.w;
    vec3 V = normalize(u_cameraPos.xyz - worldPos);

    // Get main directional light direction for shadow calculation
    vec3 mainLightDir = -u_lights[1].xyz;

    // Calculate shadow factor
    float shadowFactor = 1.0;
    if (u_shadowParams.x > 0.0)
    {
        shadowFactor = calculateShadow(worldPos, N, mainLightDir, viewSpaceDepth);
    }

    // Calculate direct lighting with shadow
    vec3 Lo = evaluateAllLightsWithShadow(worldPos, N, V, albedo.rgb, metallic, roughness, shadowFactor);

    // Calculate ambient/IBL
    vec3 F0 = vec3_splat(0.04);  // Non-metallic F0
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = vec3_splat(1.0) - kS;

    // Diffuse IBL
    vec3 irradiance = textureCube(s_irradiance, N).rgb;
    vec3 diffuseIBL = irradiance * albedo.rgb;

    // Specular IBL (minimal for foliage)
    vec3 R = reflect(-V, N);
    float maxMipLevel = u_iblParams.z;
    float mipLevel = roughness * maxMipLevel;
    vec3 prefilteredColor = textureCubeLod(s_prefilter, R, mipLevel).rgb;
    vec2 brdf = texture2D(s_brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

    // Combine ambient
    vec3 ambient = (kD * diffuseIBL + specularIBL * 0.3) * u_iblParams.x;

    // Final color
    vec3 color = ambient + Lo;

    // Output (HDR)
    gl_FragColor = vec4(color, albedo.a);
}
