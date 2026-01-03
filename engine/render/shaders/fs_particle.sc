$input v_texcoord0, v_color0, v_life

#include <bgfx_shader.sh>

SAMPLER2D(s_texture, 0);
SAMPLER2D(s_depth, 1);

uniform vec4 u_particleParams[4];
// u_particleParams[0].x = soft particles enabled (0 or 1)
// u_particleParams[0].y = soft particle distance

void main()
{
    // Sample particle texture
    vec4 texColor = texture2D(s_texture, v_texcoord0);

    // Combine with vertex color
    vec4 color = texColor * v_color0;

    // Soft particle fade (if enabled)
    float softParticles = u_particleParams[0].x;
    float softDistance = u_particleParams[0].y;

    // Note: Soft particle depth comparison would require screen-space depth
    // For now, just use the basic color output
    // Full implementation would sample depth buffer and fade based on distance

    // Discard fully transparent pixels
    if (color.a < 0.01)
    {
        discard;
    }

    gl_FragColor = color;
}
