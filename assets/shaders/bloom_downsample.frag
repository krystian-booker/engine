// Bloom downsample with 13-tap filter
// Uses Karis average to prevent fireflies
#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D inputTexture;

layout(push_constant) uniform PushConstants {
    vec2 texelSize;
} pc;

// Karis average to prevent fireflies (bright single pixels)
vec3 KarisAverage(vec3 color) {
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return color / (1.0 + luma);
}

void main() {
    // 13-tap downsampling pattern
    vec2 uv = fragTexCoord;
    vec2 ts = pc.texelSize;

    // Center
    vec3 a = texture(inputTexture, uv).rgb;

    // 4 corners (diagonal)
    vec3 b = texture(inputTexture, uv + vec2(-ts.x, -ts.y)).rgb;
    vec3 c = texture(inputTexture, uv + vec2( ts.x, -ts.y)).rgb;
    vec3 d = texture(inputTexture, uv + vec2(-ts.x,  ts.y)).rgb;
    vec3 e = texture(inputTexture, uv + vec2( ts.x,  ts.y)).rgb;

    // 4 edge centers
    vec3 f = texture(inputTexture, uv + vec2(-ts.x * 2.0, 0.0)).rgb;
    vec3 g = texture(inputTexture, uv + vec2( ts.x * 2.0, 0.0)).rgb;
    vec3 h = texture(inputTexture, uv + vec2(0.0, -ts.y * 2.0)).rgb;
    vec3 i = texture(inputTexture, uv + vec2(0.0,  ts.y * 2.0)).rgb;

    // 4 intermediate corners
    vec3 j = texture(inputTexture, uv + vec2(-ts.x, -ts.y * 2.0)).rgb;
    vec3 k = texture(inputTexture, uv + vec2( ts.x, -ts.y * 2.0)).rgb;
    vec3 l = texture(inputTexture, uv + vec2(-ts.x,  ts.y * 2.0)).rgb;
    vec3 m = texture(inputTexture, uv + vec2( ts.x,  ts.y * 2.0)).rgb;

    // Apply Karis average to prevent fireflies
    vec3 result = KarisAverage(a) * 0.5;
    result += KarisAverage(b + c + d + e) * 0.125;
    result += KarisAverage(f + g + h + i) * 0.03125;
    result += KarisAverage(j + k + l + m) * 0.015625;

    outColor = vec4(result, 1.0);
}
