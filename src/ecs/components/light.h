#pragma once
#include "core/types.h"
#include "core/math.h"
#include "renderer/uniform_buffers.h"  // For ShadowFilterMode

enum class LightType {
    Directional,
    Point,
    Spot,
    Area,
    Tube,
    Hemisphere
};

struct Light {
    LightType type = LightType::Directional;

    // Color and intensity
    Vec3 color{1.0f, 1.0f, 1.0f};
    f32 intensity = 1.0f;

    // Point/Spot light parameters
    f32 range = 10.0f;
    f32 attenuation = 1.0f;

    // Spot light parameters
    f32 innerConeAngle = 30.0f;  // Degrees
    f32 outerConeAngle = 45.0f;  // Degrees

    // Area light parameters
    f32 width = 1.0f;            // Width of rectangular area light
    f32 height = 1.0f;           // Height of rectangular area light
    bool twoSided = false;       // Emit light from both sides

    // Tube light parameters
    f32 tubeLength = 1.0f;       // Length of tube/line light
    f32 tubeRadius = 0.1f;       // Radius of tube light

    // Hemisphere light parameters
    Vec3 skyColor{0.4f, 0.6f, 1.0f};      // Sky/upper hemisphere color
    Vec3 groundColor{0.1f, 0.1f, 0.1f};   // Ground/lower hemisphere color

    // Shadow casting
    bool castsShadows = true;

    // Shadow filtering configuration
    ShadowFilterMode shadowFilterMode = ShadowFilterMode::PCF;  // Default to basic PCF
    f32 shadowSearchRadius = 5.0f;                               // Search radius for PCSS or max radius for ContactHardening

    // EVSM-specific parameters (only used if shadowFilterMode == EVSM)
    f32 evsmPositiveExponent = 40.0f;
    f32 evsmNegativeExponent = 40.0f;
    f32 evsmLightBleedReduction = 0.3f;

    // Computed values (updated by lighting system)
    Vec3 direction{0, -1, 0};    // For directional/spot
};
