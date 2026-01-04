$input v_texcoord0

#include <bgfx_shader.sh>

// Hi-Z Mipmap Generation Fragment Shader
// Generates a hierarchical depth buffer by taking the maximum (farthest) depth
// at each 2x2 block from the previous mip level.

SAMPLER2D(s_depth, 0);

uniform vec4 u_texelSize;  // xy = texel size, zw = resolution
uniform vec4 u_hizLevel;   // x = current mip level

void main()
{
    vec2 uv = v_texcoord0;
    vec2 texelSize = u_texelSize.xy;

    // Sample 4 neighboring depth values from previous mip level
    // For level 0, we sample from the source depth buffer
    // For other levels, we sample from the previous hi-z mip

    float d0 = texture2D(s_depth, uv + vec2(-0.5, -0.5) * texelSize).r;
    float d1 = texture2D(s_depth, uv + vec2( 0.5, -0.5) * texelSize).r;
    float d2 = texture2D(s_depth, uv + vec2(-0.5,  0.5) * texelSize).r;
    float d3 = texture2D(s_depth, uv + vec2( 0.5,  0.5) * texelSize).r;

    // Take the maximum depth (farthest from camera in reverse-Z)
    // For reverse-Z depth buffer, closer = larger values, farther = smaller values
    // We want the conservative maximum, so we take the min for reverse-Z
    // However, if using standard depth (0=near, 1=far), take max
    // This shader assumes standard depth buffer convention
    float maxDepth = max(max(d0, d1), max(d2, d3));

    gl_FragColor = vec4(maxDepth, 0.0, 0.0, 1.0);
}
