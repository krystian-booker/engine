$input v_worldPos, v_normal

#include <bgfx_shader.sh>

uniform vec4 u_cameraPos;
uniform vec4 u_lightDir;
uniform vec4 u_debugMode;

void main()
{
    vec3 N = normalize(v_normal);
    vec3 L = normalize(-u_lightDir.xyz);
    vec3 V = normalize(u_cameraPos.xyz - v_worldPos.xyz);

    int mode = int(max(0.0, u_debugMode.x + 0.1)); // robust casting
    vec3 color = vec3_splat(0.0);

    if (mode == 0) {
        color = N * 0.5 + 0.5;
    } else if (mode == 1) {
        color = vec3_splat(max(dot(N, L), 0.0));
    } else if (mode == 2) {
        color = vec3_splat(max(dot(N, V), 0.0));
    } else {
        color = vec3_splat(1.0); // Fallback color
    }

    gl_FragColor = vec4(color, 1.0);
}
