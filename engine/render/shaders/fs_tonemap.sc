// TODO (Optimization): Currently using dynamic branching (if/else) for tone mapping selection.
// Once the pipeline is stable, convert these into BGFX shader macros (e.g., #ifdef TONEMAP_AGX) 
// defined in varying.def.sc or the build script. This will generate zero-cost shader variants 
// and eliminate runtime branching overhead on the GPU.
$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_scene, 0);       // HDR scene
SAMPLER2D(s_bloom, 1);       // Bloom texture
SAMPLER2D(s_volumetric, 2);  // volumetrix sampler

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

// High-precision Luminance-based Reinhard Extended
vec3 tonemapReinhardExtended(vec3 color, float whitePoint)
{
    // Calculate luminance using Rec.709 luma coefficients
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    
    // Reinhard extended formula on luminance
    float numerator = luminance * (1.0 + luminance / (whitePoint * whitePoint));
    float newLuminance = numerator / (1.0 + luminance);
    
    // Apply new luminance back to color
    return color * (newLuminance / max(luminance, 0.0001));
}

// Simple Reinhard on luminance
vec3 tonemapReinhard(vec3 color)
{
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float newLuminance = luminance / (1.0 + luminance);
    return color * (newLuminance / max(luminance, 0.0001));
}

// High-precision ACES Filmic Tone Mapping Curve (Stephen Hill Fit)
vec3 tonemapACES(vec3 color)
{
    // 1. The Input Transform: sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
    // Using dot products guarantees cross-platform consistency in BGFX.
    vec3 v = vec3(
        dot(vec3(0.59719, 0.35458, 0.04823), color),
        dot(vec3(0.07600, 0.90834, 0.01566), color),
        dot(vec3(0.02840, 0.13383, 0.83777), color)
    );
    
    
    // 2. Apply the RRT + ODT fit
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    vec3 fit = a / b;
    
    // 3. The Output Transform: ODT_SAT => XYZ => D60_2_D65 => sRGB
    vec3 finalColor = vec3(
        dot(vec3( 1.60475, -0.53108, -0.07367), fit),
        dot(vec3(-0.10208,  1.10813, -0.00605), fit),
        dot(vec3(-0.00327, -0.07276,  1.07602), fit)
    );
    
    // 4. Strip the baked-in sRGB curve to output Linear
    return pow(saturate(finalColor), vec3_splat(2.2));
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
    vec3 curr = uncharted2Tonemap(x);
    vec3 whiteScale = vec3_splat(1.0) / uncharted2Tonemap(vec3_splat(W));

    return curr * whiteScale;
}

// DEFAULT tonemapping for engine
// High-precision AgX Tone Mapping Curve
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

vec3 tonemapAgX(vec3 color)
{
    // 1. Input Transform (sRGB to AgX Inset Working Space)
    vec3 v = vec3(
        dot(vec3(0.842479062253094, 0.0784335999999992, 0.0792237451477643), color),
        dot(vec3(0.0423282422610123, 0.878468636469772,  0.0791661274605434), color),
        dot(vec3(0.0423756549057051, 0.0784336,          0.879142973793104), color)
    );

    // 2. AgX Log2 encoding
    const float minEv = -12.47393;
    const float maxEv = 4.026069;

    v = max(v, vec3_splat(0.000001)); // Prevent -INF in log2
    v = log2(v);
    v = (v - minEv) / (maxEv - minEv);
    v = saturate(v);

    // 3. Apply AgX contrast curve
    v = agxDefaultContrastApprox(v);

    // 4. Output Transform (AgX Outset Working Space to sRGB)
    vec3 finalColor = vec3(
        dot(vec3( 1.19687900512017,  -0.0980208811401368, -0.0990297440797205), v),
        dot(vec3(-0.0528968517574562, 1.15190312990417,   -0.0989611768448433), v),
        dot(vec3(-0.0529716355144438, -0.0980434501171241,  1.15107367264116), v)
    );

    // 5. AgX "Punchy" look — boost saturation slightly for richer colors
    vec3 agxLuma = vec3_splat(dot(finalColor, vec3(0.2126, 0.7152, 0.0722)));
    finalColor = mix(agxLuma, finalColor, 1.0);
    finalColor = saturate(finalColor);

    // 6. Strip the baked-in sRGB curve to output Linear
    return pow(finalColor, vec3_splat(2.2));
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

    // Add volumetric fog compositing
    vec4 vol = texture2D(s_volumetric, uv);
    hdrColor = hdrColor * vol.a + vol.rgb;

    // Apply exposure
    float exposure = u_tonemapParams.x;
    hdrColor *= exposure;

    // Tone mapping
    float tonemapOp = u_tonemapParams.w;
    float whitePoint = u_tonemapParams.z;
    vec3 ldrColor = applyTonemap(hdrColor, tonemapOp, whitePoint);

    // Vignette
    float vignette = calculateVignette(uv, u_vignetteParams.y, u_vignetteParams.z);
    ldrColor = mix(ldrColor, ldrColor * vignette, u_vignetteParams.x);

    // Gamma correction
    float gamma = u_tonemapParams.y;
    float invGamma = 1.0 / max(gamma, 0.0001);
    ldrColor = pow(max(ldrColor, vec3_splat(0.0)), vec3_splat(invGamma));

    gl_FragColor = vec4(ldrColor, 1.0);
}
