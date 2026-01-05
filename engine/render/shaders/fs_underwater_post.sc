$input v_texcoord0

#include <bgfx_shader.sh>

// Samplers
SAMPLER2D(s_sceneColor, 0);
SAMPLER2D(s_sceneDepth, 1);
SAMPLER2D(s_causticsTexture, 2);
SAMPLER2D(s_noiseTexture, 3);

// Uniforms
uniform vec4 u_underwaterFogColor;     // xyz = color, w = density
uniform vec4 u_underwaterFogParams;    // x = start, y = end, z = unused, w = unused
uniform vec4 u_underwaterTint;         // xyz = tint color, w = strength
uniform vec4 u_underwaterDistortion;   // x = strength, y = speed, z = time, w = saturation
uniform vec4 u_underwaterCaustics;     // x = intensity, y = scale, z = speed, w = unused
uniform vec4 u_cameraParams;           // x = near, y = far, z = unused, w = unused

// Helper functions
float linearizeDepth(float depth, float near, float far)
{
    return near * far / (far - depth * (far - near));
}

vec3 adjustSaturation(vec3 color, float saturation)
{
    vec3 luminance = vec3(0.2126, 0.7152, 0.0722);
    float lum = dot(color, luminance);
    return mix(vec3(lum, lum, lum), color, saturation);
}

void main()
{
    vec2 uv = v_texcoord0;
    float time = u_underwaterDistortion.z;

    // UV distortion for underwater waviness
    float distortionStrength = u_underwaterDistortion.x;
    float distortionSpeed = u_underwaterDistortion.y;

    vec2 noiseUV = uv * 3.0 + time * distortionSpeed * 0.1;
    vec2 noise = texture2D(s_noiseTexture, noiseUV).rg * 2.0 - 1.0;

    vec2 distortedUV = uv + noise * distortionStrength;
    distortedUV = clamp(distortedUV, 0.001, 0.999);  // Prevent edge artifacts

    // Sample scene color with distortion
    vec3 sceneColor = texture2D(s_sceneColor, distortedUV).rgb;

    // Sample depth
    float depth = texture2D(s_sceneDepth, distortedUV).r;
    float linearDepth = linearizeDepth(depth, u_cameraParams.x, u_cameraParams.y);

    // Underwater fog
    vec3 fogColor = u_underwaterFogColor.xyz;
    float fogDensity = u_underwaterFogColor.w;
    float fogStart = u_underwaterFogParams.x;
    float fogEnd = u_underwaterFogParams.y;

    float fogFactor = saturate((linearDepth - fogStart) / (fogEnd - fogStart));
    fogFactor = 1.0 - exp(-fogFactor * fogDensity);

    vec3 foggedColor = mix(sceneColor, fogColor, fogFactor);

    // Color tint (blue/green shift)
    vec3 tintColor = u_underwaterTint.xyz;
    float tintStrength = u_underwaterTint.w;
    vec3 tintedColor = mix(foggedColor, foggedColor * tintColor, tintStrength);

    // Saturation adjustment (underwater tends to be desaturated)
    float saturation = u_underwaterDistortion.w;
    vec3 adjustedColor = adjustSaturation(tintedColor, saturation);

    // Caustics on surfaces (simple approximation)
    float causticsIntensity = u_underwaterCaustics.x;
    float causticsScale = u_underwaterCaustics.y;
    float causticsSpeed = u_underwaterCaustics.z;

    vec2 causticsUV = uv * causticsScale + time * causticsSpeed * 0.05;
    vec3 caustics = texture2D(s_causticsTexture, causticsUV).rgb;

    // Apply caustics based on depth (stronger near surfaces)
    float causticsDepthFade = 1.0 - saturate(linearDepth / 20.0);
    adjustedColor += caustics * causticsIntensity * causticsDepthFade * (1.0 - fogFactor);

    // Vignette (darker at edges underwater)
    vec2 vignetteUV = uv * 2.0 - 1.0;
    float vignette = 1.0 - dot(vignetteUV, vignetteUV) * 0.3;
    adjustedColor *= vignette;

    // Light absorption (red fades first, then green)
    float absorption = saturate(linearDepth / 30.0);
    adjustedColor.r *= 1.0 - absorption * 0.6;
    adjustedColor.g *= 1.0 - absorption * 0.3;

    gl_FragColor = vec4(adjustedColor, 1.0);
}
