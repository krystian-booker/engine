#pragma once
#include "core/math.h"
#include "ecs/entity.h"

struct Transform {
    // Local transform (relative to parent)
    Vec3 localPosition{0, 0, 0};
    Quat localRotation{1, 0, 0, 0};  // Identity quaternion
    Vec3 localScale{1, 1, 1};

    // World transform (computed)
    Mat4 worldMatrix{1.0f};

    // Hierarchy
    Entity parent = Entity::Invalid;
    // Note: Children will be stored separately in a hierarchy manager

    // Dirty flag for optimization
    bool isDirty = true;

    // Helper to mark dirty
    // Note: Dirty propagation to children is handled by TransformSystem
    void MarkDirty() {
        isDirty = true;
    }

    // Compute local matrix
    Mat4 GetLocalMatrix() const {
        Mat4 translation = Translate(Mat4(1.0f), localPosition);
        Mat4 rotation = QuatToMat4(localRotation);
        Mat4 scale = Scale(Mat4(1.0f), localScale);
        return translation * rotation * scale;
    }
};
