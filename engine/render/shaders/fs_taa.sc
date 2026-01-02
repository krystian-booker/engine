$input v_texcoord0

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_current, 0);      // Current frame
SAMPLER2D(s_history, 1);      // Previous frame (history buffer)
SAMPLER2D(s_depth, 2);        // Depth buffer
SAMPLER2D(s_velocity, 3);     // Motion vectors (optional)

// TAA parameters
uniform vec4 u_taaParams;     // x=blend_factor, y=motion_scale, z=use_velocity, w=sharpness
uniform vec4 u_texelSize;     // xy=1/resolution, zw=resolution

// RGB to YCoCg conversion for neighborhood clamping
vec3 RGBToYCoCg(vec3 rgb)
{
    float Y  = dot(rgb, vec3(0.25, 0.5, 0.25));
    float Co = dot(rgb, vec3(0.5, 0.0, -0.5));
    float Cg = dot(rgb, vec3(-0.25, 0.5, -0.25));
    return vec3(Y, Co, Cg);
}

vec3 YCoCgToRGB(vec3 ycocg)
{
    float Y  = ycocg.x;
    float Co = ycocg.y;
    float Cg = ycocg.z;
    return vec3(Y + Co - Cg, Y + Cg, Y - Co - Cg);
}

// Catmull-Rom filter for sharper history sampling
vec3 sampleHistoryCatmullRom(vec2 uv)
{
    vec2 position = uv * u_texelSize.zw;
    vec2 centerPosition = floor(position - 0.5) + 0.5;
    vec2 f = position - centerPosition;
    vec2 f2 = f * f;
    vec2 f3 = f * f2;

    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w2 = -1.5 * f3 + 2.0 * f2 + 0.5 * f;
    vec2 w3 = 0.5 * (f3 - f2);

    vec2 w12 = w1 + w2;
    vec2 tc12 = (centerPosition + w2 / w12) * u_texelSize.xy;
    vec2 tc0 = (centerPosition - 1.0) * u_texelSize.xy;
    vec2 tc3 = (centerPosition + 2.0) * u_texelSize.xy;

    vec3 result =
        texture2D(s_history, vec2(tc0.x, tc0.y)).rgb * (w0.x * w0.y) +
        texture2D(s_history, vec2(tc12.x, tc0.y)).rgb * (w12.x * w0.y) +
        texture2D(s_history, vec2(tc3.x, tc0.y)).rgb * (w3.x * w0.y) +
        texture2D(s_history, vec2(tc0.x, tc12.y)).rgb * (w0.x * w12.y) +
        texture2D(s_history, vec2(tc12.x, tc12.y)).rgb * (w12.x * w12.y) +
        texture2D(s_history, vec2(tc3.x, tc12.y)).rgb * (w3.x * w12.y) +
        texture2D(s_history, vec2(tc0.x, tc3.y)).rgb * (w0.x * w3.y) +
        texture2D(s_history, vec2(tc12.x, tc3.y)).rgb * (w12.x * w3.y) +
        texture2D(s_history, vec2(tc3.x, tc3.y)).rgb * (w3.x * w3.y);

    return max(result, vec3_splat(0.0));
}

void main()
{
    vec2 uv = v_texcoord0;
    vec2 texelSize = u_texelSize.xy;

    // Sample current frame
    vec3 current = texture2D(s_current, uv).rgb;

    // Get motion vector
    vec2 velocity = vec2_splat(0.0);
    if (u_taaParams.z > 0.5)
    {
        velocity = texture2D(s_velocity, uv).rg * u_taaParams.y;
    }

    // Reproject to previous frame position
    vec2 historyUV = uv - velocity;

    // Reject history if reprojected outside screen
    if (historyUV.x < 0.0 || historyUV.x > 1.0 || historyUV.y < 0.0 || historyUV.y > 1.0)
    {
        gl_FragColor = vec4(current, 1.0);
        return;
    }

    // Sample history with Catmull-Rom filter for sharpness
    vec3 history;
    if (u_taaParams.w > 0.5)
    {
        history = sampleHistoryCatmullRom(historyUV);
    }
    else
    {
        history = texture2D(s_history, historyUV).rgb;
    }

    // Neighborhood clamping in YCoCg space
    // Sample 3x3 neighborhood of current frame
    vec3 s0 = texture2D(s_current, uv + vec2(-texelSize.x, -texelSize.y)).rgb;
    vec3 s1 = texture2D(s_current, uv + vec2( 0.0, -texelSize.y)).rgb;
    vec3 s2 = texture2D(s_current, uv + vec2( texelSize.x, -texelSize.y)).rgb;
    vec3 s3 = texture2D(s_current, uv + vec2(-texelSize.x, 0.0)).rgb;
    vec3 s4 = current;
    vec3 s5 = texture2D(s_current, uv + vec2( texelSize.x, 0.0)).rgb;
    vec3 s6 = texture2D(s_current, uv + vec2(-texelSize.x, texelSize.y)).rgb;
    vec3 s7 = texture2D(s_current, uv + vec2( 0.0, texelSize.y)).rgb;
    vec3 s8 = texture2D(s_current, uv + vec2( texelSize.x, texelSize.y)).rgb;

    // Convert to YCoCg
    vec3 y0 = RGBToYCoCg(s0);
    vec3 y1 = RGBToYCoCg(s1);
    vec3 y2 = RGBToYCoCg(s2);
    vec3 y3 = RGBToYCoCg(s3);
    vec3 y4 = RGBToYCoCg(s4);
    vec3 y5 = RGBToYCoCg(s5);
    vec3 y6 = RGBToYCoCg(s6);
    vec3 y7 = RGBToYCoCg(s7);
    vec3 y8 = RGBToYCoCg(s8);

    // Compute bounding box
    vec3 minColor = min(y0, min(y1, min(y2, min(y3, min(y4, min(y5, min(y6, min(y7, y8))))))));
    vec3 maxColor = max(y0, max(y1, max(y2, max(y3, max(y4, max(y5, max(y6, max(y7, y8))))))));

    // Variance clipping for tighter bounds
    vec3 mean = (y0 + y1 + y2 + y3 + y4 + y5 + y6 + y7 + y8) / 9.0;
    vec3 variance = ((y0 - mean) * (y0 - mean) +
                     (y1 - mean) * (y1 - mean) +
                     (y2 - mean) * (y2 - mean) +
                     (y3 - mean) * (y3 - mean) +
                     (y4 - mean) * (y4 - mean) +
                     (y5 - mean) * (y5 - mean) +
                     (y6 - mean) * (y6 - mean) +
                     (y7 - mean) * (y7 - mean) +
                     (y8 - mean) * (y8 - mean)) / 9.0;
    vec3 stddev = sqrt(max(variance, vec3_splat(0.0)));

    // Tighter bounds using variance
    float gamma = 1.0;
    minColor = mean - gamma * stddev;
    maxColor = mean + gamma * stddev;

    // Clamp history to neighborhood bounds
    vec3 historyYCoCg = RGBToYCoCg(history);
    historyYCoCg = clamp(historyYCoCg, minColor, maxColor);
    history = YCoCgToRGB(historyYCoCg);

    // Calculate velocity-based blend factor
    float velocityLength = length(velocity * u_texelSize.zw);
    float velocityWeight = clamp(velocityLength * 0.1, 0.0, 0.5);

    // Blend current with clamped history
    float blendFactor = u_taaParams.x;
    blendFactor = mix(blendFactor, 0.5, velocityWeight);  // Reduce history weight for fast motion

    vec3 result = mix(current, history, blendFactor);

    gl_FragColor = vec4(result, 1.0);
}
