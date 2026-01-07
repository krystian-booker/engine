$input v_texcoord0

#include <bgfx_shader.sh>

// Input textures
SAMPLER2D(s_velocity, 0);        // Full-resolution velocity buffer

// Parameters
uniform vec4 u_motionParams;     // x=tile_size, y=full_width, z=full_height, w=unused

void main()
{
    vec2 uv = v_texcoord0;

    // Get tile parameters
    float tileSize = u_motionParams.x;
    vec2 fullSize = vec2(u_motionParams.y, u_motionParams.z);

    // Calculate pixel coordinates in the tile
    vec2 tileCoord = floor(uv * fullSize / tileSize);
    vec2 tileStart = tileCoord * tileSize;

    // Find maximum velocity magnitude in this tile
    vec2 maxVelocity = vec2(0.5, 0.5);  // Start at zero (encoded as 0.5)
    float maxMagnitude = 0.0;

    // Sample all pixels in the tile
    for (float y = 0.0; y < tileSize; y += 1.0)
    {
        for (float x = 0.0; x < tileSize; x += 1.0)
        {
            vec2 sampleCoord = (tileStart + vec2(x, y) + 0.5) / fullSize;

            // Clamp to valid range
            sampleCoord = clamp(sampleCoord, vec2_splat(0.0), vec2_splat(1.0));

            vec2 velocity = texture2D(s_velocity, sampleCoord).rg;

            // Decode velocity from [0, 1] to [-0.5, 0.5]
            vec2 decoded = velocity - 0.5;
            float magnitude = length(decoded);

            // Keep the velocity with maximum magnitude
            if (magnitude > maxMagnitude)
            {
                maxMagnitude = magnitude;
                maxVelocity = velocity;
            }
        }
    }

    gl_FragColor = vec4(maxVelocity, 0.0, 1.0);
}
