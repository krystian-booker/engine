// HLSL fragment shader with PBR Cook-Torrance BRDF (compiled to SPIR-V via DXC)

// View/Projection matrices (Set 0, Binding 0)
[[vk::binding(0, 0)]]
cbuffer ViewProjection : register(b0)
{
    float4x4 view;
    float4x4 projection;
};

// GPU Light structure (must match GPULight in C++)
// Type encoding: 0=Directional, 1=Point, 2=Spot, 3=Area, 4=Tube, 5=Hemisphere
struct GPULight
{
    float4 positionAndType;    // xyz = position/direction, w = type
    float4 colorAndIntensity;  // rgb = color, w = intensity
    float4 directionAndRange;  // xyz = direction, w = range
    float4 spotAngles;         // x = inner cone cos, y = outer cone cos, z = castsShadows, w = shadowMapIndex

    // Extended parameters for area/tube/hemisphere lights
    float4 areaParams;         // x = width, y = height, z = twoSided (0/1), w = unused
    float4 tubeParams;         // x = length, y = radius, z/w = unused
    float4 hemisphereParams;   // xyz = skyColor or right vector (area), w = unused
    float4 hemisphereParams2;  // xyz = groundColor or up vector (area), w = unused
};

// Lighting uniform buffer (Set 0, Binding 1)
[[vk::binding(1, 0)]]
cbuffer LightingData : register(b1)
{
    float4 cameraPosition;     // xyz = camera position
    uint numLights;
    uint padding1;
    uint padding2;
    uint padding3;
    GPULight lights[16];       // Max 16 lights
};

// Shadow uniform buffer (Set 0, Binding 2)
[[vk::binding(2, 0)]]
cbuffer ShadowData : register(b2)
{
    float4x4 cascadeViewProj[4];  // View-projection matrix for each cascade
    float4 cascadeSplits;          // xyz = cascade split distances, w = numCascades
    float4 shadowParams;           // x = shadow bias, y = PCF radius, z/w = padding
};

// Shadow map array (Set 0, Binding 3)
[[vk::binding(3, 0)]]
Texture2DArray shadowMap : register(t2);

[[vk::binding(3, 0)]]
SamplerComparisonState shadowSampler : register(s2);

// IBL textures (Set 0, Bindings 4-6)
[[vk::binding(4, 0)]]
TextureCube irradianceMap : register(t3);

[[vk::binding(4, 0)]]
SamplerState irradianceSampler : register(s3);

[[vk::binding(5, 0)]]
TextureCube prefilteredMap : register(t4);

[[vk::binding(5, 0)]]
SamplerState prefilteredSampler : register(s4);

[[vk::binding(6, 0)]]
Texture2D brdfLUT : register(t5);

[[vk::binding(6, 0)]]
SamplerState brdfSampler : register(s5);

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
// Shadow Mapping Functions
// ============================================================================

// Calculate shadow with PCF (Percentage Closer Filtering)
float CalculateShadowPCF(float3 worldPos, float3 normal, float3 lightDir, uint cascadeIndex)
{
    // Transform position to light clip space
    float4 lightSpacePos = mul(cascadeViewProj[cascadeIndex], float4(worldPos, 1.0));

    // Perspective divide
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // Convert to texture coordinates [0,1]
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.y = 1.0 - projCoords.y;  // Flip Y for Vulkan

    // Check if outside shadow map bounds
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 1.0;  // No shadow
    }

    // Normal-based bias to reduce shadow acne
    float bias = max(shadowParams.x * (1.0 - dot(normal, lightDir)), shadowParams.x * 0.1);
    float currentDepth = projCoords.z - bias;

    // PCF with 3x3 kernel
    float shadow = 0.0;
    float2 texelSize = 1.0 / float2(2048.0, 2048.0);  // Shadow map resolution
    float pcfRadius = shadowParams.y;

    for (float x = -pcfRadius; x <= pcfRadius; x += 1.0) {
        for (float y = -pcfRadius; y <= pcfRadius; y += 1.0) {
            float2 offset = float2(x, y) * texelSize;
            float3 sampleCoords = float3(projCoords.xy + offset, float(cascadeIndex));

            // Hardware PCF comparison
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, sampleCoords, currentDepth);
        }
    }

    float kernelSize = (pcfRadius * 2.0 + 1.0);
    shadow /= (kernelSize * kernelSize);

    return shadow;
}

