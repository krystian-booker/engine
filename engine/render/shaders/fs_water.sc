$input v_worldPos, v_screenPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_texcoord1, v_texcoord2, v_foamUV

#include <bgfx_shader.sh>

// Samplers
SAMPLER2D(s_normalMap1, 0);
SAMPLER2D(s_normalMap2, 1);
SAMPLER2D(s_foamTexture, 2);
SAMPLER2D(s_reflectionTexture, 3);
SAMPLER2D(s_refractionTexture, 4);
SAMPLER2D(s_depthTexture, 5);
SAMPLER2D(s_causticsTexture, 6);

// Uniforms
uniform vec4 u_waterShallowColor;    // xyz = color, w = opacity
uniform vec4 u_waterDeepColor;       // xyz = color, w = depth_fade_distance
uniform vec4 u_waterFresnelParams;   // x = power, y = bias, z = reflection_strength, w = refraction_strength
uniform vec4 u_waterSpecularParams;  // x = power, y = intensity, z = foam_threshold, w = shore_foam_width
uniform vec4 u_waterCameraPos;       // xyz = camera position, w = unused
uniform vec4 u_waterSunDir;          // xyz = sun direction, w = unused
uniform vec4 u_waterSunColor;        // xyz = sun color, w = intensity

// Helper functions
vec3 blendNormals(vec3 n1, vec3 n2)
{
    return normalize(vec3(n1.xy + n2.xy, n1.z * n2.z));
}

float linearizeDepth(float depth, float near, float far)
{
    return near * far / (far - depth * (far - near));
}

void main()
{
    // Sample and blend normal maps
    vec3 normal1 = texture2D(s_normalMap1, v_texcoord1).xyz * 2.0 - 1.0;
    vec3 normal2 = texture2D(s_normalMap2, v_texcoord2).xyz * 2.0 - 1.0;
    vec3 blendedNormal = blendNormals(normal1, normal2);

    // Transform normal to world space using TBN matrix
    mat3 TBN = mat3(v_tangent, v_bitangent, v_normal);
    vec3 worldNormal = normalize(mul(TBN, blendedNormal));

    // View direction
    vec3 viewDir = normalize(u_waterCameraPos.xyz - v_worldPos.xyz);

    // Fresnel effect
    float fresnelPower = u_waterFresnelParams.x;
    float fresnelBias = u_waterFresnelParams.y;
    float fresnel = fresnelBias + (1.0 - fresnelBias) * pow(1.0 - max(dot(viewDir, worldNormal), 0.0), fresnelPower);

    // Sample scene depth for depth fade
    vec2 screenUV = v_screenPos.xy;
    float sceneDepth = texture2D(s_depthTexture, screenUV).r;
    float waterDepth = v_worldPos.w;
    float depthDifference = sceneDepth - waterDepth;
    float depthFade = saturate(depthDifference / u_waterDeepColor.w);

    // Blend shallow and deep water colors based on depth
    vec3 waterColor = mix(u_waterShallowColor.xyz, u_waterDeepColor.xyz, depthFade);

    // Sample reflection with distortion
    float reflectionStrength = u_waterFresnelParams.z;
    vec2 reflectionUV = screenUV + worldNormal.xz * 0.03;
    vec3 reflection = texture2D(s_reflectionTexture, reflectionUV).rgb;

    // Sample refraction with distortion
    float refractionStrength = u_waterFresnelParams.w;
    vec2 refractionUV = screenUV + worldNormal.xz * 0.02;
    vec3 refraction = texture2D(s_refractionTexture, refractionUV).rgb;

    // Combine reflection and refraction using fresnel
    vec3 finalColor = mix(refraction * waterColor, reflection, fresnel * reflectionStrength);

    // Specular highlight (sun reflection)
    vec3 halfDir = normalize(viewDir + u_waterSunDir.xyz);
    float specAngle = max(dot(worldNormal, halfDir), 0.0);
    float specPower = u_waterSpecularParams.x;
    float specIntensity = u_waterSpecularParams.y;
    float specular = pow(specAngle, specPower) * specIntensity;
    finalColor += u_waterSunColor.xyz * specular * u_waterSunColor.w;

    // Shore foam (where water meets geometry)
    float foamThreshold = u_waterSpecularParams.z;
    float shoreFoamWidth = u_waterSpecularParams.w;
    float shoreFoam = 1.0 - saturate(depthDifference / shoreFoamWidth);
    vec4 foamSample = texture2D(s_foamTexture, v_foamUV.xy);
    float foamAmount = shoreFoam * foamSample.r;

    // Wave crest foam (based on wave height - could be passed from vertex shader)
    // For now, use normal Y component as proxy
    float waveFoam = saturate((worldNormal.y - foamThreshold) / (1.0 - foamThreshold));
    foamAmount = max(foamAmount, waveFoam * foamSample.g);

    // Add foam to final color
    finalColor = mix(finalColor, vec3(1.0, 1.0, 1.0), foamAmount * 0.8);

    // Caustics (optional - would be projected onto underwater surfaces)
    // This is simplified - full implementation would project onto scene geometry
    vec3 caustics = texture2D(s_causticsTexture, v_foamUV.zw).rgb;
    finalColor += caustics * 0.1 * (1.0 - depthFade);

    // Output with water opacity
    float opacity = u_waterShallowColor.w;
    opacity = mix(opacity, 1.0, fresnel * 0.3);  // More opaque at grazing angles

    gl_FragColor = vec4(finalColor, opacity);
}
