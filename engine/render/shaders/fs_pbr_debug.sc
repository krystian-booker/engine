// =============================================================================
// PBR Analytical Lighting Debug Shader
// =============================================================================
// Cook-Torrance microfacet BRDF with per-term isolation for validation.
//
// Debug modes (cycles every 4 seconds via u_time.x):
//   0 - Full PBR (Diffuse + Specular)
//   1 - Fresnel Only (F)
//   2 - NDF Only (D)
//   3 - Geometry Only (G)
//   4 - Specular Only (D*G*F / denominator)
//   5 - Diffuse Only (Lambertian)
//
// ---- VALIDATION HARD RULES ----
// Energy Conservation: If a sphere looks brighter than the light intensity
//   (1.0), check the kD calculation. kD = (1 - F) * (1 - metalness).
// Metallic Coloration: For Metallic=1.0, specular reflection is tinted by
//   Albedo color. For Metallic=0.0, specular reflection is pure white (F0=0.04).
// Horizon Handling: NdotL and NdotV are clamped to 0.0 to prevent light
//   leaking from behind the object.
// =============================================================================

$input v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_color0, v_clipPos

#include <bgfx_shader.sh>
#include "common/uniforms.sh"
#include "common/light_data.sh"

// Redefine constants (common/pbr.sh uses these but we inline the math for clarity)
#define PI 3.14159265359
#define INV_PI 0.31830988618

void main()
{
    vec3 worldPos = v_worldPos.xyz;
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_cameraPos.xyz - worldPos);

    // Material parameters from engine uniform system
    vec3 albedo    = u_albedoColor.xyz;
    float metallic  = u_pbrParams.x;
    float roughness = max(u_pbrParams.y, 0.04); // Prevent singularity at roughness=0

    // --- Unpack directional light via getLight() ---
    Light mainLight = getLight(0);
    vec3 L          = normalize(-mainLight.direction);
    vec3 radiance   = mainLight.color * mainLight.intensity;

    // Half vector
    vec3 H = normalize(V + L);

    // Clamped dot products — horizon handling prevents light leaking
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    // ---- F0: base reflectance at normal incidence ----
    // Dielectrics: 0.04 (4% reflectance). Metals: use albedo as F0.
    vec3 F0 = mix(vec3_splat(0.04), albedo, metallic);

    // ---- D: Trowbridge-Reitz GGX Normal Distribution Function ----
    float a      = roughness * roughness;
    float a2     = a * a;
    float dTerm  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    float D      = a2 / max(PI * dTerm * dTerm, 1e-4);

    // ---- G: Smith's method with Schlick-GGX approximation ----
    float r      = roughness + 1.0;
    float k      = (r * r) / 8.0;
    float G_V    = NdotV / max(NdotV * (1.0 - k) + k, 1e-4);
    float G_L    = NdotL / max(NdotL * (1.0 - k) + k, 1e-4);
    float G      = G_V * G_L;

    // ---- Fresnel: Rim visualization (N.V) vs BRDF Fresnel (V.H) ----
    // For validation mode 1, we show standard grazing-angle Fresnel (rim).
    // For the BRDF math, we use VdotH.
    float rim          = pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0);
    vec3 F_rim         = F0 + (vec3_splat(1.0) - F0) * rim;
    
    // BRDF Fresnel using V.H
    vec3 F = F0 + (vec3_splat(1.0) - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);

    // ---- Specular: Cook-Torrance BRDF ----
    // Added protection against small NdotV/NdotL to prevent analytical black holes
    float specDenom = 4.0 * max(NdotV, 1e-4) * max(NdotL, 1e-4);
    vec3 specular   = (D * G * F) / specDenom;

    // ---- Diffuse: Lambertian with energy conservation ----
    // kS is the specular ratio (Fresnel)
    vec3 kS = F;
    // kD is the diffuse ratio
    vec3 kD = (vec3_splat(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = kD * albedo * INV_PI;

    // ---- Compose full PBR lighting ----
    vec3 Lo      = (diffuse + specular) * radiance * NdotL;
    vec3 ambient = vec3_splat(0.03) * albedo;

    // ---- Debug mode selection ----
    // We use u_pbrParams.w (set from m_debug_mode in C++) as the debug selection
    int debugMode = int(u_pbrParams.w);

    vec3 color = vec3_splat(0.0);

    if (debugMode == 0)
    {
        color = ambient + Lo;
    }
    else if (debugMode == 1)
    {
        // Mode 1: Fresnel Only — must be F(N,V) to show Rim for validation
        color = F_rim;
    }
    else if (debugMode == 2)
    {
        // Mode 2: NDF Only — Visual validation of hotspot blur
        color = vec3_splat(D * 0.1) * NdotL;
    }
    else if (debugMode == 3)
    {
        color = vec3_splat(G);
    }
    else if (debugMode == 4)
    {
        // Mode 4: Specular Only — D*G*F with light and N.L
        color = specular * radiance * NdotL;
    }
    else if (debugMode == 5)
    {
        // Mode 5: Diffuse Only — must be Lambertian * kD * radiance * NdotL
        color = diffuse * radiance * NdotL;
    }
    else if (debugMode == 6)
    {
        // TEST A: Are Normals working? (Should be a rainbow)
        color = normalize(v_normal) * 0.5 + 0.5;
    }
    else if (debugMode == 7)
    {
        // TEST B: Is the Camera Pos reaching the shader?
        color = fract(u_cameraPos.xyz);
    }
    else if (debugMode == 8)
    {
        // TEST C: Is WorldPos working?
        color = v_worldPos.xyz * 0.1;
    }
    else
    {
        color = vec3_splat(0.5);
    }
    
    // Output final color (alpha=1 for opaque objects)
    gl_FragColor = vec4(color, 1.0);
}
