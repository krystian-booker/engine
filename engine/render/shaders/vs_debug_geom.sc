$input a_position, a_normal, a_tangent
$output v_normal, v_tangent, v_bitangent

#include <bgfx_shader.sh>

void main()
{
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
    
    // Transform normal, tangent, bitangent to world space
    vec3 c0 = u_model[0][0].xyz;
    vec3 c1 = u_model[0][1].xyz;
    vec3 c2 = u_model[0][2].xyz;
    vec3 cofR = cross(c1, c2);
    vec3 cofU = cross(c2, c0);
    vec3 cofF = cross(c0, c1);
    
    v_normal = normalize(cofR * a_normal.x + cofU * a_normal.y + cofF * a_normal.z);
    // a_tangent is actually a vec4 representing XYZ and a sign W. We only need the XYZ.
    vec3 tangentDir = a_tangent.xyz;
    v_tangent = normalize(cofR * tangentDir.x + cofU * tangentDir.y + cofF * tangentDir.z);
    
    // Calculate bitangent properly with the sign
    v_bitangent = cross(v_normal, v_tangent) * a_tangent.w;

    vec4 clipPos = mul(u_viewProj, worldPos);
    gl_Position = clipPos;
}
