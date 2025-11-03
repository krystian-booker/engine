// HLSL fragment shader for shadow pass (depth-only, no color output)
// This shader is minimal since we only write to depth buffer

struct PSIn {
    float4 pos : SV_Position;
};

void main(PSIn i)
{
    // No color output - depth is written automatically
    // Could add alpha testing here if needed for alpha-masked materials
}
