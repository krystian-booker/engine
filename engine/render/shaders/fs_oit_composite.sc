$input v_texcoord0

#include <bgfx_shader.sh>

// OIT Composite Fragment Shader
// Composites the OIT result over the opaque scene

SAMPLER2D(s_accum, 0);    // Accumulation buffer
SAMPLER2D(s_reveal, 1);   // Revealage buffer
SAMPLER2D(s_opaque, 2);   // Opaque scene

void main()
{
    vec2 uv = v_texcoord0;

    // Sample buffers
    vec4 accum = texture2D(s_accum, uv);
    float reveal = texture2D(s_reveal, uv).r;
    vec3 opaqueColor = texture2D(s_opaque, uv).rgb;

    // Avoid division by zero
    float epsilon = 0.0001;

    // Calculate average color from accumulation
    // accum.rgb = sum of (color * alpha * weight)
    // accum.a = sum of (alpha * weight)
    vec3 transparentColor = accum.rgb / max(accum.a, epsilon);

    // Calculate coverage from revealage
    // reveal = product of (1 - alpha) = how much of the opaque is visible
    // coverage = 1 - reveal = how much transparent covers
    float coverage = 1.0 - reveal;

    // Blend transparent over opaque
    // If coverage is low, show more opaque
    // If coverage is high, show more transparent
    vec3 finalColor = mix(opaqueColor, transparentColor, coverage);

    // Handle edge case: if no transparent was rendered (accum.a near 0)
    // just show the opaque color
    if (accum.a < epsilon)
    {
        finalColor = opaqueColor;
    }

    gl_FragColor = vec4(finalColor, 1.0);
}
