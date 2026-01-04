$input v_texcoord0

#include <bgfx_shader.sh>

// Screen-Space Reflections Temporal Resolve Fragment Shader
// Blends current frame SSR with history to reduce noise

SAMPLER2D(s_reflection, 0);  // Current frame reflection
SAMPLER2D(s_history, 1);     // Previous frame reflection
SAMPLER2D(s_velocity, 2);    // Motion vectors
SAMPLER2D(s_hit, 3);         // Hit UV + confidence from trace pass

uniform mat4 u_prevViewProj;
uniform vec4 u_ssrParams;    // x=temporal_weight
uniform vec4 u_texelSize;    // xy=texel size, zw=resolution

// Catmull-Rom bicubic filter for history sampling
vec4 textureBicubic(sampler2D tex, vec2 uv, vec2 texelSize)
{
    vec2 position = uv / texelSize - 0.5;
    vec2 f = fract(position);
    vec2 pos = (floor(position) + 0.5) * texelSize;

    // Catmull-Rom weights
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);

    vec2 w12 = w1 + w2;
    vec2 offset12 = w2 / w12;

    vec2 pos0 = pos - texelSize;
    vec2 pos3 = pos + texelSize * 2.0;
    vec2 pos12 = pos + texelSize * offset12;

    vec4 result = vec4(0.0, 0.0, 0.0, 0.0);
    result += texture2DLod(tex, vec2(pos0.x, pos0.y), 0.0) * w0.x * w0.y;
    result += texture2DLod(tex, vec2(pos12.x, pos0.y), 0.0) * w12.x * w0.y;
    result += texture2DLod(tex, vec2(pos3.x, pos0.y), 0.0) * w3.x * w0.y;
    result += texture2DLod(tex, vec2(pos0.x, pos12.y), 0.0) * w0.x * w12.y;
    result += texture2DLod(tex, vec2(pos12.x, pos12.y), 0.0) * w12.x * w12.y;
    result += texture2DLod(tex, vec2(pos3.x, pos12.y), 0.0) * w3.x * w12.y;
    result += texture2DLod(tex, vec2(pos0.x, pos3.y), 0.0) * w0.x * w3.y;
    result += texture2DLod(tex, vec2(pos12.x, pos3.y), 0.0) * w12.x * w3.y;
    result += texture2DLod(tex, vec2(pos3.x, pos3.y), 0.0) * w3.x * w3.y;

    return result;
}

// Clamp color to neighborhood to prevent ghosting
vec4 clipToAABB(vec4 color, vec4 minColor, vec4 maxColor)
{
    vec4 center = (minColor + maxColor) * 0.5;
    vec4 halfExtent = (maxColor - minColor) * 0.5;

    vec4 offset = color - center;
    vec4 normalized = offset / max(halfExtent, vec4(0.0001, 0.0001, 0.0001, 0.0001));
    float maxComponent = max(max(abs(normalized.x), abs(normalized.y)),
                             max(abs(normalized.z), abs(normalized.w)));

    if (maxComponent > 1.0)
    {
        return center + offset / maxComponent;
    }
    return color;
}

void main()
{
    vec2 uv = v_texcoord0;
    vec2 texelSize = u_texelSize.xy;

    // Sample current frame reflection
    vec4 currentReflection = texture2D(s_reflection, uv);
    vec4 hitInfo = texture2D(s_hit, uv);

    // Get motion vector for reprojection
    vec2 velocity = texture2D(s_velocity, uv).xy;

    // Reproject to previous frame position
    vec2 historyUV = uv - velocity;

    // Check if reprojected position is valid
    if (historyUV.x < 0.0 || historyUV.x > 1.0 ||
        historyUV.y < 0.0 || historyUV.y > 1.0)
    {
        gl_FragColor = currentReflection;
        return;
    }

    // Sample history with bicubic filter for quality
    vec4 historyReflection = textureBicubic(s_history, historyUV, texelSize);

    // Compute neighborhood bounds for anti-ghosting
    vec4 minColor = currentReflection;
    vec4 maxColor = currentReflection;

    // Sample 3x3 neighborhood
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec4 neighbor = texture2DLod(s_reflection, uv + offset, 0.0);
            minColor = min(minColor, neighbor);
            maxColor = max(maxColor, neighbor);
        }
    }

    // Clip history to neighborhood to prevent ghosting
    vec4 clippedHistory = clipToAABB(historyReflection, minColor, maxColor);

    // Temporal blend
    float temporalWeight = u_ssrParams.x;

    // Reduce temporal weight when hit confidence is low
    float hitConfidence = hitInfo.z;
    temporalWeight = mix(0.5, temporalWeight, hitConfidence);

    // Blend current and history
    vec4 result = mix(currentReflection, clippedHistory, temporalWeight);

    gl_FragColor = result;
}
