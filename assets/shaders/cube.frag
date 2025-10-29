// HLSL fragment shader with PBR Cook-Torrance BRDF (compiled to SPIR-V via DXC)

// Material SSBO structure (must match GPUMaterial in C++)
struct GPUMaterial
{
    uint albedoIndex;
    uint normalIndex;
    uint metalRoughIndex;
    uint aoIndex;

    uint emissiveIndex;
    uint flags;
    uint padding1;
    uint padding2;

    float4 albedoTint;
    float4 emissiveFactor;

    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float aoStrength;
};

// Set 1, Binding 0: Material SSBO (persistent)
[[vk::binding(0, 1)]]
StructuredBuffer<GPUMaterial> materials : register(t0);

// Set 1, Binding 1: Bindless texture array (persistent)
[[vk::binding(1, 1)]]
Texture2D textures[] : register(t1);

[[vk::binding(1, 1)]]
SamplerState samplers[] : register(s1);

struct PSIn {
    [[vk::location(0)]] float3 fragNormal   : TEXCOORD0;
    [[vk::location(1)]] float3 fragTangent  : TEXCOORD1;
    [[vk::location(2)]] float3 fragBitangent: TEXCOORD2;
    [[vk::location(3)]] float2 fragTexCoord : TEXCOORD3;
    [[vk::location(4)]] float3 fragWorldPos : TEXCOORD4;
    [[vk::location(5)]] nointerpolation uint materialIndex : TEXCOORD5;
};

// Constants
static const float PI = 3.14159265359;

// ============================================================================
// PBR Helper Functions
// ============================================================================

// GGX/Trowbridge-Reitz normal distribution function
float DistributionGGX(float3 N, float3 H, float roughness)
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

// Schlick-GGX geometry function
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0001);
}

// Smith's method for geometry obstruction
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Unpack normal from normal map
float3 UnpackNormal(float3 normalMap, float3 N, float3 T, float3 B, float normalScale)
{
    // Normal map is in [0,1], convert to [-1,1]
    float3 tangentNormal = normalMap * 2.0 - 1.0;
    tangentNormal.xy *= normalScale;

    // Transform from tangent space to world space
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(tangentNormal, TBN));
}

// ============================================================================
// Main Fragment Shader
// ============================================================================

float4 main(PSIn i) : SV_Target
{
    // Fetch material data
    GPUMaterial mat = materials[i.materialIndex];

    // Sample textures using bindless array
    float4 albedo = textures[NonUniformResourceIndex(mat.albedoIndex)].Sample(
        samplers[NonUniformResourceIndex(mat.albedoIndex)], i.fragTexCoord);
    albedo *= mat.albedoTint;

    float3 normalMap = textures[NonUniformResourceIndex(mat.normalIndex)].Sample(
        samplers[NonUniformResourceIndex(mat.normalIndex)], i.fragTexCoord).rgb;

    float4 metalRough = textures[NonUniformResourceIndex(mat.metalRoughIndex)].Sample(
        samplers[NonUniformResourceIndex(mat.metalRoughIndex)], i.fragTexCoord);
    float roughness = metalRough.r * mat.roughnessFactor;
    float metallic = metalRough.g * mat.metallicFactor;

    float ao = textures[NonUniformResourceIndex(mat.aoIndex)].Sample(
        samplers[NonUniformResourceIndex(mat.aoIndex)], i.fragTexCoord).r;
    ao = lerp(1.0, ao, mat.aoStrength);

    float3 emissive = textures[NonUniformResourceIndex(mat.emissiveIndex)].Sample(
        samplers[NonUniformResourceIndex(mat.emissiveIndex)], i.fragTexCoord).rgb;
    emissive *= mat.emissiveFactor.rgb * mat.emissiveFactor.w;

    // Reconstruct TBN and apply normal mapping
    float3 N = normalize(i.fragNormal);
    float3 T = normalize(i.fragTangent);
    float3 B = normalize(i.fragBitangent);
    N = UnpackNormal(normalMap, N, T, B, mat.normalScale);

    // View direction
    float3 V = normalize(float3(0.0, 5.0, 10.0) - i.fragWorldPos); // Camera position hardcoded for now

    // Calculate reflectance at normal incidence
    // For dielectrics F0 = 0.04, for metals use albedo color
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo.rgb, metallic);

    // ========================================================================
    // Lighting calculation
    // ========================================================================

    // Directional light (sun)
    float3 L = normalize(float3(1.0, 1.0, 1.0));
    float3 H = normalize(V + L);
    float3 radiance = float3(1.0, 1.0, 1.0) * 5.0; // Light color and intensity

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    float3 specular = numerator / max(denominator, 0.0001);

    // Energy conservation: diffuse component
    float3 kS = F; // Specular reflection coefficient
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallic; // Metals have no diffuse component

    float NdotL = max(dot(N, L), 0.0);

    // Lambertian diffuse
    float3 diffuse = kD * albedo.rgb / PI;

    // Final outgoing radiance
    float3 Lo = (diffuse + specular) * radiance * NdotL;

    // Ambient term (very simple ambient)
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo.rgb * ao;

    // Add emissive
    float3 color = ambient + Lo + emissive;

    // HDR tonemapping (simple Reinhard)
    color = color / (color + float3(1.0, 1.0, 1.0));

    // Gamma correction
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    return float4(color, albedo.a);
}
