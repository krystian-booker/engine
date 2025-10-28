// Common helpers for mipmap compute shaders (HLSL → SPIR-V)

static float3 SrgbToLinear(float3 srgb)
{
    float3 low = srgb / 12.92f;
    float3 high = pow((srgb + 0.055f) / 1.055f, 2.4f);
    float3 mask = step(float3(0.04045f, 0.04045f, 0.04045f), srgb);
    return lerp(low, high, mask);
}

static float3 LinearToSrgb(float3 linearColor)
{
    float3 low = linearColor * 12.92f;
    float3 high = 1.055f * pow(linearColor, 1.0f / 2.4f) - 0.055f;
    float3 mask = step(float3(0.0031308f, 0.0031308f, 0.0031308f), linearColor);
    return lerp(low, high, mask);
}

static float3 DecodeNormal(float3 encoded)
{
    return encoded * 2.0f - 1.0f;
}

static float3 EncodeNormal(float3 normal)
{
    return normal * 0.5f + 0.5f;
}

static float3 SafeNormalize(float3 v)
{
    float lenSq = dot(v, v);
    if (lenSq > 1e-6f)
    {
        return normalize(v);
    }
    return float3(0.0f, 0.0f, 1.0f);
}

static uint2 ClampCoord(uint2 coord, uint2 maxCoord)
{
    return min(coord, maxCoord);
}
