$input v_normal, v_tangent, v_bitangent

#include <bgfx_shader.sh>

// We pack a mode integer into the fractional part of a color or pass it via a uniform.
// Since we only have PBR uniforms bound by default if this is loaded as a material, 
// we will repurpose u_albedoColor.x to store the mode.
// Mode 0: Normal
// Mode 1: Tangent
// Mode 2: Bitangent
uniform vec4 u_albedoColor; 

void main()
{
    int mode = int(u_albedoColor.x + 0.5);
    vec3 geom_var = v_normal;
    
    if (mode == 1) {
        geom_var = v_tangent;
    } else if (mode == 2) {
        geom_var = v_bitangent;
    }
    
    // Map [-1, 1] to [0, 1] for visualization
    vec3 color = geom_var * 0.5 + 0.5;
    
    gl_FragColor = vec4(color, 1.0);
}
