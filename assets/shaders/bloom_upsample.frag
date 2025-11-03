// Bloom upsample with tent filter
// Combines lower resolution bloom with current level
#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D inputTexture;

layout(push_constant) uniform PushConstants {
    vec2 texelSize;
    float radius;
} pc;

void main() {
    // 9-tap tent filter (bilinear upsampling)
    vec2 uv = fragTexCoord;
    float r = pc.radius;
    vec2 ts = pc.texelSize;

    vec3 result = vec3(0.0);

    // Center
    result += texture(inputTexture, uv).rgb * 4.0;

    // Corners
    result += texture(inputTexture, uv + vec2(-ts.x, -ts.y) * r).rgb;
    result += texture(inputTexture, uv + vec2( ts.x, -ts.y) * r).rgb;
    result += texture(inputTexture, uv + vec2(-ts.x,  ts.y) * r).rgb;
    result += texture(inputTexture, uv + vec2( ts.x,  ts.y) * r).rgb;

    // Edges
    result += texture(inputTexture, uv + vec2(-ts.x, 0.0) * r).rgb * 2.0;
    result += texture(inputTexture, uv + vec2( ts.x, 0.0) * r).rgb * 2.0;
    result += texture(inputTexture, uv + vec2(0.0, -ts.y) * r).rgb * 2.0;
    result += texture(inputTexture, uv + vec2(0.0,  ts.y) * r).rgb * 2.0;

    result /= 16.0;

    outColor = vec4(result, 1.0);
}
