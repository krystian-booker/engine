$input a_position, a_normal, a_texcoord0, a_color0, a_tangent
$output v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_color0, v_clipPos

#include <bgfx_shader.sh>

void main()
{
    // Transform to world space
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));

    // Calculate view-space position for depth (used for cascade selection)
    vec4 viewPos = mul(u_view, worldPos);
    float viewSpaceDepth = -viewPos.z;  // Negate because view space is negative Z forward

    v_worldPos = vec4(worldPos.xyz, viewSpaceDepth);

    // Transform normal, tangent, bitangent to world space
    // Use cofactor matrix (== inverse-transpose * det) to handle non-uniform scale.
    // Since we normalize afterwards, skipping the determinant division is fine.
    vec3 c0 = u_model[0][0].xyz;
    vec3 c1 = u_model[0][1].xyz;
    vec3 c2 = u_model[0][2].xyz;
    mat3 normalMatrix = mat3(
        cross(c1, c2),
        cross(c2, c0),
        cross(c0, c1)
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
