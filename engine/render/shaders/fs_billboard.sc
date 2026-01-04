$input v_texcoord0, v_color

#include <bgfx_shader.sh>

SAMPLER2D(s_billboard, 0);

uniform vec4 u_billboardParams;  // x=depth_fade_distance, yzw=unused

void main()
{
    // Sample texture
    vec4 texColor = texture2D(s_billboard, v_texcoord0);

    // Apply vertex color (tint)
    vec4 finalColor = texColor * v_color;

    // Alpha test - discard fully transparent pixels
    if (finalColor.a < 0.01)
    {
        discard;
    }

    gl_FragColor = finalColor;
}
