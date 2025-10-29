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
    f32 fov = 60.0f;
    f32 aspectRatio = 16.0f / 9.0f;
    f32 nearPlane = 0.1f;
    f32 farPlane = 1000.0f;

    // Orthographic parameters
    f32 orthoSize = 10.0f;

    // Clear color
    Vec4 clearColor{0.15f, 0.15f, 0.15f, 1.0f};

    // Is this the active camera?
    bool isActive = false;

    // Is this an editor camera? (not serialized, for scene view only)
    bool isEditorCamera = false;

    // Computed matrices (updated by camera system)
    Mat4 viewMatrix{1.0f};
    Mat4 projectionMatrix{1.0f};
};
