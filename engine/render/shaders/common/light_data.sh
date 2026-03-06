#ifndef LIGHT_DATA_SH
#define LIGHT_DATA_SH

#include "uniforms.sh"

struct Light
{
    vec3 position;
    float type;
    vec3 direction;
    float range;
    vec3 color;
    float intensity;
    float innerAngle;
    float outerAngle;
    int shadowIndex;
};

Light getLight(int index)
{
    Light light;
    int base = index * 4;
    vec4 data0 = u_lights[base + 0];
    vec4 data1 = u_lights[base + 1];
    vec4 data2 = u_lights[base + 2];
    vec4 data3 = u_lights[base + 3];
    light.position = data0.xyz;
    light.type = data0.w;
    light.direction = data1.xyz;
    light.range = data1.w;
    light.color = data2.xyz;
    light.intensity = data2.w;
    light.innerAngle = data3.x;
    light.outerAngle = data3.y;
    light.shadowIndex = int(data3.z);
    return light;
}

#endif // LIGHT_DATA_SH
