#pragma once

#include "core/types.h"
#include "core/math.h"
#include "core/ray.h"
#include <limits>
#include <algorithm>

/**
 * AABB (Axis-Aligned Bounding Box)
 *
 * Represents an axis-aligned bounding box for collision detection,
 * raycasting, and frustum culling.
 */
struct AABB {
    Vec3 min;
    Vec3 max;

    // Default constructor - invalid bounds
    AABB()
        : min(Vec3(std::numeric_limits<f32>::max()))
        , max(Vec3(std::numeric_limits<f32>::lowest())) {}

    // Construct from min/max points
    AABB(const Vec3& min, const Vec3& max)
        : min(min), max(max) {}

    // Construct from center and extents
    static AABB FromCenterExtents(const Vec3& center, const Vec3& extents) {
        return AABB(center - extents, center + extents);
    }

    // Get center point
    Vec3 GetCenter() const {
        return (min + max) * 0.5f;
    }

    // Get extents (half-size)
    Vec3 GetExtents() const {
        return (max - min) * 0.5f;
    }

    // Get size
    Vec3 GetSize() const {
        return max - min;
    }

    // Expand to include a point
    void Expand(const Vec3& point) {
        min = Min(min, point);
        max = Max(max, point);
    }

    // Expand to include another AABB
    void Expand(const AABB& other) {
        min = Min(min, other.min);
        max = Max(max, other.max);
    }

    // Check if bounds are valid
    bool IsValid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    // Check if point is inside bounds
    bool Contains(const Vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    // Ray intersection test
    // Returns true if ray intersects, and sets tMin/tMax to entry/exit distances
    bool IntersectsRay(const Ray& ray, f32& tMin, f32& tMax) const {
        Vec3 invDir = Vec3(
            1.0f / ray.direction.x,
            1.0f / ray.direction.y,
            1.0f / ray.direction.z
        );

        Vec3 t0s = (min - ray.origin) * invDir;
        Vec3 t1s = (max - ray.origin) * invDir;

        Vec3 tsmaller = Min(t0s, t1s);
        Vec3 tbigger = Max(t0s, t1s);

        tMin = std::max(tsmaller.x, std::max(tsmaller.y, tsmaller.z));
        tMax = std::min(tbigger.x, std::min(tbigger.y, tbigger.z));

        return tMin <= tMax && tMax >= 0.0f;
    }

    // Simple ray intersection test (returns true/false only)
    bool IntersectsRay(const Ray& ray) const {
        f32 tMin, tMax;
        return IntersectsRay(ray, tMin, tMax);
    }

    // Transform bounds by matrix
    AABB Transform(const Mat4& matrix) const {
        // Transform center
        Vec4 centerHom = Vec4(GetCenter(), 1.0f);
        Vec4 transformedCenter = matrix * centerHom;
        Vec3 newCenter = Vec3(transformedCenter.x, transformedCenter.y, transformedCenter.z);

        // Extract scale from matrix to transform extents
        Vec3 scale = Vec3(
            Length(Vec3(matrix[0][0], matrix[0][1], matrix[0][2])),
            Length(Vec3(matrix[1][0], matrix[1][1], matrix[1][2])),
            Length(Vec3(matrix[2][0], matrix[2][1], matrix[2][2]))
        );

        Vec3 extents = GetExtents();
        Vec3 newExtents = extents * scale;

        return AABB::FromCenterExtents(newCenter, newExtents);
    }
};

// Min/Max helper functions for AABB
inline Vec3 Min(const Vec3& a, const Vec3& b) {
    return Vec3(
        std::min(a.x, b.x),
        std::min(a.y, b.y),
        std::min(a.z, b.z)
    );
}

inline Vec3 Max(const Vec3& a, const Vec3& b) {
    return Vec3(
        std::max(a.x, b.x),
        std::max(a.y, b.y),
        std::max(a.z, b.z)
    );
}
