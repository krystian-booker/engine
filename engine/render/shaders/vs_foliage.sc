$input a_position, a_normal, a_texcoord0, a_color0, a_tangent, i_data0, i_data1, i_data2, i_data3
$output v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_color0, v_clipPos

#include <bgfx_shader.sh>

uniform vec4 u_foliageWind;   // xy=direction, z=time, w=strength
uniform vec4 u_foliageParams; // x=unused, y=unused, z=alpha_cutoff, w=fade_start

void main()
{
    // Build instance transform matrix from row vectors
    mat4 instanceTransform = mtxFromRows(i_data0, i_data1, i_data2, i_data3);

    // Extract instance position for wind phase variation
    vec3 instancePos = vec3(i_data0.w, i_data1.w, i_data2.w);

    // Wind animation - apply to vertices above ground level
    float heightFactor = max(0.0, a_position.y);
    float windPhase = u_foliageWind.z + instancePos.x * 0.1 + instancePos.z * 0.1;
    float windFactor = heightFactor * u_foliageWind.w;

    // Gentle sway for foliage (less aggressive than grass)
    vec3 windOffset = vec3(0.0, 0.0, 0.0);
    windOffset.x = sin(windPhase) * windFactor * 0.15;
    windOffset.z = sin(windPhase * 0.7 + 1.5) * windFactor * 0.1;

    // Apply wind offset to local position
    vec3 localPos = a_position + windOffset;

    // Transform to world space using instance matrix
    vec4 worldPos = mul(instanceTransform, vec4(localPos, 1.0));

    // Calculate view-space depth for shadow cascade selection
    vec4 viewPos = mul(u_view, worldPos);
    float viewSpaceDepth = -viewPos.z;

    v_worldPos = vec4(worldPos.xyz, viewSpaceDepth);

    // Transform normal, tangent, bitangent to world space
    mat3 normalMatrix = mat3(
        instanceTransform[0].xyz,
        instanceTransform[1].xyz,
        instanceTransform[2].xyz
    );
    v_normal = normalize(mul(normalMatrix, a_normal));
    v_tangent = normalize(mul(normalMatrix, a_tangent));
    v_bitangent = cross(v_normal, v_tangent);

    // Pass through UV and vertex color
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;

    // Calculate clip-space position
    vec4 clipPos = mul(u_viewProj, worldPos);
    v_clipPos = clipPos;

    gl_Position = clipPos;
}
