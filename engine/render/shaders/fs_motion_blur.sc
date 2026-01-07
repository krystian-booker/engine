$input v_texcoord0

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_color, 0);           // Scene color
SAMPLER2D(s_depth, 1);           // Scene depth
SAMPLER2D(s_velocity, 2);        // Velocity buffer
SAMPLER2D(s_neighborMax, 3);     // Neighbor max velocity (tile-based)

// Parameters
uniform vec4 u_motionParams;     // x=intensity, y=max_blur_radius, z=min_threshold, w=sample_count
uniform vec4 u_motionParams2;    // x=jitter, y=depth_falloff, z=center_falloff_start, w=center_falloff_end
uniform vec4 u_texelSize;        // xy=1/size, zw=size

// Pseudo-random number for jittering
float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec2 uv = v_texcoord0;
    vec2 texel = u_texelSize.xy;

    // Get parameters
    float intensity = u_motionParams.x;
    float maxBlurRadius = u_motionParams.y;
    float minThreshold = u_motionParams.z;
    int sampleCount = int(u_motionParams.w);

    float useJitter = u_motionParams2.x;
    float depthFalloff = u_motionParams2.y;
    float centerStart = u_motionParams2.z;
    float centerEnd = u_motionParams2.w;

    // Sample velocity at current pixel
    vec2 velocity = texture2D(s_velocity, uv).rg;

    // Decode velocity from [0, 1] to actual velocity
    velocity = (velocity - 0.5) * 2.0 * maxBlurRadius;

    // Get neighbor max velocity for tile-based blur
    vec2 neighborVel = texture2D(s_neighborMax, uv).rg;
    neighborVel = (neighborVel - 0.5) * 2.0 * maxBlurRadius;

    // Use maximum of local and neighbor velocity for scatter blur
    float localMag = length(velocity);
    float neighborMag = length(neighborVel);

    // If neighbor has stronger motion, use it (helps with foreground bleeding)
    if (neighborMag > localMag)
    {
        velocity = neighborVel;
    }

    float velocityMag = length(velocity);

    // Early out if velocity is below threshold
    if (velocityMag < minThreshold)
    {
        gl_FragColor = texture2D(s_color, uv);
        return;
    }

    // Scale velocity by intensity
    velocity *= intensity;
    velocityMag *= intensity;

    // Center attenuation (reduce blur near screen center for gameplay clarity)
    if (centerEnd > 0.0)
    {
        vec2 centerDist = abs(uv - 0.5) * 2.0;
        float distFromCenter = max(centerDist.x, centerDist.y);
        float centerMask = smoothstep(centerStart, centerEnd, distFromCenter);
        velocity *= centerMask;
        velocityMag *= centerMask;
    }

    // Clamp to max blur radius in pixels
    float maxPixels = maxBlurRadius / texel.x;
    if (velocityMag > maxPixels * texel.x)
    {
        velocity = normalize(velocity) * maxPixels * texel.x;
    }

    // Sample center pixel
    vec4 centerColor = texture2D(s_color, uv);
    float centerDepth = texture2D(s_depth, uv).r;

    // Accumulate samples along velocity direction
    vec4 colorSum = centerColor;
    float weightSum = 1.0;

    // Calculate step size
    vec2 step = velocity / float(sampleCount);

    // Optional jitter to reduce banding
    float jitterOffset = 0.0;
    if (useJitter > 0.5)
    {
        jitterOffset = rand(uv + vec2_splat(0.5)) - 0.5;
    }

    // Sample in both directions along velocity vector
    for (int i = 1; i <= sampleCount; i++)
    {
        float t = (float(i) + jitterOffset) / float(sampleCount);

        // Sample in positive direction
        vec2 sampleUV = uv + step * t;
        if (sampleUV.x >= 0.0 && sampleUV.x <= 1.0 && sampleUV.y >= 0.0 && sampleUV.y <= 1.0)
        {
            vec4 sampleColor = texture2D(s_color, sampleUV);
            float sampleDepth = texture2D(s_depth, sampleUV).r;

            // Depth-aware blending (objects in front don't get background blur)
            float weight = 1.0;
            if (depthFalloff > 0.0)
            {
                float depthDiff = (sampleDepth - centerDepth) * depthFalloff;
                weight = saturate(1.0 - max(depthDiff, 0.0));
            }

            colorSum += sampleColor * weight;
            weightSum += weight;
        }

        // Sample in negative direction
        sampleUV = uv - step * t;
        if (sampleUV.x >= 0.0 && sampleUV.x <= 1.0 && sampleUV.y >= 0.0 && sampleUV.y <= 1.0)
        {
            vec4 sampleColor = texture2D(s_color, sampleUV);
            float sampleDepth = texture2D(s_depth, sampleUV).r;

            float weight = 1.0;
            if (depthFalloff > 0.0)
            {
                float depthDiff = (sampleDepth - centerDepth) * depthFalloff;
                weight = saturate(1.0 - max(depthDiff, 0.0));
            }

            colorSum += sampleColor * weight;
            weightSum += weight;
        }
    }

    // Normalize
    vec4 result = colorSum / weightSum;

    gl_FragColor = result;
}
