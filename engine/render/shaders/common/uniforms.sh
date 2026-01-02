// Common uniform declarations for PBR rendering
#ifndef UNIFORMS_SH
#define UNIFORMS_SH

// Camera uniforms
uniform vec4 u_cameraPos;      // xyz = camera position, w = unused

// Material uniforms
uniform vec4 u_albedoColor;    // xyz = base color, w = alpha
uniform vec4 u_pbrParams;      // x = metallic, y = roughness, z = ao, w = alpha cutoff
uniform vec4 u_emissiveColor;  // xyz = emissive color, w = emissive intensity

// Light uniforms (up to 8 lights)
// Each light uses 4 vec4s:
// [0] = position.xyz, type (0=dir, 1=point, 2=spot)
// [1] = direction.xyz, range
// [2] = color.xyz, intensity
// [3] = spotParams.x = inner angle, y = outer angle, z = shadow index, w = unused
uniform vec4 u_lights[32];     // 8 lights * 4 vec4s each
uniform vec4 u_lightCount;     // x = active light count

// Shadow uniforms are declared in shadow.sh (u_shadowParams, u_cascadeSplits, u_shadowMatrix0-3)

// IBL uniforms
uniform vec4 u_iblParams;      // x = intensity, y = rotation, z = max mip level, w = unused

// Time and misc uniforms
uniform vec4 u_time;           // x = total time, y = delta time, z = sin(time), w = cos(time)

#endif // UNIFORMS_SH
