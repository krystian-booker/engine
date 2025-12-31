#include <bgfx_shader.sh>

void main()
{
    // Depth-only fragment shader
    // The depth buffer is written automatically
    // We don't need to output anything for opaque shadow casters
    gl_FragColor = vec4_splat(0.0);
}
