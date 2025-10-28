// HLSL vertex shader (compiled to SPIR-V via DXC)
[[vk::binding(0, 0)]]
cbuffer ViewProjection : register(b0)
{
    float4x4 view;
    float4x4 projection;
};

struct PushConstants
{
    float4x4 model;
};

[[vk::push_constant]]
PushConstants pc;

struct VSIn {
    float3 inPosition : POSITION;
    float3 inNormal   : NORMAL;
    float4 inTangent  : TANGENT;      // xyz = tangent, w = handedness
    float2 inTexCoord : TEXCOORD0;
};

struct VSOut {
    float4 pos          : SV_Position;
    [[vk::location(0)]] float3 fragNormal   : TEXCOORD0;
    [[vk::location(1)]] float3 fragTangent  : TEXCOORD1;
    [[vk::location(2)]] float3 fragBitangent: TEXCOORD2;
    [[vk::location(3)]] float2 fragTexCoord : TEXCOORD3;
    [[vk::location(4)]] float3 fragWorldPos : TEXCOORD4;
};

VSOut main(VSIn i)
{
    VSOut o;
    float4 worldPos = mul(pc.model, float4(i.inPosition, 1.0));
    o.pos = mul(projection, mul(view, worldPos));
    o.fragWorldPos = worldPos.xyz;

    // Transform TBN to world space
    // Normal matrix approximation (works for uniform scaling and rotation)
    float3x3 normalMatrix = (float3x3)pc.model;
    o.fragNormal = normalize(mul(normalMatrix, i.inNormal));
    o.fragTangent = normalize(mul(normalMatrix, i.inTangent.xyz));

    // Compute bitangent using handedness (stored in tangent.w)
    o.fragBitangent = cross(o.fragNormal, o.fragTangent) * i.inTangent.w;

    o.fragTexCoord = i.inTexCoord;
    return o;
}
