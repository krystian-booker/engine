#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace engine::core {

// Vector types
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

// Integer vector types
using IVec2 = glm::ivec2;
using IVec3 = glm::ivec3;
using IVec4 = glm::ivec4;

// Unsigned integer vector types
using UVec2 = glm::uvec2;
using UVec3 = glm::uvec3;
using UVec4 = glm::uvec4;

// Matrix types
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;

// Quaternion
using Quat = glm::quat;

// Axis-aligned bounding box
struct AABB {
    Vec3 min{0.0f};
    Vec3 max{0.0f};

    AABB() = default;
    AABB(const Vec3& min_, const Vec3& max_) : min(min_), max(max_) {}

    Vec3 center() const { return (min + max) * 0.5f; }
    Vec3 size() const { return max - min; }
    Vec3 extents() const { return size() * 0.5f; }

    bool contains(const Vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    bool intersects(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    void expand(const Vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    void expand(const AABB& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }
};

// Ray for raycasting
struct Ray {
    Vec3 origin{0.0f};
    Vec3 direction{0.0f, 0.0f, -1.0f};

    Ray() = default;
    Ray(const Vec3& o, const Vec3& d) : origin(o), direction(glm::normalize(d)) {}

    Vec3 at(float t) const { return origin + direction * t; }
};

// Frustum for culling
struct Frustum {
    // Planes: left, right, bottom, top, near, far
    Vec4 planes[6];

    void extract_from_matrix(const Mat4& vp);
    bool contains_point(const Vec3& point) const;
    bool contains_aabb(const AABB& aabb) const;
};

} // namespace engine::core
