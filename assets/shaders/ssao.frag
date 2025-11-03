// Screen-Space Ambient Occlusion
// Samples depth buffer in view space to estimate ambient occlusion
#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out float outOcclusion;

layout(binding = 0) uniform sampler2D depthTexture;
layout(binding = 1) uniform sampler2D normalTexture;
layout(binding = 2) uniform sampler2D noiseTexture;

layout(binding = 3) uniform SSAOParams {
    mat4 projection;
    mat4 view;
    vec4 samples[64];  // Kernel samples (xyz = offset, w = unused)
    vec2 noiseScale;   // Screen size / noise size
    float radius;
    float bias;
    float intensity;
    uint sampleCount;
} ssao;

// Reconstruct view space position from depth
vec3 ViewPosFromDepth(vec2 uv, float depth) {
    // Convert depth to NDC
    vec4 clipSpacePos = vec4(uv * 2.0 - 1.0, depth, 1.0);

    // Unproject
    vec4 viewSpacePos = inverse(ssao.projection) * clipSpacePos;
    return viewSpacePos.xyz / viewSpacePos.w;
}

void main() {
    // Get view space position and normal
    float depth = texture(depthTexture, fragTexCoord).r;

    // Skip skybox
    if (depth >= 1.0) {
        outOcclusion = 1.0;
        return;
    }

    vec3 viewPos = ViewPosFromDepth(fragTexCoord, depth);
    vec3 normal = normalize(texture(normalTexture, fragTexCoord).xyz * 2.0 - 1.0);
    normal = mat3(ssao.view) * normal;  // Transform to view space

    // Get random rotation vector from noise texture
    vec3 randomVec = normalize(texture(noiseTexture, fragTexCoord * ssao.noiseScale).xyz * 2.0 - 1.0);

    // Create TBN matrix (Gram-Schmidt process)
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // Sample kernel
    float occlusion = 0.0;
    for (uint i = 0; i < ssao.sampleCount; ++i) {
        // Get sample position in view space
        vec3 samplePos = TBN * ssao.samples[i].xyz;
        samplePos = viewPos + samplePos * ssao.radius;

        // Project sample position to screen space
        vec4 offset = vec4(samplePos, 1.0);
        offset = ssao.projection * offset;
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;

        // Get sample depth
        float sampleDepth = texture(depthTexture, offset.xy).r;
        vec3 sampleViewPos = ViewPosFromDepth(offset.xy, sampleDepth);

        // Range check and accumulate
        float rangeCheck = smoothstep(0.0, 1.0, ssao.radius / abs(viewPos.z - sampleViewPos.z));
        occlusion += (sampleViewPos.z >= samplePos.z + ssao.bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(ssao.sampleCount));
    occlusion = pow(occlusion, ssao.intensity);

    outOcclusion = occlusion;
}
