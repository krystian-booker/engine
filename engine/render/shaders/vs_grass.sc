$input a_position, a_texcoord0, a_texcoord1, i_data0, i_data1
$output v_texcoord0, v_color, v_worldPos, v_fade

#include <bgfx_shader.sh>

uniform vec4 u_grassWind;   // xy=direction, z=time, w=strength
uniform vec4 u_grassParams; // x=blade_width, y=blade_height, z=alpha_cutoff, w=fade_start

void main()
{
    // Unpack instance data
    vec3 instancePos = i_data0.xyz;
    float instanceRot = i_data0.w;
    float instanceScale = i_data1.x;
    float instanceBend = i_data1.y;

    // Unpack color from packed uint (stored as 2 floats)
    uint colorPacked = floatBitsToUint(i_data1.z);
    vec4 instanceColor;
    instanceColor.r = float((colorPacked >> 0u) & 0xFFu) / 255.0;
    instanceColor.g = float((colorPacked >> 8u) & 0xFFu) / 255.0;
    instanceColor.b = float((colorPacked >> 16u) & 0xFFu) / 255.0;
    instanceColor.a = float((colorPacked >> 24u) & 0xFFu) / 255.0;

    float instanceRandom = i_data1.w;

    // Height factor for wind animation (0 at base, 1 at tip)
    float heightFactor = a_texcoord1;

    // Apply Y rotation to vertex position
    float cosR = cos(instanceRot);
    float sinR = sin(instanceRot);
    vec3 localPos = a_position;
    localPos.x = a_position.x * cosR - a_position.z * sinR;
    localPos.z = a_position.x * sinR + a_position.z * cosR;

    // Scale the blade
    localPos.x *= u_grassParams.x;  // width
    localPos.y *= instanceScale;     // height (already scaled per instance)

    // Wind animation
    float windPhase = u_grassWind.z + instancePos.x * 0.3 + instancePos.z * 0.3 + instanceRandom * 6.28;
    float windFactor = heightFactor * heightFactor * u_grassWind.w;  // Quadratic falloff from base

    // Primary wind sway
    vec2 windOffset = u_grassWind.xy * sin(windPhase) * windFactor;

    // Secondary turbulence
    float turbulence = sin(windPhase * 2.3 + instanceRandom * 3.14) * 0.3;
    windOffset += u_grassWind.xy * turbulence * windFactor * 0.5;

    // Apply interaction bend (push grass aside)
    vec2 bendOffset = u_grassWind.xy * instanceBend * heightFactor;

    // Apply offsets to local position
    localPos.x += windOffset.x + bendOffset.x;
    localPos.z += windOffset.y + bendOffset.y;

    // Transform to world space
    vec3 worldPos = instancePos + localPos;
    v_worldPos = worldPos;

    // Transform to clip space
    gl_Position = mul(u_viewProj, vec4(worldPos, 1.0));

    // Pass through texcoords and color
    v_texcoord0 = a_texcoord0;

    // Blend base to tip color based on height
    vec3 baseColor = instanceColor.rgb;
    vec3 tipColor = baseColor * 1.3;  // Lighter at tips
    v_color = vec4(mix(baseColor, tipColor, heightFactor), instanceColor.a);

    // Distance fade
    float dist = length(u_view[3].xyz - worldPos);
    v_fade = 1.0 - smoothstep(u_grassParams.w, u_grassParams.w * 1.5, dist);
}