// Select cascade based on view space depth
uint SelectCascade(float viewDepth)
{
    uint numCascades = (uint)cascadeSplits.w;

    for (uint i = 0; i < numCascades - 1; ++i) {
        if (viewDepth < cascadeSplits[i]) {
            return i;
        }
    }

    return numCascades - 1;
}

// Calculate shadow factor for directional light with CSM
float CalculateDirectionalLightShadow(float3 worldPos, float3 normal, float3 lightDir, float viewDepth)
{
    uint numCascades = (uint)cascadeSplits.w;

    if (numCascades == 0) {
        return 1.0;  // No shadows
    }

    // Select appropriate cascade
    uint cascadeIndex = SelectCascade(viewDepth);

    return CalculateShadowPCF(worldPos, normal, lightDir, cascadeIndex);
}

// ============================================================================
// IBL (Image-Based Lighting) Functions
// ============================================================================

// Fresnel-Schlick with roughness for IBL
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Calculate IBL contribution (diffuse + specular)
float3 CalculateIBL(float3 N, float3 V, float3 F0, float3 albedo, float roughness, float metallic, float ao)
{
    // Reflection vector
    float3 R = reflect(-V, N);

    // Diffuse IBL (irradiance)
    float3 kS = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;  // Metallic surfaces have no diffuse

    float3 irradiance = irradianceMap.Sample(irradianceSampler, N).rgb;
    float3 diffuse = irradiance * albedo;

    // Specular IBL (prefiltered environment map + BRDF LUT)
    const float MAX_REFLECTION_LOD = 4.0;  // Max mip level of prefiltered map
    float3 prefilteredColor = prefilteredMap.SampleLevel(prefilteredSampler, R, roughness * MAX_REFLECTION_LOD).rgb;

    float2 envBRDF = brdfLUT.Sample(brdfSampler, float2(max(dot(N, V), 0.0), roughness)).rg;
    float3 specular = prefilteredColor * (kS * envBRDF.x + envBRDF.y);

    // Combine diffuse and specular with AO
    float3 ambient = (kD * diffuse + specular) * ao;

    return ambient;
}

// ============================================================================
// Area Light Functions (Simplified LTC)
// ============================================================================

// Integrate edge contribution for area light polygon
float IntegrateEdge(float3 v1, float3 v2)
{
    float cosTheta = dot(v1, v2);
    cosTheta = clamp(cosTheta, -0.9999, 0.9999);

    float theta = acos(cosTheta);
    float res = cross(v1, v2).z * theta / sin(theta);

    return res;
}

// Evaluate area light contribution (simplified rectangular light)
float3 EvaluateAreaLight(float3 N, float3 V, float3 P,
                         float3 lightPos, float3 lightDir, float3 lightRight, float3 lightUp,
                         float width, float height, float3 lightColor, float intensity,
                         float3 F0, float roughness, float3 albedo, float metallic)
{
    // Transform fragment position to light space
    float3 toLight = lightPos - P;
    float3 L = normalize(toLight);

    // Check if facing the light
    float NdotL = dot(N, L);
    if (NdotL <= 0.0)
        return float3(0.0, 0.0, 0.0);

    // Compute four corners of rectangular area light in world space
    float halfWidth = width * 0.5;
    float halfHeight = height * 0.5;

    float3 corner1 = lightPos + lightRight * halfWidth + lightUp * halfHeight;
    float3 corner2 = lightPos - lightRight * halfWidth + lightUp * halfHeight;
    float3 corner3 = lightPos - lightRight * halfWidth - lightUp * halfHeight;
    float3 corner4 = lightPos + lightRight * halfWidth - lightUp * halfHeight;

    // Vectors from fragment to corners
    float3 L1 = normalize(corner1 - P);
    float3 L2 = normalize(corner2 - P);
    float3 L3 = normalize(corner3 - P);
    float3 L4 = normalize(corner4 - P);

    // Integrate edges (simplified polygon integration)
    float sum = 0.0;
    sum += IntegrateEdge(L1, L2);
    sum += IntegrateEdge(L2, L3);
    sum += IntegrateEdge(L3, L4);
    sum += IntegrateEdge(L4, L1);
    sum = max(0.0, sum);

    // Approximate specular (simplified, not full LTC)
    float3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(NdotL, 0.0);
    float3 specular = numerator / max(denominator, 0.0001);

    // Diffuse
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallic;
    float3 diffuse = kD * albedo / PI;

    // Combine with area light irradiance
    float3 radiance = lightColor * intensity * sum;

    return (diffuse + specular) * radiance * NdotL;
}

