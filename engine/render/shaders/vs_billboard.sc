$input a_position, a_texcoord0
$output v_texcoord0, v_color

#include <bgfx_shader.sh>

uniform vec4 u_billboardColor;      // Billboard tint color
uniform vec4 u_billboardUV;         // xy=offset, zw=scale

void main()
{
    // Transform position (billboard orientation handled by transform matrix)
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));

    // Apply UV offset and scale for sprite sheets
    v_texcoord0 = a_texcoord0 * u_billboardUV.zw + u_billboardUV.xy;

    // Pass through color
    v_color = u_billboardColor;
}
