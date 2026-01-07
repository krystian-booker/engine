$input a_position
$output v_screenPos, v_worldPos

#include <bgfx_shader.sh>

// Decal transform (model matrix for the decal box)
uniform mat4 u_decalTransform;

void main()
{
    // Transform vertex to world space
    vec4 worldPos = mul(u_decalTransform, vec4(a_position, 1.0));
    v_worldPos = worldPos.xyz;

    // Transform to clip space
    gl_Position = mul(u_viewProj, worldPos);

    // Pass screen position for depth reconstruction
    v_screenPos = gl_Position;
}
