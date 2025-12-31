$input v_texcoord0

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_ssao, 0);        // SSAO texture to blur
SAMPLER2D(s_depth, 1);       // Scene depth for edge-aware blur

// Blur parameters
uniform vec4 u_blurParams;   // xy=texel size, z=blur radius, w=depth threshold

void main()
{
    vec2 uv = v_texcoord0;
    vec2 texelSize = u_blurParams.xy;

    // Get center pixel values
    float centerAO = texture2D(s_ssao, uv).r;
    float centerDepth = texture2D(s_depth, uv).r;

    // Bilateral blur (edge-preserving)
    float totalWeight = 1.0;
    float result = centerAO;

    // 4x4 bilateral blur
    for (int x = -2; x <= 2; x++)
    {
        for (int y = -2; y <= 2; y++)
        {
            if (x == 0 && y == 0) continue;

            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 sampleUV = uv + offset;

            float sampleAO = texture2D(s_ssao, sampleUV).r;
            float sampleDepth = texture2D(s_depth, sampleUV).r;

            // Spatial weight (gaussian-like)
            float dist = length(vec2(float(x), float(y)));
            float spatialWeight = exp(-dist * dist / 4.0);

            // Depth weight (edge-aware)
            float depthDiff = abs(centerDepth - sampleDepth);
            float depthWeight = exp(-depthDiff * depthDiff * u_blurParams.w);

            float weight = spatialWeight * depthWeight;
            result += sampleAO * weight;
            totalWeight += weight;
        }
    }

    result /= totalWeight;

    gl_FragColor = vec4(vec3_splat(result), 1.0);
}
