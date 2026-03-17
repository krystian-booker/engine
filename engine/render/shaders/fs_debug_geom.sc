$input v_normal, v_tangent, v_bitangent, v_texcoord0

#include <bgfx_shader.sh>
#include "common/uniforms.sh"

SAMPLER2D(s_metallicRoughness, 2);

void main()
{
    vec3 worldNormal = normalize(v_normal);
    vec3 packedNormal = worldNormal * 0.5 + 0.5;
    vec4 mrSample = texture2D(s_metallicRoughness, v_texcoord0);
    float roughness = max(mrSample.g * u_pbrParams.y, 0.04);
    gl_FragColor = vec4(packedNormal, roughness);
}
