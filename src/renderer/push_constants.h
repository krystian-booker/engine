#pragma once

#include "core/types.h"
#include "core/math.h"

// Push constants shared between vertex and fragment shaders
struct PushConstants {
    Mat4 model;           // 64 bytes
    u32 materialIndex;    // 4 bytes
    u32 padding[1];       // 4 bytes padding (total 72 bytes)
};

static_assert(sizeof(PushConstants) == 72, "PushConstants must be 72 bytes");
