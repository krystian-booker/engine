#pragma once

#include "types.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

// Type aliases for GLM types
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

using Mat3 = glm::mat3;
using Mat4 = glm::mat4;

using Quat = glm::quat;

// Vector operations
inline f32 Dot(const Vec3& a, const Vec3& b) {
    return glm::dot(a, b);
}

inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return glm::cross(a, b);
}

inline f32 Length(const Vec3& v) {
    return glm::length(v);
}

inline Vec3 Normalize(const Vec3& v) {
    return glm::normalize(v);
}

// Matrix transformations
inline Mat4 Translate(const Mat4& m, const Vec3& v) {
    return glm::translate(m, v);
}

inline Mat4 Rotate(const Mat4& m, f32 angle, const Vec3& axis) {
    return glm::rotate(m, angle, axis);
}

inline Mat4 Scale(const Mat4& m, const Vec3& v) {
    return glm::scale(m, v);
}

// Projection matrices
inline Mat4 Perspective(f32 fovy, f32 aspect, f32 near, f32 far) {
    return glm::perspective(fovy, aspect, near, far);
}

inline Mat4 Ortho(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
    return glm::ortho(left, right, bottom, top, near, far);
}

// View matrix
inline Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    return glm::lookAt(eye, center, up);
}

// Quaternion operations
inline Quat QuatFromAxisAngle(const Vec3& axis, f32 angle) {
    return glm::angleAxis(angle, axis);
}

inline Mat4 QuatToMat4(const Quat& q) {
    return glm::mat4_cast(q);
}

// Utility functions
inline f32 Radians(f32 degrees) {
    return glm::radians(degrees);
}

inline f32 Degrees(f32 radians) {
    return glm::degrees(radians);
}
