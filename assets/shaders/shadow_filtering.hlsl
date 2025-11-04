// shadow_filtering.hlsl - Advanced shadow filtering functions
// Ported from GLSL to HLSL for use with DXC compiler
#ifndef SHADOW_FILTERING_HLSL
#define SHADOW_FILTERING_HLSL

// ============================================================================
// CONSTANTS
// ============================================================================

static const float PI = 3.14159265359;

// PCSS parameters
static const int PCSS_BLOCKER_SEARCH_SAMPLES = 16;
static const int PCSS_PCF_SAMPLES = 32;
static const float PCSS_LIGHT_SIZE = 0.05;  // Light source size in world units

// Contact-hardening parameters
static const int CH_PCF_SAMPLES = 16;
static const float CH_MAX_RADIUS = 5.0;

// EVSM parameters
static const float EVSM_EPSILON = 0.0001;

// ============================================================================
// SAMPLING PATTERNS
// ============================================================================

// Vogel disk sampling pattern (better distribution than Poisson)
float2 VogelDiskSample(int sampleIndex, int samplesCount, float phi) {
    float goldenAngle = 2.4;
    float r = sqrt(float(sampleIndex) + 0.5) / sqrt(float(samplesCount));
    float theta = float(sampleIndex) * goldenAngle + phi;
    return float2(r * cos(theta), r * sin(theta));
}

// Random rotation angle based on screen position
float InterleavedGradientNoise(float2 screenPos) {
    float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(magic.z * frac(dot(screenPos, magic.xy)));
}

// ============================================================================
// BASIC PCF (Existing - for reference)
// ============================================================================

float PCF_DirectionalShadow(Texture2DArray shadowMap, SamplerComparisonState shadowSampler,
                           float4 shadowCoord, int cascadeIndex, float bias, float radius) {
    float shadow = 0.0;

    // Get texture dimensions
    float width, height, elements;
    shadowMap.GetDimensions(width, height, elements);
    float2 texelSize = 1.0 / float2(width, height);

    int samples = 9; // 3x3 kernel
    int halfSamples = 1;

    for (int x = -halfSamples; x <= halfSamples; ++x) {
        for (int y = -halfSamples; y <= halfSamples; ++y) {
            float2 offset = float2(x, y) * texelSize * radius;
            float3 sampleCoord = float3(shadowCoord.xy + offset, float(cascadeIndex));
            float compareDepth = shadowCoord.z - bias;

            // Hardware PCF comparison
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, sampleCoord, compareDepth);
        }
    }

    return shadow / float(samples);
}

// ============================================================================
// PCSS (Percentage-Closer Soft Shadows)
// ============================================================================

// Step 1: Search for blockers and compute average blocker depth
// Uses raw depth texture to read actual depth values
float PCSS_BlockerSearch(Texture2DArray rawDepthMap, SamplerState rawSampler,
                         float3 shadowCoord, int cascadeIndex, float searchRadius,
                         float receiverDepth) {
    // Get texture dimensions
    float width, height, elements;
    rawDepthMap.GetDimensions(width, height, elements);
    float2 texelSize = 1.0 / float2(width, height);

    float phi = InterleavedGradientNoise(shadowCoord.xy * float2(width, height)) * 2.0 * PI;

    float blockerSum = 0.0;
    int numBlockers = 0;

    // Search for blockers in a disk around the sample point
    for (int i = 0; i < PCSS_BLOCKER_SEARCH_SAMPLES; ++i) {
        float2 offset = VogelDiskSample(i, PCSS_BLOCKER_SEARCH_SAMPLES, phi) * searchRadius;
        float3 sampleCoord = float3(shadowCoord.xy + offset * texelSize, float(cascadeIndex));

        // Sample raw depth value (no comparison)
        float blockerDepth = rawDepthMap.SampleLevel(rawSampler, sampleCoord, 0).r;

        // Check if this sample is a blocker (closer to light than receiver)
        if (blockerDepth < receiverDepth) {
            blockerSum += blockerDepth;
            numBlockers++;
        }
    }

    // Return average blocker depth, or -1 if no blockers found
    if (numBlockers > 0) {
        return blockerSum / float(numBlockers);
    }
    return -1.0;
}

