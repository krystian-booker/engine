// HLSL fragment shader for skybox rendering

[[vk::binding(1, 0)]]
TextureCube environmentMap : register(t0);

[[vk::binding(1, 0)]]
SamplerState envSampler : register(s0);

struct PSIn {
    [[vk::location(0)]] float3 worldPos : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target
{
    float3 envColor = environmentMap.Sample(envSampler, i.worldPos).rgb;

    // Optional: Apply tone mapping for HDR
    // envColor = envColor / (envColor + float3(1.0, 1.0, 1.0));

    return float4(envColor, 1.0);
}
