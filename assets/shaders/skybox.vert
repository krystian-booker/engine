// HLSL vertex shader for skybox rendering
[[vk::binding(0, 0)]]
cbuffer ViewProjection : register(b0)
{
    float4x4 view;
    float4x4 projection;
};

struct VSIn {
    float3 inPosition : POSITION;
};

struct VSOut {
    float4 pos          : SV_Position;
    [[vk::location(0)]] float3 worldPos : TEXCOORD0;
};

VSOut main(VSIn i)
{
    VSOut o;

    // Remove translation from view matrix
    float4x4 viewNoTranslation = view;
    viewNoTranslation[3][0] = 0.0;
    viewNoTranslation[3][1] = 0.0;
    viewNoTranslation[3][2] = 0.0;

    float4 clipPos = mul(projection, mul(viewNoTranslation, float4(i.inPosition, 1.0)));

    // Set depth to max (w) for skybox to always be behind everything
    o.pos = clipPos.xyww;
    o.worldPos = i.inPosition;

    return o;
}
