// ============================================================================
// Color Pipeline Debug Fragment Shader
// ============================================================================
//
// Paired with vs_pbr.sc (shares varying_pbr.def.sc for texture coordinate access).
// Visualizes individual texture channels for pipeline validation.
//
// Debug mode is read from u_pbrParams.z (the 'ao' field in MaterialData).
//   0.0 = Raw Albedo (sRGB passthrough)
//   1.0 = Linear Albedo (pow(texel, 2.2))
//   2.0 = Roughness grayscale (metallic_roughness.g)
//   3.0 = Metalness grayscale (metallic_roughness.b)
//
// IMPORTANT: The post-process pipeline should be set to gamma=1.0 and
// tonemap=none when using this shader, so debug values pass through to
// the display unchanged.
// ============================================================================

$input v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_color0, v_clipPos

#include <bgfx_shader.sh>
#include "common/uniforms.sh"

// PBR texture samplers — same slots as fs_pbr.sc
SAMPLER2D(s_albedo,            0);  // Base color (RGB) + opacity (A)
SAMPLER2D(s_normal,            1);  // Normal map (unused here, kept for slot consistency)
SAMPLER2D(s_metallicRoughness, 2);  // Green = roughness, Blue = metallic (glTF convention)
SAMPLER2D(s_ao,                3);  // Ambient occlusion (unused here)
SAMPLER2D(s_emissive,          4);  // Emissive (unused here)

void main()
{
    // Read debug mode from u_pbrParams.z (MaterialData.ao field)
    float mode = u_pbrParams.z;

    vec4 albedoSample = texture2D(s_albedo, v_texcoord0);
    vec4 mrSample = texture2D(s_metallicRoughness, v_texcoord0);

    vec3 outColor;

    if (mode < 0.5)
    {
        // ── Mode 0: Raw Albedo (sRGB) ──
        // Output the raw texture data with no conversion. In sRGB-encoded textures,
        // a 50% grey pixel has value 128/255 ≈ 0.502. When the post-process pipeline
        // uses gamma=1.0, this value reaches the framebuffer as-is. On an sRGB display,
        // 0.5 should appear as perceptual "middle grey" — the intended appearance.
        //
        // If this looks "too bright" (closer to 73% grey), the texture is being
        // linearized somewhere before this shader sees it (e.g., BGFX_TEXTURE_SRGB flag).
        outColor = albedoSample.rgb;
    }
    else if (mode < 1.5)
    {
        // ── Mode 1: Linear Albedo ──
        // Apply sRGB-to-linear conversion: pow(texel, 2.2).
        // For a 50% grey sRGB pixel: 0.5^2.2 ≈ 0.218 — should look significantly darker
        // than Mode 0 on the display.
        //
        // VALIDATION: A color picker should read approximately 0.21 for the grey patch.
        // If it reads 0.5 (same as Mode 0), the conversion is not happening.
        // If it reads ~0.05 (0.218^2.2), there's a DOUBLE gamma application.
        //
        // abs() prevents HLSL pow() error with potentially negative values.
        outColor = pow(abs(albedoSample.rgb), vec3_splat(2.2));
    }
    else if (mode < 2.5)
    {
        // ── Mode 2: Roughness Map ──
        // Display the roughness value (green channel of MR texture) as grayscale.
        // This channel MUST be sampled in linear space — no gamma correction.
        //
        // VALIDATION: The test MR texture has green=128/255 ≈ 0.502.
        // This should display as mid-grey (value ≈ 0.5 in the framebuffer).
        // If it appears brighter (~0.73), the MR texture is incorrectly sampled as sRGB:
        //   0.5 sRGB → 0.5^(1/2.2) ≈ 0.73 (WRONG — roughness is distorted)
        //
        // Raw texture value, NOT multiplied by u_pbrParams.y (material roughness).
        float roughness = mrSample.g;
        outColor = vec3_splat(roughness);
    }
    else
    {
        // ── Mode 3: Metalness Map ──
        // Display the metalness value (blue channel of MR texture) as grayscale.
        // Must also be sampled in linear space.
        //
        // The test MR texture has:
        //   Top half:    blue=0   → should display as black (dielectric)
        //   Bottom half: blue=255 → should display as white (metal)
        //
        // Raw texture value, NOT multiplied by u_pbrParams.x (material metallic).
        float metallic = mrSample.b;
        outColor = vec3_splat(metallic);
    }

    gl_FragColor = vec4(outColor, 1.0);
}
