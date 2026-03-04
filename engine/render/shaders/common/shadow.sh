// Shadow mapping functions for PBR shaders
// Uses Texture2DArray (R32F) to eliminate branching overhead.

#ifndef SHADOW_SH
#define SHADOW_SH

// Shadow uniforms
// x=bias, y=normalBias, z=cascadeBlend, w=1.0/shadowMapResolution
uniform vec4 u_shadowParams;     
uniform vec4 u_cascadeSplits;    // x,y,z,w = split distances for 4 cascades

// Array of matrices eliminates branching during coordinate projection
uniform mat4 u_shadowMatrix[4];  

// Single Texture2DArray for all cascades eliminates branching during sampling
SAMPLER2DARRAY(s_shadowMap, 8);

// PCF 3x3 kernel for Texture2DArray
float sampleShadowMapPCF3x3(vec3 projCoord, int cascade, float bias)
{
    float currentDepth = projCoord.z - bias;
    float shadow = 0.0;
    
    // Dynamic texel size passed from C++
    float texelSize = u_shadowParams.w; 

    // texture2DArrayLod takes a vec3: (u, v, layer_index)
    vec3 coord = vec3(projCoord.xy, float(cascade));

    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            vec3 offsetCoord = coord + vec3(float(x) * texelSize, float(y) * texelSize, 0.0);
            float storedDepth = texture2DArrayLod(s_shadowMap, offsetCoord, 0.0).r;
            shadow += step(currentDepth, storedDepth);
        }
    }

    return shadow / 9.0;
}

int selectCascade(float viewSpaceDepth)
{
    if (viewSpaceDepth < u_cascadeSplits.x) return 0;
    if (viewSpaceDepth < u_cascadeSplits.y) return 1;
    if (viewSpaceDepth < u_cascadeSplits.z) return 2;
    return 3;
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

    // Slope-scaled bias to reduce acne at grazing angles.
    return max(0.005 * (1.0 - nDotL), 0.0005);
}

float calculateShadow(vec3 worldPos, vec3 normal, vec3 lightDir, float viewSpaceDepth)
{
    float normalOffset = u_shadowParams.y;
    worldPos += normal * normalOffset;

    int cascade = selectCascade(viewSpaceDepth);
    vec3 shadowCoord = getShadowCoord(worldPos, cascade);

    // Bounds check disabled
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0 || shadowCoord.z > 1.0)
    {
        return 1.0;
    }

    float bias = calculateBias(normal, lightDir);
    
    // Branchless sampling
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
            
            // Validate next cascade bounds before sampling
            if (nextShadowCoord.x >= 0.0 && nextShadowCoord.x <= 1.0 &&
                nextShadowCoord.y >= 0.0 && nextShadowCoord.y <= 1.0 &&
                nextShadowCoord.z >= 0.0 && nextShadowCoord.z <= 1.0)
            {
                float nextShadow = sampleShadowMapPCF3x3(nextShadowCoord, nextCascade, bias);
                shadow = mix(shadow, nextShadow, blendFactor);
            }
        }
    }

    return shadow;
}

#endif // SHADOW_SH
