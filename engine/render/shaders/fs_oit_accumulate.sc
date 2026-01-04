$input v_texcoord0

#include <bgfx_shader.sh>

// OIT Accumulation Fragment Shader
// This shader outputs to MRT:
// - Target 0: Accumulation (RGB = weighted color sum, A = weighted alpha sum)
// - Target 1: Revealage (R = (1-alpha) product)

SAMPLER2D(s_albedo, 0);

uniform vec4 u_oitParams;      // x=weight_power, y=max_distance, z=near_plane, w=far_plane
uniform vec4 u_materialColor;  // Material base color/tint

// Calculate depth-based weight for OIT
// Higher weight = closer to camera = more visible
float calculateWeight(float z, float alpha)
{
    float power = u_oitParams.x;
    float maxDist = u_oitParams.y;

    // Clamp z to valid range
    z = clamp(z, 0.001, maxDist);

    // Weight function based on distance and alpha
    // Closer objects and more opaque objects get higher weights
    float weight = pow(max(alpha, 0.0), power) * clamp(0.03 / (1e-5 + pow(z / maxDist, 4.0)), 1e-2, 3e3);

    return weight;
}

void main()
{
    // Sample texture
    vec4 texColor = texture2D(s_albedo, v_texcoord0);
    vec4 color = texColor * u_materialColor;

    // Skip nearly transparent pixels
    if (color.a < 0.01)
    {
        discard;
    }

    // Get fragment depth (linearized)
    float z = gl_FragCoord.z;

    // Calculate weight
    float weight = calculateWeight(z, color.a);

    // Output to accumulation buffer (MRT 0)
    // RGB = premultiplied color * weight
    // A = alpha * weight
    gl_FragData[0] = vec4(color.rgb * color.a * weight, color.a * weight);

    // Output to revealage buffer (MRT 1)
    // Store (1 - alpha) for the product
    gl_FragData[1] = vec4(color.a, 0.0, 0.0, 1.0);
}
