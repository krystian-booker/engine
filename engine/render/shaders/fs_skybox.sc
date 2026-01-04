$input v_worldDir

#include <bgfx_shader.sh>

SAMPLERCUBE(s_skybox, 0);

uniform vec4 u_skyboxParams;  // x=intensity, y=rotation_y (radians), zw=unused

// Rotate direction around Y axis
vec3 rotateY(vec3 dir, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return vec3(
        c * dir.x + s * dir.z,
        dir.y,
        -s * dir.x + c * dir.z
    );
}

void main()
{
    // Normalize the world direction
    vec3 dir = normalize(v_worldDir);

    // Apply Y-axis rotation
    float rotation = u_skyboxParams.y;
    dir = rotateY(dir, rotation);

    // Sample the cubemap
    vec3 color = textureCube(s_skybox, dir).rgb;

    // Apply intensity
    float intensity = u_skyboxParams.x;
    color *= intensity;

    gl_FragColor = vec4(color, 1.0);
}
