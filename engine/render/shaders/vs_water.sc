$input a_position, a_normal, a_texcoord0, a_tangent
$output v_worldPos, v_screenPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_texcoord1, v_texcoord2, v_foamUV

#include <bgfx_shader.sh>

// Water uniforms
uniform vec4 u_waterWaveParams;    // xy = direction, z = amplitude, w = frequency
uniform vec4 u_waterWaveParams2;   // x = speed, y = steepness, z = time, w = unused
uniform vec4 u_waterNormalScroll;  // xy = scroll1, zw = scroll2
uniform vec4 u_waterNormalScale;   // xy = scale1, zw = scale2
uniform vec4 u_waterFoamParams;    // x = foam scroll x, y = foam scroll y, z = caustics scale, w = caustics speed

// Gerstner wave calculation
vec3 gerstnerWave(vec2 pos, vec2 dir, float amplitude, float frequency, float steepness, float time, float speed)
{
    float k = 2.0 * 3.14159 / frequency;
    float c = sqrt(9.81 / k);
    float f = k * (dot(dir, pos) - c * speed * time);
    float a = amplitude * steepness / k;

    return vec3(
        dir.x * a * cos(f),
        amplitude * sin(f),
        dir.y * a * cos(f)
    );
}

void main()
{
    vec3 localPos = a_position;
    vec3 displacement = vec3(0.0, 0.0, 0.0);

    // Get wave parameters
    vec2 waveDir = normalize(u_waterWaveParams.xy);
    float amplitude = u_waterWaveParams.z;
    float frequency = u_waterWaveParams.w;
    float speed = u_waterWaveParams2.x;
    float steepness = u_waterWaveParams2.y;
    float time = u_waterWaveParams2.z;

    // Apply Gerstner wave displacement
    if (amplitude > 0.001)
    {
        // Main wave
        displacement += gerstnerWave(localPos.xz, waveDir, amplitude, frequency, steepness, time, speed);

        // Secondary wave (perpendicular, smaller)
        vec2 waveDir2 = vec2(-waveDir.y, waveDir.x);
        displacement += gerstnerWave(localPos.xz, waveDir2, amplitude * 0.5, frequency * 0.7, steepness * 0.8, time, speed * 0.9);

        // Tertiary wave (diagonal, even smaller)
        vec2 waveDir3 = normalize(waveDir + waveDir2);
        displacement += gerstnerWave(localPos.xz, waveDir3, amplitude * 0.25, frequency * 1.3, steepness * 0.6, time, speed * 1.1);
    }

    localPos += displacement;

    // Transform to world space
    vec4 worldPos = mul(u_model[0], vec4(localPos, 1.0));

    // Calculate view-space position for depth
    vec4 viewPos = mul(u_view, worldPos);
    float viewSpaceDepth = -viewPos.z;

    v_worldPos = vec4(worldPos.xyz, viewSpaceDepth);

    // Transform normal to world space (will be recalculated in fragment for detail)
    mat3 normalMatrix = mat3(u_model[0][0].xyz, u_model[0][1].xyz, u_model[0][2].xyz);
    v_normal = normalize(mul(normalMatrix, a_normal));
    v_tangent = normalize(mul(normalMatrix, a_tangent.xyz));
    v_bitangent = cross(v_normal, v_tangent) * a_tangent.w;

    // Base UV
    v_texcoord0 = a_texcoord0;

    // Scrolling UVs for normal maps
    vec2 normalScroll1 = u_waterNormalScroll.xy * time;
    vec2 normalScroll2 = u_waterNormalScroll.zw * time;
    v_texcoord1 = a_texcoord0 * u_waterNormalScale.xy + normalScroll1;
    v_texcoord2 = a_texcoord0 * u_waterNormalScale.zw + normalScroll2;

    // Foam and caustics UVs
    vec2 foamScroll = vec2(u_waterFoamParams.x, u_waterFoamParams.y) * time;
    float causticsScale = u_waterFoamParams.z;
    float causticsSpeed = u_waterFoamParams.w;
    v_foamUV = vec4(
        a_texcoord0 * 4.0 + foamScroll,
        worldPos.xz * causticsScale + time * causticsSpeed
    );

    // Calculate clip and screen position
    vec4 clipPos = mul(u_viewProj, worldPos);
    v_screenPos = vec4(
        (clipPos.xy / clipPos.w) * 0.5 + 0.5,
        clipPos.zw
    );

    gl_Position = clipPos;
}