// ============================================================================
// Tube Light Functions
// ============================================================================

// Evaluate tube/line light contribution
float3 EvaluateTubeLight(float3 N, float3 V, float3 P,
                         float3 lightPos, float3 lightDir, float tubeLength, float tubeRadius,
                         float3 lightColor, float intensity,
                         float3 F0, float roughness, float3 albedo, float metallic)
{
    // Compute line segment endpoints
    float3 L0 = lightPos - lightDir * (tubeLength * 0.5);
    float3 L1 = lightPos + lightDir * (tubeLength * 0.5);

    // Find closest point on line segment
    float3 L0_to_P = P - L0;
    float3 L0_to_L1 = L1 - L0;
    float t = dot(L0_to_P, L0_to_L1) / dot(L0_to_L1, L0_to_L1);
    t = saturate(t);

    float3 closestPoint = L0 + L0_to_L1 * t;

    // Light direction and distance
    float3 L = closestPoint - P;
    float distance = length(L);
    L /= distance;

    // Check if facing the light
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0)
        return float3(0.0, 0.0, 0.0);

    // Distance attenuation with tube radius
    float attenuation = 1.0 / (distance * distance + 1.0);
    attenuation *= saturate(tubeRadius / distance);  // Tube radius influence

    // PBR evaluation
    float3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(NdotL, 0.0);
    float3 specular = numerator / max(denominator, 0.0001);

    // Diffuse
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallic;
    float3 diffuse = kD * albedo / PI;

    // Combine
    float3 radiance = lightColor * intensity * attenuation;

    return (diffuse + specular) * radiance * NdotL;
}

// ============================================================================
// Hemisphere Light Functions
// ============================================================================