// Step 2: Estimate penumbra size based on blocker distance
float PCSS_PenumbraSize(float receiverDepth, float avgBlockerDepth, float lightSize) {
    if (avgBlockerDepth < 0.0) {
        return 0.0; // No blockers
    }

    // Penumbra ratio = (receiver - blocker) / blocker
    float penumbra = (receiverDepth - avgBlockerDepth) / avgBlockerDepth;
    return penumbra * lightSize;
}

// Step 3: PCF with variable kernel size (using hardware comparison sampler)
float PCSS_PCF(Texture2DArray shadowMap, SamplerComparisonState shadowSampler,
               float3 shadowCoord, int cascadeIndex, float filterRadius, float bias) {
    // Get texture dimensions
    float width, height, elements;
    shadowMap.GetDimensions(width, height, elements);
    float2 texelSize = 1.0 / float2(width, height);

    float phi = InterleavedGradientNoise(shadowCoord.xy) * 2.0 * PI;

    float shadow = 0.0;

    for (int i = 0; i < PCSS_PCF_SAMPLES; ++i) {
        float2 offset = VogelDiskSample(i, PCSS_PCF_SAMPLES, phi) * filterRadius;
        float3 sampleCoord = float3(shadowCoord.xy + offset * texelSize, float(cascadeIndex));
        float compareDepth = shadowCoord.z - bias;

        shadow += shadowMap.SampleCmpLevelZero(shadowSampler, sampleCoord, compareDepth);
    }

    return shadow / float(PCSS_PCF_SAMPLES);
}

// Complete PCSS pipeline
float PCSS_DirectionalShadow(Texture2DArray shadowMap, SamplerComparisonState shadowSampler,
                             Texture2DArray rawDepthMap, SamplerState rawSampler,
                             float4 shadowCoord, int cascadeIndex, float bias, float searchRadius) {
    float receiverDepth = shadowCoord.z;

    // Step 1: Blocker search with raw depth access
    float avgBlockerDepth = PCSS_BlockerSearch(rawDepthMap, rawSampler, shadowCoord.xyz,
                                                cascadeIndex, searchRadius, receiverDepth);

    if (avgBlockerDepth < 0.0) {
        return 1.0; // No blockers, fully lit
    }

    // Step 2: Penumbra estimation
    float penumbraSize = PCSS_PenumbraSize(receiverDepth, avgBlockerDepth, PCSS_LIGHT_SIZE);

    // Step 3: PCF with variable filter
    float shadow = PCSS_PCF(shadowMap, shadowSampler, shadowCoord.xyz, cascadeIndex, penumbraSize, bias);

    return shadow;
}

// ============================================================================
// CONTACT-HARDENING SHADOWS (Simplified PCSS)
// ============================================================================

float ContactHardeningShadow(Texture2DArray shadowMap, SamplerComparisonState shadowSampler,
                            Texture2DArray rawDepthMap, SamplerState rawSampler,
                            float4 shadowCoord, int cascadeIndex, float bias, float maxRadius) {
    // Get texture dimensions
    float width, height, elements;
    shadowMap.GetDimensions(width, height, elements);
    float2 texelSize = 1.0 / float2(width, height);

    float phi = InterleavedGradientNoise(shadowCoord.xy) * 2.0 * PI;

    // Sample depth at center
    float receiverDepth = shadowCoord.z;

    // Estimate average depth difference from nearby shadow map samples
    // This determines how "hard" the shadow should be
    float depthDiffSum = 0.0;
    int numSamples = 4;

    for (int i = 0; i < numSamples; ++i) {
        float2 offset = VogelDiskSample(i, numSamples, phi) * maxRadius * 0.5;
        float3 sampleCoord = float3(shadowCoord.xy + offset * texelSize, float(cascadeIndex));

        float occluderDepth = rawDepthMap.SampleLevel(rawSampler, sampleCoord, 0).r;
        depthDiffSum += abs(receiverDepth - occluderDepth);
    }

    float avgDepthDiff = depthDiffSum / float(numSamples);

    // Calculate filter radius based on depth difference
    // Close to occluder = small radius (hard shadow)
    // Far from occluder = large radius (soft shadow)
    float filterRadius = clamp(avgDepthDiff * 100.0, maxRadius * 0.1, maxRadius);

    float shadow = 0.0;
    float compareDepth = receiverDepth - bias;

    for (int i = 0; i < CH_PCF_SAMPLES; ++i) {
        float2 offset = VogelDiskSample(i, CH_PCF_SAMPLES, phi) * filterRadius;
        float3 sampleCoord = float3(shadowCoord.xy + offset * texelSize, float(cascadeIndex));

        shadow += shadowMap.SampleCmpLevelZero(shadowSampler, sampleCoord, compareDepth);
    }

    return shadow / float(CH_PCF_SAMPLES);
}

