// Shadow mapping functions for PBR shaders

#ifndef SHADOW_SH
#define SHADOW_SH

// Shadow uniforms
uniform vec4 u_shadowParams;     // x=bias, y=normalBias, z=cascadeBlend, w=pcfRadius
uniform vec4 u_cascadeSplits;    // x,y,z,w = split distances for 4 cascades
uniform mat4 u_shadowMatrix0;    // Cascade 0 light space matrix
uniform mat4 u_shadowMatrix1;    // Cascade 1 light space matrix
uniform mat4 u_shadowMatrix2;    // Cascade 2 light space matrix
uniform mat4 u_shadowMatrix3;    // Cascade 3 light space matrix

// Shadow samplers - using comparison samplers for hardware PCF
SAMPLER2DSHADOW(s_shadowMap0, 8);
SAMPLER2DSHADOW(s_shadowMap1, 9);
SAMPLER2DSHADOW(s_shadowMap2, 10);
SAMPLER2DSHADOW(s_shadowMap3, 11);

// Poisson disk for PCF sampling
vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),
    vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),
    vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),
    vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),
    vec2(0.14383161, -0.14100790)
);

// Sample a single shadow map with hardware PCF
float sampleShadowMapPCF(sampler2DShadow shadowMap, vec3 shadowCoord, float bias)
{
    // Apply bias
    shadowCoord.z -= bias;

    // Single hardware PCF sample
    return shadow2D(shadowMap, shadowCoord);
}

// Sample shadow map with software PCF (Poisson disk)
float sampleShadowMapSoftPCF(sampler2DShadow shadowMap, vec3 shadowCoord, float bias, vec2 texelSize, int samples)
{
    shadowCoord.z -= bias;

    float shadow = 0.0;
    float radius = u_shadowParams.w;

    for (int i = 0; i < samples; i++)
    {
        vec2 offset = poissonDisk[i] * radius * texelSize;
        shadow += shadow2D(shadowMap, vec3(shadowCoord.xy + offset, shadowCoord.z));
    }

    return shadow / float(samples);
}

// PCF 3x3 kernel for smooth shadows
float sampleShadowMapPCF3x3(sampler2DShadow shadowMap, vec3 shadowCoord, float bias, vec2 texelSize)
{
    shadowCoord.z -= bias;

    float shadow = 0.0;

    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            shadow += shadow2D(shadowMap, vec3(shadowCoord.xy + offset, shadowCoord.z));
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

    if (cascade == 0)
        shadowPos = mul(u_shadowMatrix0, vec4(worldPos, 1.0));
    else if (cascade == 1)
        shadowPos = mul(u_shadowMatrix1, vec4(worldPos, 1.0));
    else if (cascade == 2)
        shadowPos = mul(u_shadowMatrix2, vec4(worldPos, 1.0));
    else
        shadowPos = mul(u_shadowMatrix3, vec4(worldPos, 1.0));

    // Perspective divide and remap to [0,1]
    vec3 projCoord = shadowPos.xyz / shadowPos.w;
    projCoord = projCoord * 0.5 + 0.5;

    // Flip Y for DirectX
#if BGFX_SHADER_LANGUAGE_HLSL
    projCoord.y = 1.0 - projCoord.y;
#endif

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

    // Check if we're outside shadow map bounds
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0 || shadowCoord.z > 1.0)
    {
        return 1.0;  // Not in shadow
    }

    // Calculate bias
    float bias = calculateBias(normal, lightDir);

    // Texel size (assuming 2048 resolution, can be made uniform)
    vec2 texelSize = vec2(1.0 / 2048.0);

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
    vec2 texelSize = vec2(1.0 / 2048.0);

    return sampleCascade(cascade, shadowCoord, bias, texelSize);
}

#endif // SHADOW_SH
