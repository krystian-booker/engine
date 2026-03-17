$input v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_color0, v_clipPos

#include <bgfx_shader.sh>
#include "common/uniforms.sh"
#include "common/pbr.sh"
#include "common/light_probes.sh"
#include "common/lighting.sh"
#include "common/shadow.sh"

// PBR textures
SAMPLER2D(s_albedo,           0);  // Base color (RGB) + opacity (A)
SAMPLER2D(s_normal,           1);  // Normal map
SAMPLER2D(s_metallicRoughness, 2);  // Green = roughness, Blue = metallic (glTF convention)
SAMPLER2D(s_ao,               3);  // Ambient occlusion
SAMPLER2D(s_emissive,         4);  // Emissive color

// IBL textures
SAMPLERCUBE(s_irradiance,     5);  // Diffuse IBL (irradiance map)
SAMPLERCUBE(s_prefilter,      6);  // Specular IBL (prefiltered environment)
SAMPLER2D(s_brdfLUT,          7);  // BRDF integration LUT

// Shadow maps are defined in shadow.sh (slots 8-11)

// Screen-space SSAO (slot 12) — sampled with screen UVs, not mesh UVs
SAMPLER2D(s_ssao, 12);

// Opaque scene copy for screen-space refraction (slot 13)
SAMPLER2D(s_opaqueColor, 13);
SAMPLER2D(s_opaqueDepth, 15);

uniform vec4 u_oitParams;      // x=weight_power, y=max_distance, z=near_plane, w=far_plane

// Calculate depth-based weight for OIT
float calculateWeight(float z, float alpha)
{
    float power = u_oitParams.x;
    float maxDist = u_oitParams.y;
    z = clamp(z, 0.001, maxDist);
    return pow(max(alpha, 0.0), power) * clamp(0.03 / (1e-5 + pow(z / maxDist, 4.0)), 1e-2, 3e3);
}

vec3 reconstructWorldPos(vec2 uv, float depth)
{
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
#if BGFX_SHADER_LANGUAGE_HLSL
    clipPos.y = -clipPos.y;
#endif
    vec4 worldPos = mul(u_invViewProj, clipPos);
    return worldPos.xyz / worldPos.w;
}

vec2 projectWorldToScreenUV(vec3 worldPos)
{
    vec4 clipPos = mul(u_viewProj, vec4(worldPos, 1.0));
    vec2 uv = clipPos.xy / clipPos.w * 0.5 + 0.5;
#if BGFX_SHADER_LANGUAGE_HLSL || BGFX_SHADER_LANGUAGE_PSSL || BGFX_SHADER_LANGUAGE_METAL
    uv.y = 1.0 - uv.y;
#endif
    return uv;
}

float getViewDepth(vec3 worldPos)
{
    vec4 viewPos = mul(u_view, vec4(worldPos, 1.0));
    return -viewPos.z;
}

