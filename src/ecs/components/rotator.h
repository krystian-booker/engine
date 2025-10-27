#pragma once

#include "core/math.h"

// Marks an entity for continuous rotation around a given axis (degrees per second).
struct Rotator {
    Vec3 axis{0.0f, 1.0f, 0.0f};
    f32 speed = 0.0f;
};
