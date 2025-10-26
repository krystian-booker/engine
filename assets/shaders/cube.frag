struct PSIn {
    [[vk::location(0)]] float3 fragColor  : COLOR0;
    [[vk::location(1)]] float3 fragNormal : TEXCOORD1;
};

float4 main(PSIn i) : SV_Target
{
    // Simple directional lighting
    float3 lightDir = normalize(float3(1.0, 1.0, 1.0));
    float diff = max(dot(normalize(i.fragNormal), lightDir), 0.3); // 0.3 = ambient
    return float4(i.fragColor * diff, 1.0);
}

