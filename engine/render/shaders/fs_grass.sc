$input v_texcoord0, v_color, v_worldPos, v_fade

#include <bgfx_shader.sh>

SAMPLER2D(s_grassTexture, 0);
SAMPLER2D(s_noiseTexture, 1);

uniform vec4 u_grassParams; // x=blade_width, y=blade_height, z=alpha_cutoff, w=fade_start

void main()
{
    // Sample grass texture
    vec4 texColor = texture2D(s_grassTexture, v_texcoord0);

    // Alpha test
    float alpha = texColor.a * v_color.a * v_fade;
    if (alpha < u_grassParams.z) {
        discard;
    }

    // Final color with instance tint
    vec3 finalColor = texColor.rgb * v_color.rgb;

    // Simple ambient occlusion at base (darker near ground)
    float ao = mix(0.5, 1.0, v_texcoord0.y);
    finalColor *= ao;

    gl_FragColor = vec4(finalColor, alpha);
}
