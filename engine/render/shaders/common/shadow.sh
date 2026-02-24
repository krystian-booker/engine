// Shadow mapping functions for PBR shaders
// Uses color-based shadow maps (R32F) with manual depth comparison
// to avoid D3D11 TYPELESS/COMPARE_LEQUAL issues.

#ifndef SHADOW_SH
#define SHADOW_SH

// Shadow uniforms
uniform vec4 u_shadowParams;     // x=bias, y=normalBias, z=cascadeBlend, w=pcfRadius
uniform vec4 u_cascadeSplits;    // x,y,z,w = split distances for 4 cascades
uniform mat4 u_shadowMatrix0;    // Cascade 0 light space matrix
uniform mat4 u_shadowMatrix1;    // Cascade 1 light space matrix
uniform mat4 u_shadowMatrix2;    // Cascade 2 light space matrix
uniform mat4 u_shadowMatrix3;    // Cascade 3 light space matrix

// Shadow samplers - regular 2D samplers (color-based shadow maps)
SAMPLER2D(s_shadowMap0, 8);
SAMPLER2D(s_shadowMap1, 9);
SAMPLER2D(s_shadowMap2, 10);
SAMPLER2D(s_shadowMap3, 11);

// Sample a single shadow map with manual depth comparison
float sampleShadowMap(sampler2D shadowMap, vec3 shadowCoord, float bias)
{
    // Use texture2DLod to avoid gradient issues in divergent control flow
    float storedDepth = texture2DLod(shadowMap, shadowCoord.xy, 0.0).r;
    float currentDepth = shadowCoord.z - bias;
    return step(currentDepth, storedDepth);
}

// PCF 3x3 kernel for smooth shadows (manual comparison)
// Uses texture2DLod (SampleLevel) to avoid HLSL gradient-in-loop errors
float sampleShadowMapPCF3x3(sampler2D shadowMap, vec3 shadowCoord, float bias, vec2 texelSize)
{
    float currentDepth = shadowCoord.z - bias;
    float shadow = 0.0;

    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            float storedDepth = texture2DLod(shadowMap, shadowCoord.xy + offset, 0.0).r;
            shadow += step(currentDepth, storedDepth);
        }
    }

    return shadow / 9.0;
}

// Select cascade based on view-space depth
int selectCascade(float viewSpaceDepth)
{
    if (viewSpaceDepth < u_cascadeSplits.x) return 0;
    if (viewSpaceDepth < u_cascadeSplits.y) return 1;
    if (viewSpaceDepth < u_cascadeSplits.z) return 2;
    return 3;
}

// Get shadow coordinate for a specific cascade
vec3 getShadowCoord(vec3 worldPos, int cascade)
{
    vec4 shadowPos;

    // Use mul() not instMul() for custom Mat4 uniforms.
    // bgfx uploads GLM column-major data via raw memCopy into column_major
    // HLSL cbuffers, so mul(M, v) = M * v gives the correct result.
    // instMul(M, v) = mul(v, M) in HLSL would compute M^T * v (wrong).
    if (cascade == 0)
        shadowPos = mul(u_shadowMatrix0, vec4(worldPos, 1.0));
    else if (cascade == 1)
        shadowPos = mul(u_shadowMatrix1, vec4(worldPos, 1.0));
    else if (cascade == 2)
        shadowPos = mul(u_shadowMatrix2, vec4(worldPos, 1.0));
    else
        shadowPos = mul(u_shadowMatrix3, vec4(worldPos, 1.0));

    // Perspective divide
    // Guard w to avoid HLSL compile-time division-by-zero warning
    vec3 projCoord = shadowPos.xyz / max(shadowPos.w, 0.0001);

    // Remap xy from [-1,1] to [0,1] for UV lookup.
    // z is already in [0,1] (orthoRH_ZO projection), so leave it as-is.
    projCoord.xy = projCoord.xy * 0.5 + 0.5;

    // D3D11 Y-flip: clip Y=+1 maps to texture row 0 (v=0),
    // but our remap gives v=1.0 for Y=+1. Flip to correct.
    projCoord.y = 1.0 - projCoord.y;

    return projCoord;
}

// Sample shadow from specific cascade
float sampleCascade(int cascade, vec3 shadowCoord, float bias, vec2 texelSize)
{
    if (cascade == 0)
        return sampleShadowMapPCF3x3(s_shadowMap0, shadowCoord, bias, texelSize);
    else if (cascade == 1)
        return sampleShadowMapPCF3x3(s_shadowMap1, shadowCoord, bias, texelSize);
    else if (cascade == 2)
        return sampleShadowMapPCF3x3(s_shadowMap2, shadowCoord, bias, texelSize);
    else
        return sampleShadowMapPCF3x3(s_shadowMap3, shadowCoord, bias, texelSize);
}

// Calculate slope-scale bias based on surface angle
float calculateBias(vec3 normal, vec3 lightDir)
{
    float baseBias = u_shadowParams.x;
    float normalBias = u_shadowParams.y;

    float cosTheta = clamp(dot(normal, lightDir), 0.0, 1.0);
    float slopeBias = baseBias * tan(acos(cosTheta));
    slopeBias = clamp(slopeBias, 0.0, baseBias * 2.0);

    return baseBias + slopeBias;
}

// Main shadow calculation with cascade selection and blending
float calculateShadow(vec3 worldPos, vec3 normal, vec3 lightDir, float viewSpaceDepth)
{
    // Apply normal offset to reduce shadow acne
    float normalOffset = u_shadowParams.y;
    worldPos += normal * normalOffset;

    // Select cascade
    int cascade = selectCascade(viewSpaceDepth);

    // Get shadow coordinates
    vec3 shadowCoord = getShadowCoord(worldPos, cascade);

    // Check if were outside shadow map bounds
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0 || shadowCoord.z > 1.0)
    {
        return 1.0;  // Not in shadow
    }

    // Calculate bias
    float bias = calculateBias(normal, lightDir);

    // Texel size (assuming 2048 resolution, can be made uniform)
    vec2 texelSize = vec2_splat(1.0 / 2048.0);

    // Sample shadow
    float shadow = sampleCascade(cascade, shadowCoord, bias, texelSize);

    // Cascade blending for smooth transitions
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
            float blendFactor = (viewSpaceDepth - blendStart) / (splitDist - blendStart);
            blendFactor = clamp(blendFactor, 0.0, 1.0);

            // Sample next cascade
            int nextCascade = cascade + 1;
            vec3 nextShadowCoord = getShadowCoord(worldPos, nextCascade);
            float nextShadow = sampleCascade(nextCascade, nextShadowCoord, bias, texelSize);

            shadow = mix(shadow, nextShadow, blendFactor);
        }
    }

    return shadow;
}

// Simplified shadow calculation without cascade blending (faster)
float calculateShadowSimple(vec3 worldPos, vec3 normal, vec3 lightDir, float viewSpaceDepth)
{
    float normalOffset = u_shadowParams.y;
    worldPos += normal * normalOffset;

    int cascade = selectCascade(viewSpaceDepth);
    vec3 shadowCoord = getShadowCoord(worldPos, cascade);

    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0 || shadowCoord.z > 1.0)
    {
        return 1.0;
    }

    float bias = calculateBias(normal, lightDir);
    vec2 texelSize = vec2_splat(1.0 / 2048.0);

    return sampleCascade(cascade, shadowCoord, bias, texelSize);
}

#endif // SHADOW_SH
