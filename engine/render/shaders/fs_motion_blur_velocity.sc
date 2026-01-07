$input v_texcoord0

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_depth, 0);           // Scene depth buffer

// Motion blur parameters
uniform vec4 u_motionParams;     // x=shutter_fraction, y=max_velocity, z=unused, w=unused
uniform vec4 u_texelSize;        // xy=1/size, zw=size
uniform mat4 u_customViewProj;   // Current view-projection matrix
uniform mat4 u_prevViewProj;     // Previous frame view-projection matrix
uniform mat4 u_customInvViewProj;// Inverse of current view-projection

void main()
{
    vec2 uv = v_texcoord0;

    // Sample depth
    float depth = texture2D(s_depth, uv).r;

    // Skip sky pixels (far plane)
    if (depth >= 0.9999)
    {
        gl_FragColor = vec4(0.5, 0.5, 0.0, 0.0);  // Zero velocity (encoded as 0.5)
        return;
    }

    // Reconstruct world position from depth
    // Convert UV to clip space [-1, 1]
    vec2 clipXY = uv * 2.0 - 1.0;

    // Create clip-space position
#if BGFX_SHADER_LANGUAGE_HLSL
    vec4 clipPos = vec4(clipXY.x, -clipXY.y, depth, 1.0);
#else
    vec4 clipPos = vec4(clipXY, depth * 2.0 - 1.0, 1.0);
#endif

    // Transform to world space
    vec4 worldPos = mul(u_customInvViewProj, clipPos);
    worldPos /= worldPos.w;

    // Project to previous frame clip space
    vec4 prevClipPos = mul(u_prevViewProj, worldPos);
    prevClipPos /= prevClipPos.w;

    // Calculate velocity in screen space
    vec2 prevUV = prevClipPos.xy * 0.5 + 0.5;
#if BGFX_SHADER_LANGUAGE_HLSL
    prevUV.y = 1.0 - prevUV.y;
#endif

    // Velocity = current position - previous position
    vec2 velocity = uv - prevUV;

    // Apply shutter fraction (simulates exposure time)
    velocity *= u_motionParams.x;

    // Clamp to max velocity
    float maxVel = u_motionParams.y;
    float velLength = length(velocity);
    if (velLength > maxVel)
    {
        velocity = velocity * (maxVel / velLength);
    }

    // Encode velocity to [0, 1] range (0.5 = no motion)
    // Range: [-maxVel, maxVel] -> [0, 1]
    vec2 encodedVelocity = velocity / (maxVel * 2.0) + 0.5;

    gl_FragColor = vec4(encodedVelocity, 0.0, 1.0);
}
