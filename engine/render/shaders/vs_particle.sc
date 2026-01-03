$input a_position, a_texcoord0, a_color0, a_texcoord1
$output v_texcoord0, v_color0, v_life

#include <bgfx_shader.sh>

uniform vec4 u_cameraPos;

void main()
{
    // Extract particle parameters
    float rotation = a_texcoord1.x;
    float size = a_texcoord1.y;
    float life = a_texcoord1.z;

    // Calculate billboard offset from UV (centered at particle position)
    vec2 offset = (a_texcoord0 - vec2(0.5, 0.5)) * 2.0 * size;

    // Apply rotation
    float cosR = cos(rotation);
    float sinR = sin(rotation);
    vec2 rotatedOffset = vec2(
        offset.x * cosR - offset.y * sinR,
        offset.x * sinR + offset.y * cosR
    );

    // Get camera right and up vectors from view matrix
    vec3 cameraRight = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 cameraUp = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);

    // Billboard the particle
    vec3 worldPos = a_position + cameraRight * rotatedOffset.x + cameraUp * rotatedOffset.y;

    gl_Position = mul(u_viewProj, vec4(worldPos, 1.0));

    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
    v_life = life;
}
