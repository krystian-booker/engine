// Bright pass extraction for bloom
// Extracts pixels above threshold for bloom effect
#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D hdrInput;

layout(push_constant) uniform PushConstants {
    float threshold;
    float softThreshold;
} pc;

// Luminance calculation
float Luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// Soft threshold curve
float SoftThreshold(float value, float threshold, float softness) {
    float knee = threshold * softness;
    float soft = value - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.0001);
    return max(value - threshold, soft);
}

void main() {
    vec3 color = texture(hdrInput, fragTexCoord).rgb;

    float luma = Luminance(color);

    // Soft threshold
    float brightness = SoftThreshold(luma, pc.threshold, pc.softThreshold);

    // Preserve color ratio while extracting brightness
    vec3 brightColor = color * max(brightness / (luma + 0.0001), 0.0);

    outColor = vec4(brightColor, 1.0);
}
