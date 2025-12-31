$input a_position, a_texcoord0
$output v_texcoord0

#include <bgfx_shader.sh>

void main()
{
    // Fullscreen triangle/quad vertex shader
    // Input: normalized device coordinates (-1 to 1)
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
    v_texcoord0 = a_texcoord0;
}
