$input a_position
$output v_texcoord0

#include <bgfx_shader.sh>

void main()
{
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
    
    // Calculate UVs from clip-space position [-1, 1] to [0, 1]
    // Respect bgfx's screen origin (bottom-left vs top-left)
    vec2 uv = a_position.xy * 0.5 + 0.5;
    
#if BGFX_SHADER_LANGUAGE_HLSL || BGFX_SHADER_LANGUAGE_PSSL || BGFX_SHADER_LANGUAGE_METAL
    uv.y = 1.0 - uv.y;
#endif

    v_texcoord0 = uv;
}
