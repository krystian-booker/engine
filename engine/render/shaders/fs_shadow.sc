#include <bgfx_shader.sh>

void main()
{
    // Depth-only pass — hardware depth buffer is written automatically.
    // Color output is required by bgfx but discarded (no color attachment).
    gl_FragColor = vec4_splat(0.0);
}
