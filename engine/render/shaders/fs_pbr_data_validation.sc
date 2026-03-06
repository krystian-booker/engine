$input v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_color0, v_clipPos

#include <bgfx_shader.sh>
#include "common/uniforms.sh"
#include "common/light_data.sh"

void main()
{
    // Compute all values before branching
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_cameraPos.xyz - v_worldPos.xyz);
    Light mainLight = getLight(0);
    vec3 L = normalize(-mainLight.direction);

    int mode = int(u_pbrParams.w);
    vec3 color = vec3_splat(0.0);

    if (mode == 0) {
        // Normals (rainbow sphere)
        color = N * 0.5 + 0.5;
    } else if (mode == 1) {
        // N dot L (lit hemisphere)
        color = vec3_splat(max(dot(N, L), 0.0));
    } else if (mode == 2) {
        // N dot V (bright center, dark silhouette edges)
        color = vec3_splat(max(dot(N, V), 0.0));
    } else {
        color = vec3_splat(1.0);
    }

    gl_FragColor = vec4(color, 1.0);
}
