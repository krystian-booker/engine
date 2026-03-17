$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texture, 0);

// Debug view params: x = mode (1=Normals, 2=LinearDepth, 3=RawDepthCopy),
// y = near plane, z = far plane
uniform vec4 u_debugMode;

void main()
{
    int mode = int(u_debugMode.x + 0.5);
    
    if (mode == 1) {
        // Normals: Convert normals from [-1, 1] to [0, 1]
        vec4 tex_color = texture2D(s_texture, v_texcoord0);
        vec3 normal = tex_color.xyz;
        vec3 color = normal * 0.5 + 0.5;
        gl_FragColor = vec4(color, 1.0);
    } else if (mode == 2) {
        // Linear Depth visualization
        float raw_depth = texture2D(s_texture, v_texcoord0).r;
        float near = u_debugMode.y;
        float far = u_debugMode.z;

        // Linearize depth: convert [0,1] depth buffer to view-space distance.
        // Formula: nf / (f - d*(f-n)), equivalent to the standard ndc-based form.
        float linear_depth = (near * far) / (far - raw_depth * (far - near));

        // Normalize to [0,1] where 0 = near plane, 1 = far plane
        float t = clamp((linear_depth - near) / (far - near), 0.0, 1.0);

        // Apply power curve (t^0.25) to spread near-field values across visible
        // brightness. Without this, objects appear as black silhouettes because
        // depth values are heavily packed toward the far plane (e.g. an object at
        // z=5 with far=100 maps to only 5% brightness).
        float vis = sqrt(sqrt(t));
        gl_FragColor = vec4(vec3_splat(vis), 1.0);
    } else if (mode == 3) {
        // Raw depth copy for passes that need a sampleable color texture version
        // of the opaque depth buffer.
        float raw_depth = texture2D(s_texture, v_texcoord0).r;
        gl_FragColor = vec4(raw_depth, 0.0, 0.0, 1.0);
    } else {
        // Fallback
        gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
    }
}
