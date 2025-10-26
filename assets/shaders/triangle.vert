// HLSL vertex shader (compiled for Vulkan SPIR-V via DXC)

struct VSInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float3 color    : COLOR0;
    [[vk::location(3)]] float2 texCoord : TEXCOORD0;
};

struct VSOutput {
    float4 position              : SV_Position;
    [[vk::location(0)]] float3 color : COLOR0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}
