struct PSIn {
    [[vk::location(0)]] float3 fragNormal   : TEXCOORD0;
    [[vk::location(1)]] float3 fragTangent  : TEXCOORD1;
    [[vk::location(2)]] float3 fragBitangent: TEXCOORD2;
    [[vk::location(3)]] float2 fragTexCoord : TEXCOORD3;
    [[vk::location(4)]] float3 fragWorldPos : TEXCOORD4;
};

float4 main(PSIn i) : SV_Target
{
    // Base color (white, will be replaced by textures later)
    float3 baseColor = float3(0.8, 0.8, 0.8);

    // Simple directional lighting
    float3 lightDir = normalize(float3(1.0, 1.0, 1.0));
    float3 normal = normalize(i.fragNormal);
    float diff = max(dot(normal, lightDir), 0.0);

    // Ambient + diffuse
    float3 ambient = 0.3 * baseColor;
    float3 diffuse = diff * baseColor;

    return float4(ambient + diffuse, 1.0);
}

