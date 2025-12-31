$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_source, 0);      // Lower mip (smaller)
SAMPLER2D(s_highRes, 1);     // Current mip (higher res)

uniform vec4 u_bloomParams;  // x=scatter, y=intensity, z=unused, w=unused
uniform vec4 u_texelSize;    // xy=source texel size, zw=unused

// 9-tap tent filter for high-quality upsample
vec3 upsample9Tap(vec2 uv, vec2 texelSize)
{
    vec3 A = texture2D(s_source, uv + texelSize * vec2(-1.0, -1.0)).rgb;
    vec3 B = texture2D(s_source, uv + texelSize * vec2( 0.0, -1.0)).rgb;
    vec3 C = texture2D(s_source, uv + texelSize * vec2( 1.0, -1.0)).rgb;

    vec3 D = texture2D(s_source, uv + texelSize * vec2(-1.0, 0.0)).rgb;
    vec3 E = texture2D(s_source, uv).rgb;
    vec3 F = texture2D(s_source, uv + texelSize * vec2( 1.0, 0.0)).rgb;

    vec3 G = texture2D(s_source, uv + texelSize * vec2(-1.0, 1.0)).rgb;
    vec3 H = texture2D(s_source, uv + texelSize * vec2( 0.0, 1.0)).rgb;
    vec3 I = texture2D(s_source, uv + texelSize * vec2( 1.0, 1.0)).rgb;

    // Tent filter weights
    vec3 result = E * 4.0;
    result += (B + D + F + H) * 2.0;
    result += (A + C + G + I);
    result *= (1.0 / 16.0);

    return result;
}

void main()
{
    float scatter = u_bloomParams.x;
    vec2 texelSize = u_texelSize.xy;

    // Upsample the lower-resolution bloom
    vec3 lowRes = upsample9Tap(v_texcoord0, texelSize);

    // Get the high-res version from previous pass
    vec3 highRes = texture2D(s_highRes, v_texcoord0).rgb;

    // Combine with scatter factor
    vec3 color = mix(highRes, lowRes, scatter);

    gl_FragColor = vec4(color, 1.0);
}
