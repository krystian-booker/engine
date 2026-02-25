$input a_position, a_normal
$output v_worldPos, v_normal

#include <bgfx_shader.sh>

void main()
{
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
    v_worldPos = worldPos.xyz;
    
    vec3 normal = mul(u_model[0], vec4(a_normal, 0.0)).xyz;
    v_normal = normalize(normal);

    vec4 viewPos = mul(u_view, worldPos);
    gl_Position = mul(u_proj, viewPos);
}
