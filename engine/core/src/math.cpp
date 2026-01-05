#include <engine/core/math.hpp>
#include <cmath>

namespace engine::core {

void Frustum::extract_from_matrix(const Mat4& vp) {
    // Gribb/Hartmann method to extract frustum planes from view-projection matrix
    const float* m = glm::value_ptr(vp);
    
    // Left
    planes[0].x = m[3] + m[0];
    planes[0].y = m[7] + m[4];
    planes[0].z = m[11] + m[8];
    planes[0].w = m[15] + m[12];
    
    // Right
    planes[1].x = m[3] - m[0];
    planes[1].y = m[7] - m[4];
    planes[1].z = m[11] - m[8];
    planes[1].w = m[15] - m[12];
    
    // Bottom
    planes[2].x = m[3] + m[1];
    planes[2].y = m[7] + m[5];
    planes[2].z = m[11] + m[9];
    planes[2].w = m[15] + m[13];
    
    // Top
    planes[3].x = m[3] - m[1];
    planes[3].y = m[7] - m[5];
    planes[3].z = m[11] - m[9];
    planes[3].w = m[15] - m[13];
    
    // Near
    planes[4].x = m[3] + m[2];
    planes[4].y = m[7] + m[6];
    planes[4].z = m[11] + m[10];
    planes[4].w = m[15] + m[14];
    
    // Far
    planes[5].x = m[3] - m[2];
    planes[5].y = m[7] - m[6];
    planes[5].z = m[11] - m[10];
    planes[5].w = m[15] - m[14];

    // Normalize planes
    for (int i = 0; i < 6; ++i) {
        float len = std::sqrt(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
        if (len > 0.0f) {
            planes[i] /= len;
        }
    }
}

bool Frustum::contains_point(const Vec3& point) const {
    for (int i = 0; i < 6; ++i) {
        if (planes[i].x * point.x + planes[i].y * point.y + planes[i].z * point.z + planes[i].w < 0.0f) {
            return false;
        }
    }
    return true;
}

bool Frustum::contains_aabb(const AABB& aabb) const {
    // Check if AABB is outside any plane
    for (int i = 0; i < 6; ++i) {
        // Find the "positive vertex" (vertex most in direction of normal)
        Vec3 p = aabb.min;
        if (planes[i].x >= 0) p.x = aabb.max.x;
        if (planes[i].y >= 0) p.y = aabb.max.y;
        if (planes[i].z >= 0) p.z = aabb.max.z;

        // If even the positive vertex is behind the plane, then the box is outside
        if (planes[i].x * p.x + planes[i].y * p.y + planes[i].z * p.z + planes[i].w < 0.0f) {
            return false;
        }
    }
    return true;
}

} // namespace engine::core
