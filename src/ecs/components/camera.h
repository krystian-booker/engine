#pragma once
#include "core/types.h"
#include "core/math.h"

enum class CameraProjection {
    Perspective,
    Orthographic
};

struct Camera {
    CameraProjection projection = CameraProjection::Perspective;

    // Perspective parameters
    f32 fov = 60.0f;              // Field of view in degrees
    f32 aspectRatio = 16.0f/9.0f;

    // Orthographic parameters
    f32 orthoSize = 10.0f;        // Height of orthographic view

    // Common parameters
    f32 nearPlane = 0.1f;
    f32 farPlane = 1000.0f;

    // Clear color
    Vec4 clearColor{0.1f, 0.1f, 0.1f, 1.0f};

    // Is this the active camera?
    bool isActive = false;
};
