#pragma once

#include "core/types.h"
#include "core/math.h"

// Push constants shared between vertex and fragment shaders
struct PushConstants {
    Mat4 model;           // 64 bytes
    u32 materialIndex;    // 4 bytes
    u32 padding[2];       // 8 bytes padding to 16-byte alignment (total 76 bytes)
};

static_assert(sizeof(PushConstants) == 76, "PushConstants must be 76 bytes");
