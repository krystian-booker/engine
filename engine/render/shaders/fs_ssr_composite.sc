$input v_texcoord0

#include <bgfx_shader.sh>

// Screen-Space Reflections Composite Fragment Shader
// Blends SSR reflections with the scene based on roughness and Fresnel

SAMPLER2D(s_color, 0);       // Scene color
SAMPLER2D(s_reflection, 1);  // SSR reflection
SAMPLER2D(s_roughness, 2);   // Roughness buffer
SAMPLER2D(s_hit, 3);         // Hit UV + confidence from trace pass

uniform vec4 u_ssrParams;    // x=intensity, y=edge_fade_start, z=edge_fade_end, w=fresnel_bias
uniform vec4 u_ssrParams2;   // x=debug_mode, y=roughness_threshold

// Fresnel-Schlick approximation
float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

void main()
{
    vec2 uv = v_texcoord0;

    // Sample scene color
    vec4 sceneColor = texture2D(s_color, uv);

    // Sample SSR reflection
    vec4 reflectionColor = texture2D(s_reflection, uv);

    // Sample hit info for confidence
    vec4 hitInfo = texture2D(s_hit, uv);
    float hitConfidence = hitInfo.z;

    // Sample roughness
    float roughness = texture2D(s_roughness, uv).r;

    // Parameters
    float intensity = u_ssrParams.x;
    float edgeFadeStart = u_ssrParams.y;
    float edgeFadeEnd = u_ssrParams.z;
    float fresnelBias = u_ssrParams.w;
    float debugMode = u_ssrParams2.x;
    float roughnessThreshold = u_ssrParams2.y;

    // Edge fade - reduce reflections near screen edges
    vec2 edgeUV = abs(uv - 0.5) * 2.0;
    float edgeFade = 1.0 - smoothstep(edgeFadeStart, edgeFadeEnd, max(edgeUV.x, edgeUV.y));

    // Roughness fade - reduce reflections on rough surfaces
    float roughnessFade = 1.0 - smoothstep(0.0, roughnessThreshold, roughness);

    // Simple Fresnel approximation (assuming view is roughly from camera)
    // In a more complete implementation, you'd pass the view direction and normal
    float fresnel = fresnelSchlick(1.0 - fresnelBias, 0.04);

    // Combine all fade factors with hit confidence
    float reflectionStrength = hitConfidence * edgeFade * roughnessFade * fresnel * intensity;

    // Blend reflection with scene
    vec3 finalColor = mix(sceneColor.rgb, reflectionColor.rgb, reflectionStrength);

    // Debug mode - show SSR only
    if (debugMode > 0.5)
    {
        finalColor = reflectionColor.rgb * reflectionStrength;
    }

    gl_FragColor = vec4(finalColor, sceneColor.a);
}
