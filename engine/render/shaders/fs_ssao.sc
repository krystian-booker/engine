$input v_texcoord0

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_depth, 0);       // Scene depth buffer
SAMPLER2D(s_normal, 1);      // View-space normals
SAMPLER2D(s_noise, 2);       // Rotation noise texture

// SSAO parameters
uniform vec4 u_ssaoParams;    // x=radius, y=bias, z=intensity, w=power
uniform vec4 u_projParams;    // x=near, y=far, z=aspect, w=tanHalfFov
uniform vec4 u_noiseScale;    // xy=noise texture scale, zw=screen size
uniform vec4 u_samples[16];   // Hemisphere sample kernel (up to 64 samples, using 16 vec4s)

// Reconstruct view-space position from depth
vec3 getViewPos(vec2 uv, float depth)
{
    // Linearize depth
    float near = u_projParams.x;
    float far = u_projParams.y;
    float linearDepth = near * far / (far - depth * (far - near));

    // Reconstruct view-space position
    vec2 ndc = uv * 2.0 - 1.0;
    float aspect = u_projParams.z;
    float tanHalfFov = u_projParams.w;

    vec3 viewPos;
    viewPos.x = ndc.x * aspect * tanHalfFov * linearDepth;
    viewPos.y = ndc.y * tanHalfFov * linearDepth;
    viewPos.z = -linearDepth;

    return viewPos;
}

// Get view-space normal from G-buffer
vec3 getViewNormal(vec2 uv)
{
    vec3 normal = texture2D(s_normal, uv).xyz * 2.0 - 1.0;
    return normalize(normal);
}

void main()
{
    vec2 uv = v_texcoord0;

    // Sample depth and reconstruct position
    float depth = texture2D(s_depth, uv).r;

    // Skip sky pixels
    if (depth >= 0.9999)
    {
        gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    vec3 fragPos = getViewPos(uv, depth);
    vec3 normal = getViewNormal(uv);

    // Sample noise texture for random rotation
    vec2 noiseScale = u_noiseScale.xy;
    vec3 randomVec = texture2D(s_noise, uv * noiseScale).xyz * 2.0 - 1.0;
    randomVec.z = 0.0;
    randomVec = normalize(randomVec);

    // Create TBN matrix for orienting samples along normal
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // Sample kernel
    float radius = u_ssaoParams.x;
    float bias = u_ssaoParams.y;
    float occlusion = 0.0;
    int sampleCount = 32;

    for (int i = 0; i < sampleCount && i < 64; i++)
    {
        // Get sample position in kernel space
        int idx = i / 4;
        int comp = i - idx * 4;
        vec3 sampleDir;
        if (comp == 0) sampleDir = u_samples[idx].xyz;
        else if (comp == 1) sampleDir = vec3(u_samples[idx].w, u_samples[idx + 1].xy);
        else if (comp == 2) sampleDir = vec3(u_samples[idx + 1].zw, u_samples[idx + 2].x);
        else sampleDir = u_samples[idx + 2].yzw;

        // Transform to view space
        vec3 samplePos = TBN * sampleDir;
        samplePos = fragPos + samplePos * radius;

        // Project to screen space
        vec4 offset = vec4(samplePos, 1.0);
        offset.xy = offset.xy / -offset.z;  // Perspective divide
        offset.xy = offset.xy * 0.5 + 0.5;  // To UV space

        // Get depth at sample point
        float sampleDepth = texture2D(s_depth, offset.xy).r;
        vec3 sampleViewPos = getViewPos(offset.xy, sampleDepth);

        // Range check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleViewPos.z));

        // Compare depths
        occlusion += (sampleViewPos.z >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    // Normalize and apply intensity/power
    occlusion = 1.0 - (occlusion / float(sampleCount));
    occlusion = pow(occlusion, u_ssaoParams.w) * u_ssaoParams.z;

    // Output ambient occlusion
    gl_FragColor = vec4(vec3_splat(occlusion), 1.0);
}
