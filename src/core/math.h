#pragma once
#include "types.h"

struct Vec3 {
    f32 x, y, z;
    
    Vec3() : x(0), y(0), z(0) {}
    Vec3(f32 _x, f32 _y, f32 _z) : x(_x), y(_y), z(_z) {}
    
    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }
};

inline f32 Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}