// Evaluate hemisphere/ambient light contribution
float3 EvaluateHemisphereLight(float3 N, float3 skyColor, float3 groundColor, float intensity, float3 albedo)
{
    // Blend between sky and ground based on normal direction
    float hemisphereBlend = dot(N, float3(0, 1, 0)) * 0.5 + 0.5;
    float3 hemisphereColor = lerp(groundColor, skyColor, hemisphereBlend);

    // Apply to diffuse albedo
    return hemisphereColor * albedo * intensity;
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
    float3 V = normalize(cameraPosition.xyz - i.fragWorldPos);

    // Calculate reflectance at normal incidence
    // For dielectrics F0 = 0.04, for metals use albedo color
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo.rgb, metallic);

    // ========================================================================
    // Lighting calculation - loop through all active lights
    // ========================================================================

    float3 Lo = float3(0.0, 0.0, 0.0);  // Total outgoing radiance

    for (uint lightIdx = 0; lightIdx < numLights; ++lightIdx)
    {
        GPULight light = lights[lightIdx];

        uint lightType = (uint)light.positionAndType.w;
        float3 lightColor = light.colorAndIntensity.rgb;
        float lightIntensity = light.colorAndIntensity.w;

        float3 L;           // Light direction
        float attenuation;  // Distance attenuation

        // Calculate light direction and attenuation based on light type
        if (lightType == 0) // Directional light
        {
            L = normalize(-light.directionAndRange.xyz);
            attenuation = 1.0;

            // Apply shadows if enabled
            bool castsShadows = light.spotAngles.z > 0.5;
            if (castsShadows) {
                // Calculate view space depth for cascade selection
                float4 viewPos = mul(view, float4(i.fragWorldPos, 1.0));
                float viewDepth = abs(viewPos.z);

                float shadowFactor = CalculateDirectionalLightShadow(i.fragWorldPos, N, L, viewDepth);
                attenuation *= shadowFactor;
            }
        }
        else if (lightType == 1) // Point light
        {
            float3 lightPos = light.positionAndType.xyz;
            float3 lightVec = lightPos - i.fragWorldPos;
            float distance = length(lightVec);
            L = lightVec / distance;  // Normalize

            // Inverse square falloff with range cutoff
            float range = light.directionAndRange.w;
            float distanceRatio = saturate(1.0 - pow(distance / range, 4.0));
            attenuation = distanceRatio * distanceRatio / (distance * distance + 1.0);
        }
        else if (lightType == 2) // Spot light
        {
            float3 lightPos = light.positionAndType.xyz;
            float3 lightVec = lightPos - i.fragWorldPos;
            float distance = length(lightVec);
            L = lightVec / distance;

            // Distance attenuation
            float range = light.directionAndRange.w;
            float distanceRatio = saturate(1.0 - pow(distance / range, 4.0));
            float distAttenuation = distanceRatio * distanceRatio / (distance * distance + 1.0);

            // Cone attenuation
            float3 spotDir = normalize(light.directionAndRange.xyz);
            float cosTheta = dot(-L, spotDir);
            float innerCone = light.spotAngles.x;
            float outerCone = light.spotAngles.y;
            float epsilon = innerCone - outerCone;
            float coneAttenuation = saturate((cosTheta - outerCone) / epsilon);

            attenuation = distAttenuation * coneAttenuation;
        }
        else if (lightType == 3) // Area light
        {
            float3 lightPos = light.positionAndType.xyz;
            float3 lightDir = normalize(light.directionAndRange.xyz);
            float3 lightRight = normalize(light.hemisphereParams.xyz);
            float3 lightUp = normalize(light.hemisphereParams2.xyz);
            float width = light.areaParams.x;
            float height = light.areaParams.y;

            float3 areaContrib = EvaluateAreaLight(N, V, i.fragWorldPos,
                                                   lightPos, lightDir, lightRight, lightUp,
                                                   width, height, lightColor, lightIntensity,
                                                   F0, roughness, albedo.rgb, metallic);
            Lo += areaContrib;
            continue;  // Area light handled separately
        }
        else if (lightType == 4) // Tube light
        {
            float3 lightPos = light.positionAndType.xyz;
            float3 lightDir = normalize(light.directionAndRange.xyz);
            float tubeLength = light.tubeParams.x;
            float tubeRadius = light.tubeParams.y;

            float3 tubeContrib = EvaluateTubeLight(N, V, i.fragWorldPos,
                                                   lightPos, lightDir, tubeLength, tubeRadius,
                                                   lightColor, lightIntensity,
                                                   F0, roughness, albedo.rgb, metallic);
            Lo += tubeContrib;
            continue;  // Tube light handled separately
        }
        else if (lightType == 5) // Hemisphere light
        {
            float3 skyColor = light.hemisphereParams.xyz;
            float3 groundColor = light.hemisphereParams2.xyz;

            float3 hemisphereContrib = EvaluateHemisphereLight(N, skyColor, groundColor,
                                                               lightIntensity, albedo.rgb);
            Lo += hemisphereContrib;
            continue;  // Hemisphere light handled separately
        }
        else
        {
            continue;  // Unknown light type
        }

        // Skip if light contribution is negligible
        if (attenuation < 0.001)
            continue;

        float3 H = normalize(V + L);
        float3 radiance = lightColor * lightIntensity * attenuation;

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

        // Add to total radiance
        Lo += (diffuse + specular) * radiance * NdotL;
    }

    // IBL (Image-Based Lighting) contribution
    float3 ambient = CalculateIBL(N, V, F0, albedo.rgb, roughness, metallic, ao);

    // Add emissive
    float3 color = ambient + Lo + emissive;

    // HDR tonemapping (simple Reinhard)
    color = color / (color + float3(1.0, 1.0, 1.0));

    // Gamma correction
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    return float4(color, albedo.a);
}