void main()
{
    // Sample textures
    vec4 albedoSample = texture2D(s_albedo, v_texcoord0);
    vec4 albedo = albedoSample * u_albedoColor * v_color0;

    // Alpha test
    if (albedo.a < u_pbrParams.w)
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

    // Sample metallic-roughness
    vec4 mrSample = texture2D(s_metallicRoughness, v_texcoord0);
    float metallic = mrSample.b * u_pbrParams.x;
    float roughness = mrSample.g * u_pbrParams.y;
    roughness = max(roughness, 0.04); // Prevent divide by zero

    // Sample ambient occlusion
    float ao = texture2D(s_ao, v_texcoord0).r * u_pbrParams.z;

    // Sample emissive
    vec3 emissive = texture2D(s_emissive, v_texcoord0).rgb * u_emissiveColor.rgb * u_emissiveColor.w;

    // View direction
    vec3 worldPos = v_worldPos.xyz;
    float viewSpaceDepth = v_worldPos.w;
    vec3 V = normalize(u_cameraPos.xyz - worldPos);

    // Get main directional light direction for shadow calculation
    // Light 0 is assumed to be the main directional light
    Light mainLight = getLight(0);
    vec3 mainLightDir = normalize(-mainLight.direction);

    // Calculate shadow factor (1.0 = fully lit, 0.0 = fully shadowed)
    float shadowFactor = 1.0;
    if (u_shadowParams.x > 0.0)  // Check if shadows are enabled (bias > 0)
    {
        shadowFactor = calculateShadow(worldPos, N, mainLightDir, viewSpaceDepth);
    }

    // Calculate direct lighting with shadow applied only to the main directional light.
    // evaluateAllLightsWithShadow applies shadow as a post-multiply on light 0 only,
    // leaving fill lights unaffected. This avoids the bgfx/HLSL compiler issue where
    // shadow multiplications inside called functions get optimized away.
    vec3 Lo = evaluateAllLightsWithShadow(worldPos, N, V, albedo.rgb, metallic, roughness, shadowFactor);

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

    // Sample screen-space SSAO using clip-space position
    vec2 screenUV = v_clipPos.xy / v_clipPos.w * 0.5 + 0.5;
#if BGFX_SHADER_LANGUAGE_HLSL || BGFX_SHADER_LANGUAGE_PSSL || BGFX_SHADER_LANGUAGE_METAL
    screenUV.y = 1.0 - screenUV.y;
#endif
    float ssao = texture2D(s_ssao, screenUV).r;

    // Combine ambient with shadow attenuation.
    // Split ambient shadow: diffuse IBL dims more in shadow, specular (metallic reflections) less so.
    float diffuseAmbientShadow = mix(u_hemisphereGround.w, 1.0, shadowFactor);
    float specularShadowMin = mix(0.7, 0.92, metallic);
    float specularAmbientShadow = mix(specularShadowMin, 1.0, shadowFactor);

    // Hemisphere ambient — lightweight GI approximation
    // Uses vertex AO but NOT screen-space AO: hemisphere irradiance comes from
    // the far-field sky/ground, not affected by local screen-space occlusion.
    vec3 hemisphereAmbient = evaluateHemisphereAmbient(N, albedo.rgb, ao,
        u_hemisphereGround.rgb, u_hemisphereSky.rgb);
    float probeInfluence = probeVolumeContains(worldPos);
    vec3 probeDiffuse = sampleProbeIrradiance(worldPos, N) * albedo.rgb;
    vec3 hemisphereFloor = hemisphereAmbient * mix(1.0, u_probeState.y, probeInfluence);

    vec3 ambient = kD * diffuseIBL * ao * ssao * u_iblParams.x * diffuseAmbientShadow
                 + specularIBL * ao * ssao * u_iblParams.x * specularAmbientShadow
                 + kD * probeDiffuse * ao
                 + hemisphereFloor;

    // Final color (linear HDR — tonemapping + gamma applied by post-processing pipeline)
    vec3 color = ambient + Lo + emissive;

    // Screen-space refraction for transmissive materials
    float transmission = u_refractionParams.y;
    if (transmission > 0.0)
    {
        float ior = u_refractionParams.x;
        float thickness = u_refractionParams.z;
        vec2 refractedUV = screenUV;
        vec3 refractedDir = refract(-V, N, 1.0 / max(ior, 1.0));

        if (dot(refractedDir, refractedDir) > 0.0001)
        {
            const int refractionSteps = 6;
            float volumeThickness = max(thickness * 2.0, 0.05);
            vec3 exitPos = worldPos + refractedDir * volumeThickness;
            vec3 outsideDir = refract(refractedDir, -N, max(ior, 1.0));
            if (dot(outsideDir, outsideDir) < 0.0001)
            {
                outsideDir = -V;
            }

            float outsideDistance = clamp(max(viewSpaceDepth * 0.1, 0.35), 0.35, 1.5);
            vec3 samplePos = exitPos + outsideDir * outsideDistance;
            vec2 exitUV = projectWorldToScreenUV(samplePos);
            refractedUV = exitUV;

            // Keep the transmission sample local to the volume exit instead of
            // tracing a long post-exit ray, which over-shifts curved glass and
            // pulls in unrelated background geometry.
            float exitViewDepth = getViewDepth(samplePos);
            float depthBias = max((volumeThickness + outsideDistance) * 0.25, 0.01);

            for (int step = 1; step <= refractionSteps; ++step)
            {
                float t = float(step) / float(refractionSteps);
                vec2 marchUV = mix(screenUV, exitUV, t);

                if (marchUV.x < 0.0 || marchUV.x > 1.0 ||
                    marchUV.y < 0.0 || marchUV.y > 1.0)
                {
                    break;
                }

                float opaqueDepth = texture2D(s_opaqueDepth, marchUV).r;
                if (opaqueDepth >= 0.9999)
                {
                    refractedUV = marchUV;
                    continue;
                }

                vec3 opaqueWorldPos = reconstructWorldPos(marchUV, opaqueDepth);
                float rayViewDepth = mix(viewSpaceDepth, exitViewDepth, t);
                float opaqueViewDepth = getViewDepth(opaqueWorldPos);

                if (rayViewDepth <= opaqueViewDepth + depthBias)
                {
                    refractedUV = marchUV;
                }
            }
        }

        // Sample the opaque scene behind this surface using the depth-aware hit/fallback UV.
        vec3 background = texture2D(s_opaqueColor, clamp(refractedUV, vec2(0.0), vec2(1.0))).rgb;

        // Fresnel-based blend: more reflection at glancing angles, more transmission head-on
        float fresnel = pow(1.0 - max(dot(N, V), 0.0), 5.0);
        float transmissionFactor = transmission * (1.0 - fresnel);

        // Transmission surfaces should keep only a thin reflective lobe over the
        // refracted scene, not the full opaque diffuse response.
        vec3 transmittedSurface = specularIBL * ao * ssao * u_iblParams.x * specularAmbientShadow
                                + Lo * fresnel
                                + emissive;

        // Blend refracted background with surface lighting once in shader space.
        color = mix(background, transmittedSurface, 1.0 - transmissionFactor);

        // Transmission materials are already composed against the scene copy.
        // Output full coverage here so framebuffer alpha blending does not
        // attenuate the refracted result a second time.
        gl_FragColor = vec4(color, 1.0);
        return;
    }

#if defined(OIT_ENABLED)
    float weight = calculateWeight(gl_FragCoord.z, albedo.a);
    gl_FragData[0] = vec4(color.rgb * albedo.a * weight, albedo.a * weight);
    gl_FragData[1] = vec4(albedo.a, 0.0, 0.0, 1.0);
#else
    gl_FragColor = vec4(color, albedo.a);
#endif
}
