$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_source, 0);

uniform vec4 u_bloomParams;  // x=threshold, y=soft_threshold, z=unused, w=unused
uniform vec4 u_texelSize;    // xy=source texel size, zw=unused

// 13-tap tent filter for high-quality downsample
// Reduces aliasing compared to simple bilinear
vec3 downsample13Tap(vec2 uv, vec2 texelSize)
{
    vec3 A = texture2D(s_source, uv + texelSize * vec2(-1.0, -1.0)).rgb;
    vec3 B = texture2D(s_source, uv + texelSize * vec2( 0.0, -1.0)).rgb;
    vec3 C = texture2D(s_source, uv + texelSize * vec2( 1.0, -1.0)).rgb;
    vec3 D = texture2D(s_source, uv + texelSize * vec2(-0.5, -0.5)).rgb;
    vec3 E = texture2D(s_source, uv + texelSize * vec2( 0.5, -0.5)).rgb;
    vec3 F = texture2D(s_source, uv + texelSize * vec2(-1.0,  0.0)).rgb;
    vec3 G = texture2D(s_source, uv).rgb;
    vec3 H = texture2D(s_source, uv + texelSize * vec2( 1.0,  0.0)).rgb;
    vec3 I = texture2D(s_source, uv + texelSize * vec2(-0.5,  0.5)).rgb;
    vec3 J = texture2D(s_source, uv + texelSize * vec2( 0.5,  0.5)).rgb;
    vec3 K = texture2D(s_source, uv + texelSize * vec2(-1.0,  1.0)).rgb;
    vec3 L = texture2D(s_source, uv + texelSize * vec2( 0.0,  1.0)).rgb;
    vec3 M = texture2D(s_source, uv + texelSize * vec2( 1.0,  1.0)).rgb;

    // Weighted combination (tent filter)
    vec3 result = vec3_splat(0.0);
    result += (D + E + I + J) * 0.125;
    result += (A + B + F + G) * 0.03125;
    result += (B + C + G + H) * 0.03125;
    result += (F + G + K + L) * 0.03125;
    result += (G + H + L + M) * 0.03125;

    return result;
}

// Soft threshold for bloom (avoids hard cutoff)
vec3 prefilter(vec3 color)
{
    float threshold = u_bloomParams.x;
    float softThreshold = u_bloomParams.y;

    float brightness = max(max(color.r, color.g), color.b);
    float soft = brightness - threshold + softThreshold;
    soft = clamp(soft, 0.0, 2.0 * softThreshold);
    soft = soft * soft / (4.0 * softThreshold + 0.00001);

    float contribution = max(soft, brightness - threshold);
    contribution /= max(brightness, 0.00001);

    return color * contribution;
}

void main()
{
    vec2 texelSize = u_texelSize.xy;
    vec3 color = downsample13Tap(v_texcoord0, texelSize);

    // Apply threshold (prefilter is identity when threshold=0)
    color = prefilter(color);

    gl_FragColor = vec4(color, 1.0);
}
