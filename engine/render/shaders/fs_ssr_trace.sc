$input v_texcoord0

#include <bgfx_shader.sh>

// Screen-Space Reflections Trace Fragment Shader
// Performs ray marching in screen space to find reflections
// Uses Hi-Z acceleration for efficient tracing

SAMPLER2D(s_color, 0);
SAMPLER2D(s_depth, 1);
SAMPLER2D(s_normal, 2);
SAMPLER2D(s_roughness, 3);
SAMPLER2D(s_hiz, 4);

uniform mat4 u_viewMatrix;
uniform mat4 u_projMatrix;
uniform mat4 u_invProjMatrix;
uniform mat4 u_invViewMatrix;

uniform vec4 u_ssrParams;   // x=max_steps, y=max_distance, z=thickness, w=stride
uniform vec4 u_ssrParams2;  // x=stride_cutoff, y=roughness_threshold, z=jitter, w=frame
uniform vec4 u_texelSize;   // xy=texel size, zw=resolution

// Reconstruct view-space position from depth
vec3 getViewPosition(vec2 uv, float depth)
{
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = mul(u_invProjMatrix, clipPos);
    return viewPos.xyz / viewPos.w;
}

// Project view-space position to screen UV
vec3 projectToScreen(vec3 viewPos)
{
    vec4 clipPos = mul(u_projMatrix, vec4(viewPos, 1.0));
    vec3 ndc = clipPos.xyz / clipPos.w;
    return vec3(ndc.xy * 0.5 + 0.5, ndc.z);
}

// Linear interpolation
float linearDepth(float depth, float near, float far)
{
    return near * far / (far - depth * (far - near));
}

// Simple noise for jittering
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main()
{
    vec2 uv = v_texcoord0;

    // Sample roughness - skip highly rough surfaces
    float roughness = texture2D(s_roughness, uv).r;
    float roughnessThreshold = u_ssrParams2.y;

    if (roughness > roughnessThreshold)
    {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    // Sample depth and reconstruct position
    float depth = texture2D(s_depth, uv).r;

    // Skip sky (depth ~= 1.0)
    if (depth > 0.9999)
    {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    vec3 viewPos = getViewPosition(uv, depth);

    // Sample normal (in view space - convert from world space if needed)
    vec3 worldNormal = texture2D(s_normal, uv).xyz * 2.0 - 1.0;
    vec3 viewNormal = mul(u_viewMatrix, vec4(worldNormal, 0.0)).xyz;
    viewNormal = normalize(viewNormal);

    // Calculate view direction and reflection direction
    vec3 viewDir = normalize(viewPos);
    vec3 reflectDir = reflect(viewDir, viewNormal);

    // SSR parameters
    float maxSteps = u_ssrParams.x;
    float maxDistance = u_ssrParams.y;
    float thickness = u_ssrParams.z;
    float stride = u_ssrParams.w;
    float strideCutoff = u_ssrParams2.x;

    // Jitter starting position
    float jitter = 0.0;
    if (u_ssrParams2.z > 0.5)
    {
        float frame = u_ssrParams2.w;
        jitter = hash(uv * u_texelSize.zw + frame) * stride;
    }

    // Starting point
    vec3 rayStart = viewPos;
    vec3 rayEnd = viewPos + reflectDir * maxDistance;

    // Project start and end to screen space
    vec3 screenStart = projectToScreen(rayStart);
    vec3 screenEnd = projectToScreen(rayEnd);

    // Screen-space ray direction
    vec3 screenDir = screenEnd - screenStart;

    // Calculate step size based on screen-space distance
    float screenLength = length(screenDir.xy * u_texelSize.zw);
    float stepSize = 1.0 / max(screenLength, 1.0);

    // Ray march
    vec3 hitUV = vec3(0.0, 0.0, 0.0);
    float hitConfidence = 0.0;

    float t = jitter * stepSize;
    for (float i = 0.0; i < maxSteps; i += 1.0)
    {
        if (t > 1.0) break;

        vec3 screenPos = screenStart + screenDir * t;
        vec2 sampleUV = screenPos.xy;

        // Check bounds
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
            sampleUV.y < 0.0 || sampleUV.y > 1.0)
        {
            break;
        }

        // Sample scene depth at current position
        float sceneDepth = texture2DLod(s_depth, sampleUV, 0.0).r;

        // Compare depths
        float rayDepth = screenPos.z;
        float depthDiff = rayDepth - sceneDepth;

        // Check for intersection
        if (depthDiff > 0.0 && depthDiff < thickness)
        {
            // Found intersection - binary search refinement
            float tMin = t - stepSize;
            float tMax = t;

            for (int j = 0; j < 4; j++)
            {
                float tMid = (tMin + tMax) * 0.5;
                vec3 midPos = screenStart + screenDir * tMid;
                float midSceneDepth = texture2DLod(s_depth, midPos.xy, 0.0).r;
                float midDepthDiff = midPos.z - midSceneDepth;

                if (midDepthDiff > 0.0)
                {
                    tMax = tMid;
                }
                else
                {
                    tMin = tMid;
                }
            }

            vec3 refinedPos = screenStart + screenDir * tMax;
            hitUV = vec3(refinedPos.xy, tMax);

            // Calculate confidence based on various factors
            // 1. Edge fade
            vec2 edgeFade = abs(refinedPos.xy - 0.5) * 2.0;
            float edgeConfidence = 1.0 - max(edgeFade.x, edgeFade.y);
            edgeConfidence = clamp(edgeConfidence * 4.0, 0.0, 1.0);

            // 2. Distance fade
            float distanceConfidence = 1.0 - (tMax * maxDistance) / maxDistance;
            distanceConfidence = clamp(distanceConfidence, 0.0, 1.0);

            // 3. Roughness fade
            float roughnessConfidence = 1.0 - roughness / roughnessThreshold;

            hitConfidence = edgeConfidence * distanceConfidence * roughnessConfidence;
            break;
        }

        // Adaptive stride - use larger steps when far from surfaces
        float adaptiveStride = stride;
        if (screenLength > strideCutoff)
        {
            adaptiveStride *= 2.0;
        }

        t += stepSize * adaptiveStride;
    }

    // Output: xy = hit UV, z = confidence, w = unused
    gl_FragColor = vec4(hitUV.xy, hitConfidence, 1.0);
}
