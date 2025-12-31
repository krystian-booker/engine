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
    // Using the upper 3x3 of the model matrix (assumes uniform scale)
    mat3 normalMatrix = mat3(u_model[0]);
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
