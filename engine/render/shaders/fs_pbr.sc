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

float getMaxModelScale()
{
    vec3 c0 = u_model[0][0].xyz;
    vec3 c1 = u_model[0][1].xyz;
    vec3 c2 = u_model[0][2].xyz;
    return max(length(c0), max(length(c1), length(c2)));
}

vec3 transformPointToLocal(vec3 worldPos)
{
    return mul(u_invModel, vec4(worldPos, 1.0)).xyz;
}

vec3 transformPointToWorld(vec3 localPos)
{
    return mul(u_model[0], vec4(localPos, 1.0)).xyz;
}

vec2 intersectSphere(vec3 rayOrigin, vec3 rayDir, vec3 sphereCenter, float sphereRadius)
{
    vec3 toOrigin = rayOrigin - sphereCenter;
    float b = dot(toOrigin, rayDir);
    float c = dot(toOrigin, toOrigin) - sphereRadius * sphereRadius;
    float discriminant = b * b - c;
    if (discriminant <= 0.0)
    {
        return vec2(-1.0, -1.0);
    }

    float root = sqrt(discriminant);
    return vec2(-b - root, -b + root);
}

bool solveSphereTransmission(vec3 worldPos, float ior, out vec3 exitWorldPos, out vec3 exitWorldDir, out vec2 exitUV)
{
    if (u_refractionVolume.w <= 0.0)
    {
        return false;
    }

    vec3 localCenter = u_refractionVolume.xyz;
    float localRadius = u_refractionVolume.w;
    vec3 localSurfacePos = transformPointToLocal(worldPos);
    vec3 localCameraPos = transformPointToLocal(u_cameraPos.xyz);
    vec3 localEntryNormal = normalize(localSurfacePos - localCenter);
    vec3 localIncidentDir = normalize(localSurfacePos - localCameraPos);
    vec3 localInsideDir = refract(localIncidentDir, localEntryNormal, 1.0 / max(ior, 1.0));

    if (dot(localInsideDir, localInsideDir) <= 0.0001)
    {
        return false;
    }

    float shellThickness = max(u_refractionParams.z, 0.0);
    shellThickness = max(shellThickness, localRadius * 0.2);
    float innerRadius = max(localRadius - shellThickness, 0.0);
    vec3 localExitPos;
    vec3 localExitDir;

    if (innerRadius > 0.0001 && innerRadius < localRadius - 0.0001)
    {
        vec3 shellFrontOrigin = localSurfacePos + localInsideDir * (localRadius * 0.001);
        vec2 innerFrontHits = intersectSphere(shellFrontOrigin, localInsideDir, localCenter, innerRadius);
        if (innerFrontHits.x <= 0.0001)
        {
            return false;
        }

        vec3 localInnerFrontPos = shellFrontOrigin + localInsideDir * innerFrontHits.x;
        vec3 localInnerFrontNormal = normalize(localInnerFrontPos - localCenter);
        vec3 localCavityDir = refract(localInsideDir, localInnerFrontNormal, max(ior, 1.0));
        if (dot(localCavityDir, localCavityDir) <= 0.0001)
        {
            return false;
        }

        vec3 cavityOrigin = localInnerFrontPos + localCavityDir * (innerRadius * 0.001);
        vec2 innerBackHits = intersectSphere(cavityOrigin, localCavityDir, localCenter, innerRadius);
        if (innerBackHits.y <= 0.0001)
        {
            return false;
        }

        vec3 localInnerBackPos = cavityOrigin + localCavityDir * innerBackHits.y;
        vec3 localInnerBackNormal = normalize(localInnerBackPos - localCenter);
        vec3 localBackShellDir = refract(localCavityDir, -localInnerBackNormal, 1.0 / max(ior, 1.0));
        if (dot(localBackShellDir, localBackShellDir) <= 0.0001)
        {
            return false;
        }

        vec3 shellBackOrigin = localInnerBackPos + localBackShellDir * (localRadius * 0.001);
        vec2 outerBackHits = intersectSphere(shellBackOrigin, localBackShellDir, localCenter, localRadius);
        if (outerBackHits.y <= 0.0001)
        {
            return false;
        }

        localExitPos = shellBackOrigin + localBackShellDir * outerBackHits.y;
        vec3 localExitNormal = normalize(localExitPos - localCenter);
        localExitDir = refract(localBackShellDir, -localExitNormal, max(ior, 1.0));
        if (dot(localExitDir, localExitDir) <= 0.0001)
        {
            localExitDir = reflect(localBackShellDir, -localExitNormal);
        }
    }
    else
    {
        vec3 solidOrigin = localSurfacePos + localInsideDir * (localRadius * 0.001);
        vec2 solidHits = intersectSphere(solidOrigin, localInsideDir, localCenter, localRadius);
        if (solidHits.y <= 0.0001)
        {
            return false;
        }

        localExitPos = solidOrigin + localInsideDir * solidHits.y;
        vec3 localExitNormal = normalize(localExitPos - localCenter);
        localExitDir = refract(localInsideDir, -localExitNormal, max(ior, 1.0));
        if (dot(localExitDir, localExitDir) <= 0.0001)
        {
            localExitDir = reflect(localInsideDir, -localExitNormal);
        }
    }

    exitWorldPos = transformPointToWorld(localExitPos);
    vec3 worldDirTarget = transformPointToWorld(localExitPos + localExitDir);
    exitWorldDir = normalize(worldDirTarget - exitWorldPos);
    exitUV = projectWorldToScreenUV(exitWorldPos);
    return true;
}

