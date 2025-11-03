// HLSL vertex shader for shadow pass (compiled to SPIR-V via DXC)

struct PushConstants
{
    float4x4 lightViewProj;  // Light's view-projection matrix
    float4x4 model;           // Model matrix
};

[[vk::push_constant]]
PushConstants pc;

struct VSIn {
    float3 inPosition : POSITION;
};

struct VSOut {
    float4 pos : SV_Position;
};

VSOut main(VSIn i)
{
    VSOut o;

    // Transform vertex to light clip space
    float4 worldPos = mul(pc.model, float4(i.inPosition, 1.0));
    o.pos = mul(pc.lightViewProj, worldPos);

    return o;
}
