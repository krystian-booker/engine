$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texture, 0);

// Debug view params: x = mode (1=Normals, 2=LinearDepth), y = near plane, z = far plane
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
        // Linear Depth
        float raw_depth = texture2D(s_texture, v_texcoord0).r;
        float near = u_debugMode.y;
        float far = u_debugMode.z;
        
        // Convert non-linear depth to linear [0, 1]
        float ndc = raw_depth * 2.0 - 1.0;
        float linear_depth = (2.0 * near * far) / (far + near - ndc * (far - near));
        
        // Normalize linear depth relative to far plane for visualization
        float normalized_depth = linear_depth / far;
        gl_FragColor = vec4(vec3_splat(normalized_depth), 1.0);
    } else {
        // Fallback
        gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
    }
}
