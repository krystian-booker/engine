// HLSL vertex shader (compiled for Vulkan SPIR-V via DXC)
// Hardcoded triangle vertices
static const float2 positions[3] = {
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
};

static const float3 colors[3] = {
    float3(1.0, 0.0, 0.0),  // Red
    float3(0.0, 1.0, 0.0),  // Green
    float3(0.0, 0.0, 1.0)   // Blue
};

struct VSOut {
    float4 pos   : SV_Position;
    float3 color : COLOR0;
};

VSOut main(uint vid : SV_VertexID) {
    VSOut o;
    o.pos = float4(positions[vid], 0.0, 1.0);
    o.color = colors[vid];
    return o;
}
