$input v_texcoord0

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_tileMax, 0);         // Tile max velocity buffer

// Parameters
uniform vec4 u_texelSize;        // xy=1/tile_size, zw=tile_size

void main()
{
    vec2 uv = v_texcoord0;
    vec2 texel = u_texelSize.xy;

    // Find maximum velocity in 3x3 neighborhood of tiles
    vec2 maxVelocity = vec2(0.5, 0.5);
    float maxMagnitude = 0.0;

    // Sample 3x3 neighborhood
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            vec2 offset = vec2(float(x), float(y)) * texel;
            vec2 sampleUV = uv + offset;

            // Clamp to valid range
            sampleUV = clamp(sampleUV, vec2_splat(0.0), vec2_splat(1.0));

            vec2 velocity = texture2D(s_tileMax, sampleUV).rg;

            // Decode and check magnitude
            vec2 decoded = velocity - 0.5;
            float magnitude = length(decoded);

            if (magnitude > maxMagnitude)
            {
                maxMagnitude = magnitude;
                maxVelocity = velocity;
            }
        }
    }

    gl_FragColor = vec4(maxVelocity, 0.0, 1.0);
}
