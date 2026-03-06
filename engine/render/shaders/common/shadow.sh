// Shadow mapping functions for PBR shaders
// Uses Texture2DArray with hardware depth comparison (PCF) for shadow sampling.

#ifndef SHADOW_SH
#define SHADOW_SH

// Shadow uniforms
// x=bias, y=normalBias, z=cascadeBlend, w=1.0/shadowMapResolution
uniform vec4 u_shadowParams;
uniform vec4 u_cascadeSplits;    // x,y,z,w = split distances for 4 cascades

// Array of matrices eliminates branching during coordinate projection
uniform mat4 u_shadowMatrix[4];

// Hardware shadow sampler — uses SamplerComparisonState for built-in PCF.
// Depth texture is bound with BGFX_SAMPLER_COMPARE_LEQUAL on the C++ side.
SAMPLER2DARRAYSHADOW(s_shadowMap, 8);

// PCF 3x3 kernel using hardware depth comparison
float sampleShadowMapPCF3x3(vec3 projCoord, int cascade, float bias)
{
    float compareDepth = projCoord.z - bias;
    float shadow = 0.0;

    // Dynamic texel size passed from C++
    float texelSize = u_shadowParams.w;

    // shadow2DArray takes vec4(u, v, layer, depthToCompare)
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            vec2 offset = vec2(float(x) * texelSize, float(y) * texelSize);
            shadow += shadow2DArray(s_shadowMap, vec4(projCoord.xy + offset, float(cascade), compareDepth)).x;
        }
    }

    return shadow / 9.0;
}

int selectCascade(float viewSpaceDepth)
{
    vec4 cmp = step(u_cascadeSplits, vec4_splat(viewSpaceDepth));
    return int(min(dot(cmp, vec4(1.0, 1.0, 1.0, 1.0)), 3.0));
}

vec3 getShadowCoord(vec3 worldPos, int cascade)
{
    // 100% Branchless matrix multiplication
    vec4 shadowPos = mul(u_shadowMatrix[cascade], vec4(worldPos, 1.0));

    vec3 projCoord = shadowPos.xyz / max(shadowPos.w, 0.0001);

    // Remap xy from [-1,1] to [0,1].
    // Y-flip is handled in C++ via bgfx::getCaps()->originBottomLeft
    projCoord.xy = projCoord.xy * 0.5 + 0.5;

    return projCoord;
}

float calculateBias(vec3 normal, vec3 lightDir)
{
    vec3 N = normalize(normal);
    vec3 L = normalize(lightDir);
    float nDotL = clamp(dot(N, L), 0.0, 1.0);

    // Tan-based slope scale: bias grows as tan(theta) for curved surfaces.
    // Much more aggressive at grazing angles than linear (1-NdotL).
    float baseBias = max(u_shadowParams.x, 0.0005);
    float sinTheta = sqrt(1.0 - nDotL * nDotL);
    float tanTheta = sinTheta / max(nDotL, 0.001);
    return baseBias + baseBias * clamp(tanTheta, 0.0, 10.0);
}

float calculateShadow(vec3 worldPos, vec3 normal, vec3 lightDir, float viewSpaceDepth)
{
    int cascade = selectCascade(viewSpaceDepth);

    // Scale normal bias — far cascades cover more world space per texel
    float normalOffset = u_shadowParams.y;
    vec4 scales = u_cascadeSplits / max(u_cascadeSplits.x, 0.001);
    float cascadeScale;
    if (cascade == 0) cascadeScale = scales.x;
    else if (cascade == 1) cascadeScale = scales.y;
    else if (cascade == 2) cascadeScale = scales.z;
    else cascadeScale = scales.w;
    normalOffset *= cascadeScale;
    worldPos += normal * normalOffset;

    vec3 shadowCoord = getShadowCoord(worldPos, cascade);

    // Bounds check — allow z > 1.0 so objects behind cascade far plane still cast shadows
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0)
    {
        return 1.0;
    }
    shadowCoord.z = saturate(shadowCoord.z);

    float bias = calculateBias(normal, lightDir);

    float shadow = sampleShadowMapPCF3x3(shadowCoord, cascade, bias);

    // Cascade blending
    float blendDistance = u_shadowParams.z;

    if (cascade < 3)
    {
        float splitDist;
        if (cascade == 0) splitDist = u_cascadeSplits.x;
        else if (cascade == 1) splitDist = u_cascadeSplits.y;
        else splitDist = u_cascadeSplits.z;

        float blendStart = splitDist * (1.0 - blendDistance);

        if (viewSpaceDepth > blendStart)
        {
            float blendFactor = clamp((viewSpaceDepth - blendStart) / (splitDist - blendStart), 0.0, 1.0);

            int nextCascade = cascade + 1;
            vec3 nextShadowCoord = getShadowCoord(worldPos, nextCascade);

            if (nextShadowCoord.x >= 0.0 && nextShadowCoord.x <= 1.0 &&
                nextShadowCoord.y >= 0.0 && nextShadowCoord.y <= 1.0 &&
                nextShadowCoord.z >= 0.0)
            {
                nextShadowCoord.z = saturate(nextShadowCoord.z);
                float nextShadow = sampleShadowMapPCF3x3(nextShadowCoord, nextCascade, bias);
                shadow = mix(shadow, nextShadow, blendFactor);
            }
        }
    }

    // Smooth fade at max shadow distance instead of abrupt cutoff
    float maxShadowDist = u_cascadeSplits.w;
    float fadeStart = maxShadowDist * 0.8;
    float distanceFade = 1.0 - saturate((viewSpaceDepth - fadeStart) / (maxShadowDist - fadeStart));
    shadow = mix(1.0, shadow, distanceFade);

    return shadow;
}

#endif // SHADOW_SH
