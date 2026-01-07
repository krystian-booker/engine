$input v_texcoord0

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_color, 0);       // Full-resolution color
SAMPLER2D(s_coc, 1);         // Circle of Confusion texture

// Parameters
uniform vec4 u_texelSize;    // xy=1/size, zw=size

void main()
{
    vec2 uv = v_texcoord0;
    vec2 texel = u_texelSize.xy;

    // 4-tap bilinear downsample (box filter)
    vec2 offsets[4];
    offsets[0] = vec2(-0.5, -0.5) * texel;
    offsets[1] = vec2( 0.5, -0.5) * texel;
    offsets[2] = vec2(-0.5,  0.5) * texel;
    offsets[3] = vec2( 0.5,  0.5) * texel;

    vec4 colorSum = vec4_splat(0.0);
    float cocSum = 0.0;
    float nearSum = 0.0;
    float farSum = 0.0;

    for (int i = 0; i < 4; i++)
    {
        vec2 sampleUV = uv + offsets[i];
        vec4 color = texture2D(s_color, sampleUV);
        float coc = texture2D(s_coc, sampleUV).r * 2.0 - 1.0;  // Decode from [0,1] to [-1,1]

        colorSum += color;
        cocSum += abs(coc);

        // Separate near and far contributions
        if (coc < 0.0)
        {
            nearSum += -coc;
        }
        else
        {
            farSum += coc;
        }
    }

    colorSum /= 4.0;
    cocSum /= 4.0;
    nearSum /= 4.0;
    farSum /= 4.0;

    // Output to two render targets:
    // RT0 (near): Color pre-multiplied by near CoC
    // RT1 (far): Color pre-multiplied by far CoC

    // Near field output
    vec4 nearColor = vec4(colorSum.rgb * saturate(nearSum * 2.0), nearSum);

    // Far field output
    vec4 farColor = vec4(colorSum.rgb, farSum);

    // MRT output - near in RT0, far in RT1
    // bgfx uses gl_FragData for MRT
#if BGFX_SHADER_LANGUAGE_HLSL || BGFX_SHADER_LANGUAGE_METAL
    gl_FragData[0] = nearColor;
    gl_FragData[1] = farColor;
#else
    gl_FragData[0] = nearColor;
    gl_FragData[1] = farColor;
#endif
}
