$input v_texcoord0

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_near, 0);        // Near field (or far field)
SAMPLER2D(s_coc, 1);         // Circle of Confusion

// Parameters
uniform vec4 u_dofParams;    // x=blur_radius, y=sample_count, z=intensity, w=is_near_field
uniform vec4 u_texelSize;    // xy=1/size, zw=size

// Hexagonal bokeh kernel
// Based on separable hexagonal blur approximation
vec2 get_bokeh_offset(int index, float radius)
{
    // Golden angle spiral for better distribution
    float golden_angle = 2.39996322972865332;  // PI * (3 - sqrt(5))
    float fi = float(index);
    float r = sqrt(fi + 0.5) / sqrt(float(int(u_dofParams.y)));
    float theta = fi * golden_angle;

    return vec2(cos(theta), sin(theta)) * r * radius;
}

void main()
{
    vec2 uv = v_texcoord0;
    vec2 texel = u_texelSize.xy;

    float blur_radius = u_dofParams.x;
    int sample_count = int(u_dofParams.y);
    float intensity = u_dofParams.z;
    bool is_near = u_dofParams.w > 0.5;

    // Sample center
    vec4 centerColor = texture2D(s_near, uv);
    float centerCoc = texture2D(s_coc, uv).r * 2.0 - 1.0;

    // Determine blur amount based on CoC
    float cocAbs = is_near ? max(-centerCoc, 0.0) : max(centerCoc, 0.0);
    float radius = blur_radius * cocAbs;

    // Early out if no blur needed
    if (radius < 0.01 || sample_count <= 0)
    {
        gl_FragColor = centerColor;
        return;
    }

    // Accumulate bokeh samples
    vec4 colorSum = vec4_splat(0.0);
    float weightSum = 0.0;

    // Limit sample count for performance
    sample_count = min(sample_count, 32);

    for (int i = 0; i < sample_count; i++)
    {
        vec2 offset = get_bokeh_offset(i, radius) * texel;
        vec2 sampleUV = uv + offset;

        vec4 sampleColor = texture2D(s_near, sampleUV);
        float sampleCoc = texture2D(s_coc, sampleUV).r * 2.0 - 1.0;

        // Weight based on CoC (spread contribution of blurry samples)
        float sampleCocAbs = is_near ? max(-sampleCoc, 0.0) : max(sampleCoc, 0.0);

        // For near field: only include samples with larger or equal CoC
        // For far field: weight by CoC magnitude
        float weight;
        if (is_near)
        {
            // Near field: dilate (include samples from blurry foreground)
            weight = saturate(sampleCocAbs * 2.0);
        }
        else
        {
            // Far field: standard weighted average
            weight = saturate(sampleCocAbs);
        }

        // Brightness weighting for bokeh highlights
        float luminance = dot(sampleColor.rgb, vec3(0.299, 0.587, 0.114));
        float brightnessWeight = 1.0 + luminance * 2.0;

        weight *= brightnessWeight;

        colorSum += sampleColor * weight;
        weightSum += weight;
    }

    // Add center sample
    float centerWeight = 1.0 - cocAbs;
    colorSum += centerColor * centerWeight;
    weightSum += centerWeight;

    // Normalize
    vec4 result = colorSum / max(weightSum, 0.001);

    // Apply intensity
    result.rgb *= intensity;

    // Preserve alpha for compositing
    result.a = cocAbs;

    gl_FragColor = result;
}
