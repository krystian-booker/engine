#pragma once

#include "core/types.h"
#include "core/math.h"

// Push constants shared between vertex and fragment shaders
struct PushConstants {
    Mat4 model;           // 64 bytes
    u32 materialIndex;    // 4 bytes
    u32 screenWidth;      // 4 bytes - For Forward+ tile calculation
    u32 screenHeight;     // 4 bytes - For Forward+ tile calculation
    u32 tileSize;         // 4 bytes - Forward+ tile size (typically 16)
};

static_assert(sizeof(PushConstants) == 80, "PushConstants must be 80 bytes");
