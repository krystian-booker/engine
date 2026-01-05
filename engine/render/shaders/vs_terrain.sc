$input a_position, a_normal, a_texcoord0, a_tangent
$output v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_tileUV, v_clipPos

#include <bgfx_shader.sh>

// Terrain uniforms
uniform vec4 u_terrainParams;  // x: tile scale, y: unused, z: unused, w: unused

void main()
{
    // Transform to world space
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));

    // Calculate view-space position for depth
    vec4 viewPos = mul(u_view, worldPos);
    float viewSpaceDepth = -viewPos.z;

    v_worldPos = vec4(worldPos.xyz, viewSpaceDepth);

    // Transform normal, tangent, bitangent to world space
    mat3 normalMatrix = mat3(u_model[0][0].xyz, u_model[0][1].xyz, u_model[0][2].xyz);
    v_normal = normalize(mul(normalMatrix, a_normal));
    v_tangent = normalize(mul(normalMatrix, a_tangent.xyz));
    v_bitangent = cross(v_normal, v_tangent) * a_tangent.w;

    // Global terrain UV (0-1 range across terrain)
    v_texcoord0 = a_texcoord0;

    // Tiled UV for detail textures
    float tileScale = u_terrainParams.x;
    v_tileUV = worldPos.xz * tileScale;

    // Calculate clip-space position
    vec4 clipPos = mul(u_viewProj, worldPos);
    v_clipPos = clipPos;

    gl_Position = clipPos;
}
