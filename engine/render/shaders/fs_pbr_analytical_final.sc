$input v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_color0, v_clipPos

#include <bgfx_shader.sh>
#include "common/uniforms.sh"
#include "common/light_data.sh"

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    vec3 albedo    = u_albedoColor.xyz;
    float metallic = u_pbrParams.x;
    float roughness = max(u_pbrParams.y, 0.05);
    int mode = int(u_pbrParams.w);

    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_cameraPos.xyz - v_worldPos.xyz);
    Light mainLight = getLight(0);
    vec3 L = normalize(-mainLight.direction);
    vec3 H = normalize(V + L);

    vec3 F0 = vec3_splat(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 lightColor = mainLight.color * mainLight.intensity;
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 nominator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3 specular = nominator / max(denominator, 0.001);
    
    // Energy conservation
    vec3 kS = F;
    vec3 kD = vec3_splat(1.0) - kS;
    kD *= 1.0 - metallic;   
    
    float NdotL = max(dot(N, L), 0.0);        
    vec3 diffuse = kD * albedo / PI;
    
    // Minimal ambient to not be completely black in shadows, 
    // unless in modes 1, 2, 3 where we want strict evaluation
    vec3 ambient = vec3_splat(0.03) * albedo * u_pbrParams.z; 
    
    vec3 Lo = (diffuse + specular) * lightColor * NdotL;
    vec3 color = ambient + Lo;

    if (mode == 1) {
        // Specular only
        color = specular * lightColor * NdotL;
    } else if (mode == 2) {
        // Diffuse only
        color = diffuse * lightColor * NdotL;
    } else if (mode == 3) {
        // Fresnel only
        color = F;
    }

    // HDR to SDR Tonemapping (Reinhard)
    color = color / (color + vec3_splat(1.0));
    
    // Gamma correction
    color = max(color, vec3_splat(0.0));
    color = pow(color, vec3_splat(1.0/2.2)); 

    gl_FragColor = vec4(color, 1.0);
}
