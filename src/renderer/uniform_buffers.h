#pragma once

#include "core/math.h"
#include "core/types.h"

// View/Projection uniform buffer (Set 0, Binding 0)
struct UniformBufferObject {
    Mat4 view;
    Mat4 projection;
};

// GPU representation of a light (std140 layout)
// Type encoding: 0=Directional, 1=Point, 2=Spot, 3=Area, 4=Tube, 5=Hemisphere
struct GPULight {
    Vec4 positionAndType;    // xyz = position/direction, w = type
    Vec4 colorAndIntensity;  // rgb = color, w = intensity
    Vec4 directionAndRange;  // xyz = direction (for spot/directional/area/tube), w = range
    Vec4 spotAngles;         // x = inner cone cos, y = outer cone cos, z = castsShadows, w = shadowMapIndex

    // Extended parameters for area/tube/hemisphere lights
    Vec4 areaParams;         // x = width, y = height, z = twoSided (0/1), w = unused
    Vec4 tubeParams;         // x = length, y = radius, z/w = unused
    Vec4 hemisphereParams;   // xyz = skyColor or right vector (area), w = unused
    Vec4 hemisphereParams2;  // xyz = groundColor or up vector (area), w = unused
};

// Cascade split distances and shadow matrices
static constexpr u32 kMaxCascades = 4;

struct ShadowUniforms {
    Mat4 cascadeViewProj[kMaxCascades];  // View-projection matrix for each cascade
    Vec4 cascadeSplits;                   // xyz = cascade split distances, w = numCascades
    Vec4 shadowParams;                    // x = shadow bias, y = PCF radius, z/w = padding
};

// Maximum lights supported per frame
static constexpr u32 kMaxLights = 16;

// Lighting uniform buffer (Set 0, Binding 1)
struct LightingUniformBuffer {
    Vec4 cameraPosition;          // xyz = camera position, w = padding
    u32 numLights;                 // Active light count
    u32 padding1;
    u32 padding2;
    u32 padding3;
    GPULight lights[kMaxLights];  // Array of lights
};
