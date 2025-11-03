// SSAO bilateral blur
// Preserves edges while smoothing occlusion
#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out float outOcclusion;

layout(binding = 0) uniform sampler2D ssaoInput;

layout(push_constant) uniform PushConstants {
    vec2 texelSize;
} pc;

void main() {
    vec2 uv = fragTexCoord;
    float result = 0.0;
    float total = 0.0;

    // 5x5 Gaussian blur
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * pc.texelSize;
            float weight = 1.0 / (1.0 + abs(float(x)) + abs(float(y)));

            result += texture(ssaoInput, uv + offset).r * weight;
            total += weight;
        }
    }

    outOcclusion = result / total;
}
