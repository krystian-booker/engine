// HLSL vertex shader (compiled to SPIR-V via DXC)
[[vk::binding(0, 0)]]
cbuffer UniformBufferObject : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};

struct VSIn {
    float3 inPosition : POSITION;
    float3 inNormal   : NORMAL;
    float3 inColor    : COLOR0;
    float2 inTexCoord : TEXCOORD0;
};

struct VSOut {
    float4 pos        : SV_Position;
    [[vk::location(0)]] float3 fragColor  : COLOR0;
    [[vk::location(1)]] float3 fragNormal : TEXCOORD1;
};

VSOut main(VSIn i)
{
    VSOut o;
    float4 worldPos = mul(model, float4(i.inPosition, 1.0));
    o.pos = mul(projection, mul(view, worldPos));

    // Normal matrix approximation (no non-uniform scaling)
    float3x3 nmat = (float3x3)model;
    o.fragNormal = normalize(mul(nmat, i.inNormal));

    o.fragColor = i.inColor;
    return o;
}
