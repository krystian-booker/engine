$input v_color0

#include <bgfx_shader.sh>

void main()
{
    // The "Pink Cube": explicit fallback for missing/failed materials
    gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
}
