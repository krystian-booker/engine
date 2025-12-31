$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_scene, 0);       // HDR scene
SAMPLER2D(s_bloom, 1);       // Bloom texture

uniform vec4 u_tonemapParams;    // x=exposure, y=gamma, z=white_point, w=tonemap_op
uniform vec4 u_bloomParams;      // x=bloom_intensity, yzw=unused
uniform vec4 u_vignetteParams;   // x=enabled, y=intensity, z=smoothness, w=unused

// Tone mapping operators
#define TONEMAP_NONE            0.0
#define TONEMAP_REINHARD        1.0
#define TONEMAP_REINHARD_EXT    2.0
#define TONEMAP_ACES            3.0
#define TONEMAP_UNCHARTED2      4.0
#define TONEMAP_AGX             5.0

// Simple Reinhard
vec3 tonemapReinhard(vec3 x)
{
    return x / (vec3_splat(1.0) + x);
}

// Extended Reinhard with white point
vec3 tonemapReinhardExtended(vec3 x, float white)
{
    vec3 numerator = x * (vec3_splat(1.0) + x / (white * white));
    return numerator / (vec3_splat(1.0) + x);
}

// ACES Filmic Tone Mapping
// From: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 tonemapACES(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Uncharted 2 Filmic
vec3 uncharted2Tonemap(vec3 x)
{
    float A = 0.15;  // Shoulder strength
    float B = 0.50;  // Linear strength
    float C = 0.10;  // Linear angle
    float D = 0.20;  // Toe strength
    float E = 0.02;  // Toe numerator
    float F = 0.30;  // Toe denominator

    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemapUncharted2(vec3 x)
{
    float W = 11.2;  // Linear white point
    float exposureBias = 2.0;

    vec3 curr = uncharted2Tonemap(x * exposureBias);
    vec3 whiteScale = vec3_splat(1.0) / uncharted2Tonemap(vec3_splat(W));

    return curr * whiteScale;
}

// AgX Tone Mapping (modern replacement for ACES)
vec3 agxDefaultContrastApprox(vec3 x)
{
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;

    return + 15.5     * x4 * x2
           - 40.14    * x4 * x
           + 31.96    * x4
           - 6.868    * x2 * x
           + 0.4298   * x2
           + 0.1191   * x
           - 0.00232;
}

vec3 tonemapAgX(vec3 x)
{
    // AgX Log2 encoding
    float minEv = -12.47393;
    float maxEv = 4.026069;

    x = max(x, vec3_splat(0.000001));
    x = log2(x);
    x = (x - minEv) / (maxEv - minEv);
    x = clamp(x, 0.0, 1.0);

    // Apply AgX contrast curve
    x = agxDefaultContrastApprox(x);

    return x;
}

// Apply selected tone mapping
vec3 applyTonemap(vec3 color, float op, float whitePoint)
{
    if (op == TONEMAP_NONE)
        return clamp(color, 0.0, 1.0);
    else if (op == TONEMAP_REINHARD)
        return tonemapReinhard(color);
    else if (op == TONEMAP_REINHARD_EXT)
        return tonemapReinhardExtended(color, whitePoint);
    else if (op == TONEMAP_ACES)
        return tonemapACES(color);
    else if (op == TONEMAP_UNCHARTED2)
        return tonemapUncharted2(color);
    else if (op == TONEMAP_AGX)
        return tonemapAgX(color);

    return tonemapACES(color);  // Default
}

// Vignette effect
float calculateVignette(vec2 uv, float intensity, float smoothness)
{
    vec2 centered = uv * 2.0 - 1.0;
    float dist = length(centered);
    return smoothstep(1.0, 1.0 - smoothness, dist * intensity);
}

void main()
{
    vec2 uv = v_texcoord0;

    // Sample HDR scene
    vec3 hdrColor = texture2D(s_scene, uv).rgb;

    // Add bloom
    float bloomIntensity = u_bloomParams.x;
    if (bloomIntensity > 0.0)
    {
        vec3 bloom = texture2D(s_bloom, uv).rgb;
        hdrColor += bloom * bloomIntensity;
    }

    // Apply exposure
    float exposure = u_tonemapParams.x;
    hdrColor *= exposure;

    // Tone mapping
    float tonemapOp = u_tonemapParams.w;
    float whitePoint = u_tonemapParams.z;
    vec3 ldrColor = applyTonemap(hdrColor, tonemapOp, whitePoint);

    // Gamma correction
    float gamma = u_tonemapParams.y;
    ldrColor = pow(ldrColor, vec3_splat(1.0 / gamma));

    // Vignette
    if (u_vignetteParams.x > 0.5)
    {
        float vignette = calculateVignette(uv, u_vignetteParams.y, u_vignetteParams.z);
        ldrColor *= vignette;
    }

    gl_FragColor = vec4(ldrColor, 1.0);
}
