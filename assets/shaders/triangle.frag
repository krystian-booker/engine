// HLSL fragment (pixel) shader
float4 main([[vk::location(0)]] float3 color : COLOR0) : SV_Target {
    return float4(color, 1.0f);
}
