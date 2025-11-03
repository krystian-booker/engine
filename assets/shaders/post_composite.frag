// Post-processing composition shader
// Combines HDR, bloom, SSAO, applies tone mapping and color grading
#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D hdrInput;
layout(binding = 1) uniform sampler2D bloomTexture;
layout(binding = 2) uniform sampler2D ssaoTexture;
layout(binding = 3) uniform sampler3D colorGradingLUT;  // Optional

layout(push_constant) uniform PushConstants {
    float exposure;
    float bloomIntensity;
    uint toneMapper;  // 0=None, 1=Reinhard, 2=Uncharted2, 3=ACES
    uint enableBloom;
    uint enableSSAO;
    uint enableColorGrading;
    float vignetteIntensity;
    float vignetteRadius;
} pc;

// ============================================================================
// Tone Mapping Operators
// ============================================================================

vec3 ReinhardToneMapping(vec3 color) {
    return color / (color + vec3(1.0));
}

vec3 ReinhardLuminanceToneMapping(vec3 color) {
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float toneMappedLuma = luma / (1.0 + luma);
    return color * (toneMappedLuma / (luma + 0.0001));
}

// Uncharted 2 filmic tone mapping
vec3 Uncharted2Tonemap(vec3 x) {
    float A = 0.15;  // Shoulder strength
    float B = 0.50;  // Linear strength
    float C = 0.10;  // Linear angle
    float D = 0.20;  // Toe strength
    float E = 0.02;  // Toe numerator
    float F = 0.30;  // Toe denominator

    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 Uncharted2ToneMapping(vec3 color) {
    float exposureBias = 2.0;
    vec3 curr = Uncharted2Tonemap(color * exposureBias);
    vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(11.2));
    return curr * whiteScale;
}

// ACES filmic tone mapping (approximate)
vec3 ACESToneMapping(vec3 color) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// ACES fitted (Stephen Hill)
vec3 ACESFittedToneMapping(vec3 color) {
    mat3 inputMatrix = mat3(
        0.59719, 0.35458, 0.04823,
        0.07600, 0.90834, 0.01566,
        0.02840, 0.13383, 0.83777
    );

    mat3 outputMatrix = mat3(
        1.60475, -0.53108, -0.07367,
        -0.10208,  1.10813, -0.00605,
        -0.00327, -0.07276,  1.07602
    );

    vec3 v = inputMatrix * color;

    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;

    return outputMatrix * (a / b);
}

// ============================================================================
// Color Grading
// ============================================================================

vec3 ApplyColorGrading(vec3 color) {
    // Map [0,1] to LUT coordinates
    vec3 scale = vec3(31.0 / 32.0);
    vec3 offset = vec3(0.5 / 32.0);
    vec3 lutCoords = color * scale + offset;

    return texture(colorGradingLUT, lutCoords).rgb;
}

// ============================================================================
// Vignette
// ============================================================================

float Vignette(vec2 uv, float intensity, float radius) {
    vec2 center = uv - 0.5;
    float dist = length(center);
    float vignette = smoothstep(radius, radius - 0.5, dist);
    return mix(1.0, vignette, intensity);
}

// ============================================================================
// Main
// ============================================================================

void main() {
    vec2 uv = fragTexCoord;

    // Sample inputs
    vec3 hdrColor = texture(hdrInput, uv).rgb;

    // Apply bloom
    if (pc.enableBloom != 0) {
        vec3 bloomColor = texture(bloomTexture, uv).rgb;
        hdrColor += bloomColor * pc.bloomIntensity;
    }

    // Apply SSAO
    if (pc.enableSSAO != 0) {
        float occlusion = texture(ssaoTexture, uv).r;
        hdrColor *= occlusion;
    }

    // Apply exposure
    vec3 exposedColor = hdrColor * pc.exposure;

    // Tone mapping
    vec3 toneMapped;
    if (pc.toneMapper == 1) {
        toneMapped = ReinhardToneMapping(exposedColor);
    } else if (pc.toneMapper == 2) {
        toneMapped = Uncharted2ToneMapping(exposedColor);
    } else if (pc.toneMapper == 3) {
        toneMapped = ACESToneMapping(exposedColor);
    } else if (pc.toneMapper == 4) {
        toneMapped = ACESFittedToneMapping(exposedColor);
    } else {
        toneMapped = exposedColor;
    }

    // Color grading
    if (pc.enableColorGrading != 0) {
        toneMapped = ApplyColorGrading(toneMapped);
    }

    // Vignette
    if (pc.vignetteIntensity > 0.0) {
        float vig = Vignette(uv, pc.vignetteIntensity, pc.vignetteRadius);
        toneMapped *= vig;
    }

    // Gamma correction
    vec3 gammaCorrected = pow(toneMapped, vec3(1.0 / 2.2));

    outColor = vec4(gammaCorrected, 1.0);
}
