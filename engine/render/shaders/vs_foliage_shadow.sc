$input a_position, a_texcoord0, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0

#include <bgfx_shader.sh>

void main()
{
    // Build instance transform matrix from row vectors
    mat4 instanceTransform = mtxFromRows(i_data0, i_data1, i_data2, i_data3);

    // Transform to world space
    vec4 worldPos = mul(instanceTransform, vec4(a_position, 1.0));

    // Transform to clip space using light's view-projection
    gl_Position = mul(u_viewProj, worldPos);

    // Pass through UV for alpha testing
    v_texcoord0 = a_texcoord0;
}
