$input a_position, a_normal, a_texcoord0, a_color0, a_tangent
$output v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_color0, v_clipPos

#include <bgfx_shader.sh>

void main()
{
    // Transform to world space
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));

    // Linear view-space depth for cascade shadow selection.
    // Negated because RH view space has -Z forward; cascade splits (u_cascadeSplits)
    // are computed from camera near/far along the view axis on the C++ side.
    // Do NOT use length(viewPos.xyz) — splits are calibrated for linear Z, not radial distance.
    vec4 viewPos = mul(u_view, worldPos);
    float viewSpaceDepth = -viewPos.z;

    v_worldPos = vec4(worldPos.xyz, viewSpaceDepth);

    // Normal matrix (inverse-transpose of model) precomputed on CPU, stored in u_model[1]
    v_normal = normalize(mul(u_model[1], vec4(a_normal, 0.0)).xyz);
    v_tangent = normalize(mul(u_model[1], vec4(a_tangent.xyz, 0.0)).xyz);
    v_bitangent = cross(v_normal, v_tangent);

    // Pass through UV and vertex color
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;

    // Calculate clip-space position
    vec4 clipPos = mul(u_viewProj, worldPos);
    v_clipPos = clipPos;

    gl_Position = clipPos;
}
