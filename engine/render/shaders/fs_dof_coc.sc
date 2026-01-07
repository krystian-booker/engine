$input v_texcoord0

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_depth, 0);       // Scene depth buffer

// DOF parameters
uniform vec4 u_dofParams;    // x=focus_distance, y=focus_range, z=max_blur, w=aperture
uniform vec4 u_dofFocus;     // x=near_start, y=near_end, z=far_start, w=far_end
uniform vec4 u_projParams;   // Projection matrix parameters for depth linearization

// Linearize depth from [0,1] to view-space distance
float linearize_depth(float depth)
{
    // Using standard depth linearization for perspective projection
    // z = (2 * near * far) / (far + near - depth * (far - near))
    // Simplified using projection matrix elements passed in u_projParams

    // For reversed-Z: z = near * far / (far - depth * (far - near))
    float near = 0.1;
    float far = 1000.0;

#if BGFX_SHADER_LANGUAGE_HLSL
    // HLSL uses [0,1] depth range
    float z = near * far / (far - depth * (far - near));
#else
    // OpenGL uses [-1,1] depth range, convert to [0,1]
    float d = depth * 2.0 - 1.0;
    float z = (2.0 * near * far) / (far + near - d * (far - near));
#endif

    return z;
}

// Calculate Circle of Confusion
// Returns signed CoC: negative for near blur, positive for far blur
float calculate_coc(float depth, float focus_distance, float focus_range, float aperture)
{
    // Distance from focus plane
    float delta = depth - focus_distance;

    // Lens formula: CoC = aperture * |S2 - S1| / S2 * (f / (S1 - f))
    // Simplified for game use:
    float coc;

    if (delta < 0.0)
    {
        // Near field (in front of focus)
        float near_start = u_dofFocus.x;
        float near_end = u_dofFocus.y;
        float t = saturate((near_end - depth) / max(near_end - near_start, 0.001));
        coc = -t;  // Negative for near
    }
    else
    {
        // Far field (behind focus)
        float far_start = u_dofFocus.z;
        float far_end = u_dofFocus.w;
        float t = saturate((depth - far_start) / max(far_end - far_start, 0.001));
        coc = t;  // Positive for far
    }

    // Scale by max blur and aperture
    coc *= aperture * u_dofParams.z;

    return coc;
}

void main()
{
    vec2 uv = v_texcoord0;

    // Sample depth
    float depth = texture2D(s_depth, uv).r;

    // Skip sky pixels (far plane)
    if (depth >= 0.9999)
    {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    // Linearize depth to world units
    float linear_depth = linearize_depth(depth);

    // Calculate CoC
    float focus_distance = u_dofParams.x;
    float focus_range = u_dofParams.y;
    float aperture = u_dofParams.w;

    float coc = calculate_coc(linear_depth, focus_distance, focus_range, aperture);

    // Clamp to max blur radius
    coc = clamp(coc, -1.0, 1.0);

    // Output signed CoC (R channel)
    // Use 0.5 as center (in focus), <0.5 for near, >0.5 for far
    gl_FragColor = vec4(coc * 0.5 + 0.5, 0.0, 0.0, 1.0);
}
