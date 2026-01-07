$input v_screenPos, v_worldPos

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_depth, 0);           // Scene depth buffer
SAMPLER2D(s_gbufferNormal, 1);   // G-buffer normals
SAMPLER2D(s_decalAlbedo, 2);     // Decal albedo texture
SAMPLER2D(s_decalNormal, 3);     // Decal normal map

// Uniforms
uniform vec4 u_decalParams;      // x=angle_fade_start, y=angle_fade_end, z=blend_mode, w=channels
uniform vec4 u_decalColor;       // Base color and alpha
uniform vec4 u_decalSize;        // xyz=decal size, w=unused
uniform mat4 u_customInvViewProj;// Inverse view-projection for depth reconstruction
uniform mat4 u_decalInvTransform;// Inverse decal transform

// Reconstruct world position from depth
vec3 reconstructWorldPos(vec2 screenUV, float depth)
{
    // Convert to clip space
    vec2 clipXY = screenUV * 2.0 - 1.0;

#if BGFX_SHADER_LANGUAGE_HLSL
    vec4 clipPos = vec4(clipXY.x, -clipXY.y, depth, 1.0);
#else
    vec4 clipPos = vec4(clipXY, depth * 2.0 - 1.0, 1.0);
#endif

    // Transform to world space
    vec4 worldPos = mul(u_customInvViewProj, clipPos);
    return worldPos.xyz / worldPos.w;
}

void main()
{
    // Get screen UV from vertex position
    vec2 screenUV = v_screenPos.xy / v_screenPos.w * 0.5 + 0.5;
#if BGFX_SHADER_LANGUAGE_HLSL
    screenUV.y = 1.0 - screenUV.y;
#endif

    // Sample depth and reconstruct world position
    float depth = texture2D(s_depth, screenUV).r;

    // Skip sky pixels
    if (depth >= 0.9999)
    {
        discard;
    }

    vec3 worldPos = reconstructWorldPos(screenUV, depth);

    // Transform world position to decal local space
    vec4 localPos4 = mul(u_decalInvTransform, vec4(worldPos, 1.0));
    vec3 localPos = localPos4.xyz / localPos4.w;

    // Check if point is inside decal box (unit cube: -0.5 to 0.5)
    vec3 absLocal = abs(localPos);
    if (absLocal.x > 0.5 || absLocal.y > 0.5 || absLocal.z > 0.5)
    {
        discard;
    }

    // Calculate UV from local position (project from Z axis)
    vec2 decalUV = localPos.xy + 0.5;

    // Sample G-buffer normal for angle-based fading
    vec3 gbufferNormal = texture2D(s_gbufferNormal, screenUV).xyz * 2.0 - 1.0;
    gbufferNormal = normalize(gbufferNormal);

    // Calculate decal projection direction (local Z in world space)
    // Assuming decal projects along its local Z axis
    vec3 decalDir = normalize(mul(u_decalInvTransform, vec4(0.0, 0.0, -1.0, 0.0)).xyz);

    // Angle fade based on surface normal vs decal direction
    float angleDot = abs(dot(gbufferNormal, decalDir));
    float angleFadeStart = u_decalParams.x;
    float angleFadeEnd = u_decalParams.y;
    float angleFade = smoothstep(angleFadeEnd, angleFadeStart, angleDot);

    // Discard if facing away
    if (angleFade < 0.01)
    {
        discard;
    }

    // Sample decal textures
    vec4 decalAlbedo = texture2D(s_decalAlbedo, decalUV);

    // Apply base color
    decalAlbedo *= u_decalColor;

    // Apply angle fade
    decalAlbedo.a *= angleFade;

    // Edge fade (fade near decal boundaries)
    vec3 edgeDist = 0.5 - absLocal;
    float edgeFade = min(min(edgeDist.x, edgeDist.y), edgeDist.z) * 4.0;
    edgeFade = saturate(edgeFade);
    decalAlbedo.a *= edgeFade;

    // Sample decal normal map if available
    vec3 decalNormal = texture2D(s_decalNormal, decalUV).xyz * 2.0 - 1.0;

    // Transform decal normal to world space (simplified - assumes no scaling)
    // In a full implementation, you'd use a TBN matrix

    // Output
    // For deferred rendering, you'd write to multiple render targets:
    // RT0: Albedo (with alpha for blending)
    // RT1: Normal (blended with existing normal)
    // RT2: Roughness/Metallic

    // For now, output albedo with alpha blending
    gl_FragColor = decalAlbedo;
}
