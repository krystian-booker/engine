$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_albedo, 0);

uniform vec4 u_foliageParams; // x=unused, y=unused, z=alpha_cutoff, w=unused

void main()
{
    // Sample albedo for alpha
    float alpha = texture2D(s_albedo, v_texcoord0).a;

    // Alpha test for leaves
    float alphaCutoff = u_foliageParams.z;
    if (alphaCutoff <= 0.0)
    {
        alphaCutoff = 0.5;
    }
    if (alpha < alphaCutoff)
    {
        discard;
    }

    // Depth-only output
    gl_FragColor = vec4_splat(0.0);
}
