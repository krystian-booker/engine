$input v_texcoord0

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_depth, 0);           // Scene depth
SAMPLER2D(s_shadowMap0, 1);      // Shadow cascade 0
SAMPLER2D(s_shadowMap1, 2);      // Shadow cascade 1
SAMPLER2D(s_noise, 3);           // 3D noise for density variation

// Volumetric parameters
uniform vec4 u_volumetricParams;   // x=density, y=scattering, z=anisotropy, w=extinction
uniform vec4 u_fogColor;           // rgb=fog color, w=height falloff
uniform vec4 u_fogHeight;          // x=base height, y=falloff, z=noise scale, w=noise intensity
uniform vec4 u_lightDir;           // xyz=light direction, w=intensity
uniform vec4 u_lightColor;         // rgb=light color, w=unused
uniform vec4 u_cameraPos;          // xyz=camera position, w=unused
uniform vec4 u_projParams;         // x=near, y=far, z=unused, w=unused
// Note: u_invViewProj is provided by bgfx_shader.sh
uniform mat4 u_shadowMatrix;       // Light space matrix

// Ray march settings
#define MAX_STEPS 64
#define STEP_SIZE 0.5

// Henyey-Greenstein phase function
float henyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(max(denom, 0.0001), 1.5));
}

// Reconstruct world position from depth
vec3 getWorldPos(vec2 uv, float depth)
{
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
#if BGFX_SHADER_LANGUAGE_HLSL
    clipPos.y = -clipPos.y;
#endif
    vec4 worldPos = mul(u_invViewProj, clipPos);
    return worldPos.xyz / worldPos.w;
}

// Calculate fog density at a point
float getFogDensity(vec3 worldPos)
{
    float baseDensity = u_volumetricParams.x;

    // Height-based falloff
    float heightFalloff = u_fogColor.w;
    float baseHeight = u_fogHeight.x;
    float heightFactor = exp(-max(0.0, worldPos.y - baseHeight) * heightFalloff);

    // Noise variation
    float noiseScale = u_fogHeight.z;
    float noiseIntensity = u_fogHeight.w;
    vec3 noiseCoord = worldPos * noiseScale;
#if BGFX_SHADER_LANGUAGE_HLSL
    float noise = bgfxTexture2DLod(s_noise, noiseCoord.xz * 0.01, 0.0).r;
#else
    float noise = texture2D(s_noise, noiseCoord.xz * 0.01).r;
#endif
    noise = noise * noiseIntensity + (1.0 - noiseIntensity);

    return baseDensity * heightFactor * noise;
}

// Sample shadow map at world position
float getShadow(vec3 worldPos)
{
    vec4 shadowCoord = mul(u_shadowMatrix, vec4(worldPos, 1.0));
    shadowCoord.xyz /= shadowCoord.w;
    shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;

#if BGFX_SHADER_LANGUAGE_HLSL
    shadowCoord.y = 1.0 - shadowCoord.y;
#endif

    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0 || shadowCoord.z > 1.0)
    {
        return 1.0;
    }

    float shadowDepth = 
#if BGFX_SHADER_LANGUAGE_HLSL
        bgfxTexture2DLod(s_shadowMap0, shadowCoord.xy, 0.0).r;
#else
        texture2D(s_shadowMap0, shadowCoord.xy).r;
#endif
    float bias = 0.005;
    return shadowCoord.z - bias > shadowDepth ? 0.0 : 1.0;
}

void main()
{
    vec2 uv = v_texcoord0;

    // Get scene depth
    float depth = texture2D(s_depth, uv).r;

    // Skip sky
    if (depth >= 0.9999)
    {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    // Reconstruct world positions
    vec3 worldPosEnd = getWorldPos(uv, depth);
    vec3 worldPosStart = u_cameraPos.xyz;

    // Ray direction
    vec3 rayDir = normalize(worldPosEnd - worldPosStart);
    float rayLength = length(worldPosEnd - worldPosStart);

    // Light and view directions for phase function
    vec3 lightDir = normalize(u_lightDir.xyz);
    float cosTheta = dot(rayDir, -lightDir);

    // Phase function
    float phase = henyeyGreenstein(cosTheta, u_volumetricParams.z);

    // Ray march
    float stepSize = rayLength / float(MAX_STEPS);
    vec3 stepVec = rayDir * stepSize;
    vec3 currentPos = worldPosStart;

    vec3 scatteredLight = vec3_splat(0.0);
    float transmittance = 1.0;

    float extinction = u_volumetricParams.w;
    float scattering = u_volumetricParams.y;

    for (int i = 0; i < MAX_STEPS; i++)
    {
        // Get density at current position
        float density = getFogDensity(currentPos);

        if (density > 0.001)
        {
            // Sample shadow
            float shadow = getShadow(currentPos);

            // In-scattered light
            vec3 lightContrib = u_lightColor.rgb * u_lightDir.w * phase * shadow;

            // Add ambient
            vec3 ambient = u_fogColor.rgb * 0.1;
            lightContrib += ambient;

            // Beer-Lambert law
            float sampleExtinction = extinction * density * stepSize;
            float sampleTransmittance = exp(-sampleExtinction);

            // Accumulate
            vec3 sampleScatter = density * scattering * stepSize * lightContrib;
            scatteredLight += transmittance * sampleScatter;
            transmittance *= sampleTransmittance;

            // Early exit removed to keep loop iteration count static for HLSL compilation
        }

        currentPos += stepVec;
    }

    // Output: rgb = scattered light, a = transmittance
    gl_FragColor = vec4(scatteredLight, 1.0 - transmittance);
}
