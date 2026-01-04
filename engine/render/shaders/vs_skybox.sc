$input a_position
$output v_worldDir

#include <bgfx_shader.sh>



void main()
{
    // Use fullscreen triangle positions directly
    // a_position is in NDC space (-1 to 1)
    gl_Position = vec4(a_position.xy, 1.0, 1.0);

    // Reconstruct world direction from NDC position
    // Use far plane (z=1) for the ray direction
    vec4 worldPos = mul(u_invViewProj, vec4(a_position.xy, 1.0, 1.0));
    v_worldDir = worldPos.xyz / worldPos.w;
}
