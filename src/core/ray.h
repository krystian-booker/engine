#pragma once

#include "core/types.h"
#include "core/math.h"

/**
 * Ray
 *
 * Represents a ray with an origin and direction.
 * Used for raycasting and picking operations.
 */
struct Ray {
    Vec3 origin;
    Vec3 direction;  // Should be normalized

    Ray() : origin(0, 0, 0), direction(0, 0, -1) {}

    Ray(const Vec3& origin, const Vec3& direction)
        : origin(origin), direction(Normalize(direction)) {}

    // Get a point along the ray at the given distance
    Vec3 GetPoint(f32 distance) const {
        return origin + direction * distance;
    }
};

/**
 * Create a ray from screen coordinates
 *
 * @param screenPos Screen position (0,0 = top-left)
 * @param screenSize Screen dimensions
 * @param viewMatrix Camera view matrix
 * @param projMatrix Camera projection matrix
 * @return Ray in world space
 */
inline Ray ScreenPointToRay(const Vec2& screenPos, const Vec2& screenSize,
                           const Mat4& viewMatrix, const Mat4& projMatrix) {
    // Normalize screen coordinates to NDC [-1, 1]
    f32 x = (2.0f * screenPos.x) / screenSize.x - 1.0f;
    f32 y = 1.0f - (2.0f * screenPos.y) / screenSize.y;  // Flip Y (screen Y goes down)

    // NDC to clip space
    Vec4 rayClip(x, y, -1.0f, 1.0f);

    // Clip to eye space
    Mat4 invProj = Inverse(projMatrix);
    Vec4 rayEye = invProj * rayClip;
    rayEye = Vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);  // Direction vector (w=0)

    // Eye to world space
    Mat4 invView = Inverse(viewMatrix);
    Vec4 rayWorld = invView * rayEye;
    Vec3 direction = Normalize(Vec3(rayWorld.x, rayWorld.y, rayWorld.z));

    // Camera position from view matrix
    Vec3 cameraPos = Vec3(invView[3][0], invView[3][1], invView[3][2]);

    return Ray(cameraPos, direction);
}