// ============================================================================
// EVSM (Exponential Variance Shadow Maps)
// ============================================================================

// Compute EVSM moments from depth
float2 ComputeEVSMExponents(float depth, float positiveExponent, float negativeExponent) {
    float2 exponents = float2(positiveExponent, negativeExponent);
    // Warp depth
    float pos = exp(exponents.x * depth);
    float neg = -exp(-exponents.y * depth);
    return float2(pos, neg);
}

// Chebyshev's inequality for upper bound on probability
float ChebyshevUpperBound(float distance, float2 moments) {
    float variance = moments.y - (moments.x * moments.x);
    variance = max(variance, EVSM_EPSILON);

    float d = distance - moments.x;
    float pMax = variance / (variance + d * d);

    return (distance <= moments.x) ? 1.0 : pMax;
}

// Reduce light bleeding
float ReduceLightBleeding(float pMax, float amount) {
    return smoothstep(amount, 1.0, pMax);
}

float EVSM_Shadow(Texture2D evsmMap, SamplerState evsmSampler, float2 shadowCoord, float receiverDepth,
                  float positiveExponent, float negativeExponent, float lightBleedReduction) {
    // Sample pre-filtered EVSM moments
    float4 moments = evsmMap.Sample(evsmSampler, shadowCoord);

    // Positive test
    float2 posWarpedDepth = ComputeEVSMExponents(receiverDepth, positiveExponent, 0.0);
    float posShadow = ChebyshevUpperBound(posWarpedDepth.x, moments.xy);

    // Negative test
    float2 negWarpedDepth = ComputeEVSMExponents(receiverDepth, 0.0, negativeExponent);
    float negShadow = ChebyshevUpperBound(negWarpedDepth.y, moments.zw);

    // Combine
    float shadow = min(posShadow, negShadow);

    // Reduce light bleeding
    shadow = ReduceLightBleeding(shadow, lightBleedReduction);

    return shadow;
}

// EVSM variant for Texture2DArray (cascaded shadow maps)
float EVSM_DirectionalShadow(Texture2DArray evsmMap, SamplerState evsmSampler, float4 shadowCoord,
                             int cascadeIndex, float positiveExponent, float negativeExponent,
                             float lightBleedReduction) {
    // Sample pre-filtered EVSM moments from array
    float3 sampleCoord = float3(shadowCoord.xy, float(cascadeIndex));
    float4 moments = evsmMap.Sample(evsmSampler, sampleCoord);

    float receiverDepth = shadowCoord.z;

    // Positive test
    float2 posWarpedDepth = ComputeEVSMExponents(receiverDepth, positiveExponent, 0.0);
    float posShadow = ChebyshevUpperBound(posWarpedDepth.x, moments.xy);

    // Negative test
    float2 negWarpedDepth = ComputeEVSMExponents(receiverDepth, 0.0, negativeExponent);
    float negShadow = ChebyshevUpperBound(negWarpedDepth.y, moments.zw);

    // Combine
    float shadow = min(posShadow, negShadow);

    // Reduce light bleeding
    shadow = ReduceLightBleeding(shadow, lightBleedReduction);

    return shadow;
}

#endif // SHADOW_FILTERING_HLSL
