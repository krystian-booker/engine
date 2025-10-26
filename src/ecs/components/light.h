#pragma once
#include "core/types.h"
#include "core/math.h"

enum class LightType {
    Directional,
    Point,
    Spot
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

    // Shadow casting
    bool castsShadows = true;

    // Computed values (updated by lighting system)
    Vec3 direction{0, -1, 0};    // For directional/spot
};
