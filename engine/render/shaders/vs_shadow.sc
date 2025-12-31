$input a_position

#include <bgfx_shader.sh>

void main()
{
    // Simple depth-only vertex shader
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
