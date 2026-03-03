$input v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_color0, v_clipPos

#include <bgfx_shader.sh>
#include "common/uniforms.sh"
#include "common/pbr.sh"
#include "common/lighting.sh"
#include "common/shadow.sh"

// PBR textures
SAMPLER2D(s_albedo,           0);
SAMPLER2D(s_normal,           1);
SAMPLER2D(s_metallicRoughness, 2);
SAMPLER2D(s_ao,               3);
SAMPLER2D(s_emissive,         4);

// IBL textures
SAMPLERCUBE(s_irradiance,     5);
SAMPLERCUBE(s_prefilter,      6);
SAMPLER2D(s_brdfLUT,          7);

void main()
{
    // Sample textures
    vec4 albedoSample = texture2D(s_albedo, v_texcoord0);
    vec4 albedo = albedoSample * u_albedoColor * v_color0;

    // Normal — use geometry normal (no normal map for validation spheres)
    vec3 N = normalize(v_normal);

    // Material params
    float metallic = u_pbrParams.x;
    float roughness = u_pbrParams.y;
    roughness = max(roughness, 0.04);
    float ao = u_pbrParams.z;

    // Validation mode from alpha_cutoff slot
    int mode = int(u_pbrParams.w);

    // View direction
    vec3 worldPos = v_worldPos.xyz;
    float viewSpaceDepth = v_worldPos.w;
    vec3 V = normalize(u_cameraPos.xyz - worldPos);

    // Direct lighting (no shadows for validation)
    vec3 Lo = evaluateAllLightsWithShadow(worldPos, N, V, albedo.rgb, metallic, roughness, 1.0);

    // Calculate ambient/IBL
    vec3 F0 = mix(vec3_splat(0.04), albedo.rgb, metallic);
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = vec3_splat(1.0) - kS;
    kD *= (1.0 - metallic);

    // Diffuse IBL
    vec3 irradiance = textureCube(s_irradiance, N).rgb;
    vec3 diffuseIBL = irradiance * albedo.rgb;

    // Specular IBL
    vec3 R = reflect(-V, N);
    float maxMipLevel = u_iblParams.z;
    float mipLevel = roughness * maxMipLevel;
    vec3 prefilteredColor = textureCubeLod(s_prefilter, R, mipLevel).rgb;

    // BRDF LUT
    vec2 brdf = texture2D(s_brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);

    // Emissive
    vec3 emissive = texture2D(s_emissive, v_texcoord0).rgb * u_emissiveColor.rgb * u_emissiveColor.w;

    // Full ambient
    vec3 ambient = (kD * diffuseIBL + specularIBL) * ao * u_iblParams.x;

    // Mode switch
    vec3 color;
    if (mode == 1) {
        // Specular IBL only
        color = specularIBL * ao * u_iblParams.x;
    } else if (mode == 2) {
        // Diffuse IBL only
        color = kD * diffuseIBL * ao * u_iblParams.x;
    } else {
        // Mode 0/3: Full IBL + Analytical
        color = ambient + Lo + emissive;
    }

    gl_FragColor = vec4(color, albedo.a);
}