vec2 traceTransmissionRay(vec3 rayOrigin, vec3 rayDir, vec2 fallbackUV)
{
    const int traceSteps = 24;
    const int refineSteps = 4;

    float maxDistance = clamp(u_oitParams.y * 0.08, 12.0, 48.0);
    float stepSize = maxDistance / float(traceSteps);
    float depthBias = 0.03;
    float previousT = 0.0;
    vec2 lastValidUV = clamp(fallbackUV, vec2(0.0), vec2(1.0));

    for (int step = 1; step <= traceSteps; ++step)
    {
        float currentT = stepSize * float(step);
        vec3 samplePos = rayOrigin + rayDir * currentT;
        vec2 sampleUV = projectWorldToScreenUV(samplePos);

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
            sampleUV.y < 0.0 || sampleUV.y > 1.0)
        {
            break;
        }

        lastValidUV = sampleUV;
        float opaqueDepth = texture2D(s_opaqueDepth, sampleUV).r;
        if (opaqueDepth >= 0.9999)
        {
            previousT = currentT;
            continue;
        }

        vec3 opaqueWorldPos = reconstructWorldPos(sampleUV, opaqueDepth);
        float rayViewDepth = getViewDepth(samplePos);
        float opaqueViewDepth = getViewDepth(opaqueWorldPos);
        if (rayViewDepth >= opaqueViewDepth - depthBias)
        {
            float nearT = previousT;
            float farT = currentT;
            vec2 hitUV = sampleUV;

            for (int refine = 0; refine < refineSteps; ++refine)
            {
                float midT = 0.5 * (nearT + farT);
                vec3 midPos = rayOrigin + rayDir * midT;
                vec2 midUV = projectWorldToScreenUV(midPos);
                if (midUV.x < 0.0 || midUV.x > 1.0 ||
                    midUV.y < 0.0 || midUV.y > 1.0)
                {
                    farT = midT;
                    continue;
                }

                float midDepth = texture2D(s_opaqueDepth, midUV).r;
                if (midDepth >= 0.9999)
                {
                    nearT = midT;
                    continue;
                }

                vec3 midOpaqueWorldPos = reconstructWorldPos(midUV, midDepth);
                float midRayDepth = getViewDepth(midPos);
                float midOpaqueDepth = getViewDepth(midOpaqueWorldPos);
                if (midRayDepth >= midOpaqueDepth - depthBias)
                {
                    hitUV = midUV;
                    farT = midT;
                }
                else
                {
                    nearT = midT;
                }
            }

            return hitUV;
        }

        previousT = currentT;
    }

    return lastValidUV;
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
        vec3 traceOrigin = worldPos;
        vec3 traceDirection = vec3(0.0, 0.0, -1.0);
        bool hasTraceRay = false;
        vec3 sphereExitWorldPos;
        vec3 sphereExitWorldDir;
        vec2 sphereExitUV;

        if (solveSphereTransmission(worldPos, ior, sphereExitWorldPos, sphereExitWorldDir, sphereExitUV))
        {
            traceOrigin = sphereExitWorldPos;
            traceDirection = sphereExitWorldDir;
            refractedUV = clamp(sphereExitUV, vec2(0.0), vec2(1.0));
            hasTraceRay = true;
        }
        else
        {
            vec3 refractedDir = refract(-V, N, 1.0 / max(ior, 1.0));
            if (dot(refractedDir, refractedDir) > 0.0001)
            {
                float shellThickness = max(thickness * getMaxModelScale(), 0.0);
                vec3 exitPos = worldPos + refractedDir * shellThickness;
                vec2 exitUV = projectWorldToScreenUV(exitPos);

                if (exitUV.x >= 0.0 && exitUV.x <= 1.0 && exitUV.y >= 0.0 && exitUV.y <= 1.0)
                {
                    traceOrigin = exitPos;
                    traceDirection = normalize(refractedDir);
                    refractedUV = clamp(exitUV, vec2(0.0), vec2(1.0));
                    hasTraceRay = true;
                }
            }
        }

        if (hasTraceRay)
        {
            refractedUV = traceTransmissionRay(traceOrigin, traceDirection, refractedUV);
        }

        vec3 background = texture2D(s_opaqueColor, clamp(refractedUV, vec2(0.0), vec2(1.0))).rgb;

        // Fresnel-based blend: more reflection at glancing angles, more transmission head-on
        float fresnel = pow(1.0 - max(dot(N, V), 0.0), 5.0);

        // Keep probe/direct reflections Fresnel-weighted so the center of a
        // glass object stays transmissive instead of showing a full reflected
        // scene from an impossible angle.
        vec3 reflectedSurface = (specularIBL * ao * ssao * u_iblParams.x * specularAmbientShadow
                               + Lo) * fresnel
                              + emissive;
        float surfaceWeight = clamp((1.0 - transmission) * albedo.a + transmission * fresnel,
                                    0.0, 1.0);

        // Blend refracted background with the thin reflective surface lobe.
        color = mix(background, reflectedSurface, surfaceWeight);

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
