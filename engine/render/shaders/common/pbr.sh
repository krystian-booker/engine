// PBR BRDF functions for physically-based rendering
#ifndef PBR_SH
#define PBR_SH

#define PI 3.14159265359
#define INV_PI 0.31830988618

// Fresnel-Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness for IBL
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3_splat(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// GGX/Trowbridge-Reitz Normal Distribution Function
float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0001);
}

// Smith's Schlick-GGX Geometry Function
float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0001);
}

// Smith Geometry Function (combines shadowing and masking)
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);

    return ggx1 * ggx2;
}

// Cook-Torrance BRDF
vec3 cookTorranceBRDF(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness)
{
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    // Fresnel reflectance at normal incidence
    // For dielectrics use 0.04, for metals use albedo color
    vec3 F0 = vec3_splat(0.04);
    F0 = mix(F0, albedo, metallic);

    // Calculate BRDF terms
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(HdotV, F0);

    // Specular term
    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = numerator / denominator;

    // Diffuse term (energy conservation)
    vec3 kS = F;
    vec3 kD = vec3_splat(1.0) - kS;
    kD = kD * (1.0 - metallic); // Metals have no diffuse

    // Lambertian diffuse
    vec3 diffuse = kD * albedo * INV_PI;

    return (diffuse + specular) * NdotL;
}

// Get normal from normal map
// Uses explicit vector math instead of mat3 * vec3 to avoid
// GLSL vs HLSL mat3 constructor column/row order differences.
vec3 getNormalFromMap(vec3 normalMapSample, vec3 worldNormal, vec3 worldTangent, vec3 worldBitangent)
{
    vec3 tangentNormal = normalMapSample * 2.0 - 1.0;

    vec3 T = normalize(worldTangent);
    vec3 B = normalize(worldBitangent);
    vec3 N = normalize(worldNormal);

    return normalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z);
}

// Perceptual roughness to linear roughness
float perceptualRoughnessToRoughness(float perceptualRoughness)
{
    return perceptualRoughness * perceptualRoughness;
}

// Calculate reflectance at normal incidence for dielectrics
float iorToF0(float ior)
{
    float f = (ior - 1.0) / (ior + 1.0);
    return f * f;
}

#endif // PBR_SH
