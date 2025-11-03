// HLSL vertex shader for depth prepass (compiled to SPIR-V via DXC)

struct UniformBufferObject {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferObject> ubo;

struct VSIn {
    float3 inPosition : POSITION;
};

struct VSOut {
    float4 pos : SV_Position;
};

VSOut main(VSIn i)
{
    VSOut o;
    o.pos = mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(i.inPosition, 1.0))));
    return o;
}
