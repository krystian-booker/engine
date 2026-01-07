$input v_texcoord0

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_color, 0);       // Original full-resolution color
SAMPLER2D(s_coc, 1);         // Circle of Confusion
SAMPLER2D(s_nearBlur, 2);    // Blurred near field (half resolution)
SAMPLER2D(s_farBlur, 3);     // Blurred far field (half resolution)

// Parameters
uniform vec4 u_dofParams;    // x=high_quality_near, y=debug_coc, z=debug_focus, w=unused
uniform vec4 u_texelSize;    // xy=1/size, zw=size

void main()
{
    vec2 uv = v_texcoord0;

    // Sample all inputs
    vec4 originalColor = texture2D(s_color, uv);
    float coc = texture2D(s_coc, uv).r * 2.0 - 1.0;  // Decode [-1, 1]
    vec4 nearBlur = texture2D(s_nearBlur, uv);
    vec4 farBlur = texture2D(s_farBlur, uv);

    // Debug modes
    if (u_dofParams.y > 0.5)
    {
        // Debug CoC visualization
        // Red = near blur, Blue = far blur, Green = in focus
        if (coc < -0.01)
        {
            // Near (red)
            gl_FragColor = vec4(-coc, 0.0, 0.0, 1.0);
        }
        else if (coc > 0.01)
        {
            // Far (blue)
            gl_FragColor = vec4(0.0, 0.0, coc, 1.0);
        }
        else
        {
            // In focus (green)
            gl_FragColor = vec4(0.0, 0.5, 0.0, 1.0);
        }
        return;
    }

    if (u_dofParams.z > 0.5)
    {
        // Debug focus plane - show sharp region
        float sharpness = 1.0 - saturate(abs(coc) * 5.0);
        gl_FragColor = vec4(originalColor.rgb * sharpness + vec3_splat(0.2) * (1.0 - sharpness), 1.0);
        return;
    }

    // Calculate blend factors
    float nearCoc = max(-coc, 0.0);  // Near blur amount (0-1)
    float farCoc = max(coc, 0.0);    // Far blur amount (0-1)

    // Start with original color
    vec3 result = originalColor.rgb;

    // Blend in far field blur
    // Far field blends based on CoC magnitude
    float farBlend = saturate(farCoc * 2.0);
    result = mix(result, farBlur.rgb, farBlend);

    // Blend in near field blur
    // Near field uses premultiplied alpha for proper foreground bleeding
    float nearAlpha = nearBlur.a;
    float nearBlend = saturate(nearCoc * 2.0 + nearAlpha);

    if (u_dofParams.x > 0.5)
    {
        // High quality near field: use alpha for smooth edges
        result = mix(result, nearBlur.rgb / max(nearAlpha, 0.001), nearBlend * nearAlpha);
    }
    else
    {
        // Standard near field blending
        result = mix(result, nearBlur.rgb, nearBlend);
    }

    // Smooth transition near focus plane
    // Avoid harsh boundaries between blurred and sharp regions
    float focusTransition = saturate(1.0 - abs(coc) * 3.0);
    result = mix(result, originalColor.rgb, focusTransition * 0.3);

    gl_FragColor = vec4(result, 1.0);
}
