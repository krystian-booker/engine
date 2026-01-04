#include <engine/cinematic/track.hpp>
#include <cmath>
#include <algorithm>

namespace engine::cinematic {

Track::Track(const std::string& name, TrackType type)
    : m_name(name)
    , m_type(type) {
}

float apply_easing(float t, EaseType type) {
    // Clamp t to [0, 1]
    t = std::clamp(t, 0.0f, 1.0f);

    switch (type) {
        case EaseType::Linear:
            return t;

        case EaseType::EaseIn:
            return t * t;

        case EaseType::EaseOut:
            return 1.0f - (1.0f - t) * (1.0f - t);

        case EaseType::EaseInOut:
            return t < 0.5f
                ? 2.0f * t * t
                : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;

        case EaseType::EaseInQuad:
            return t * t;

        case EaseType::EaseOutQuad:
            return 1.0f - (1.0f - t) * (1.0f - t);

        case EaseType::EaseInOutQuad:
            return t < 0.5f
                ? 2.0f * t * t
                : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;

        case EaseType::EaseInCubic:
            return t * t * t;

        case EaseType::EaseOutCubic:
            return 1.0f - std::pow(1.0f - t, 3.0f);

        case EaseType::EaseInOutCubic:
            return t < 0.5f
                ? 4.0f * t * t * t
                : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;

        case EaseType::EaseInElastic: {
            if (t == 0.0f || t == 1.0f) return t;
            constexpr float c4 = (2.0f * 3.14159265f) / 3.0f;
            return -std::pow(2.0f, 10.0f * t - 10.0f) *
                   std::sin((t * 10.0f - 10.75f) * c4);
        }

        case EaseType::EaseOutElastic: {
            if (t == 0.0f || t == 1.0f) return t;
            constexpr float c4 = (2.0f * 3.14159265f) / 3.0f;
            return std::pow(2.0f, -10.0f * t) *
                   std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
        }

        case EaseType::EaseOutBounce: {
            constexpr float n1 = 7.5625f;
            constexpr float d1 = 2.75f;

            if (t < 1.0f / d1) {
                return n1 * t * t;
            } else if (t < 2.0f / d1) {
                t -= 1.5f / d1;
                return n1 * t * t + 0.75f;
            } else if (t < 2.5f / d1) {
                t -= 2.25f / d1;
                return n1 * t * t + 0.9375f;
            } else {
                t -= 2.625f / d1;
                return n1 * t * t + 0.984375f;
            }
        }

        default:
            return t;
    }
}

// Template implementations for common types
template<>
float interpolate_linear(const float& a, const float& b, float t) {
    return a + (b - a) * t;
}

template<>
Vec2 interpolate_linear(const Vec2& a, const Vec2& b, float t) {
    return glm::mix(a, b, t);
}

template<>
Vec3 interpolate_linear(const Vec3& a, const Vec3& b, float t) {
    return glm::mix(a, b, t);
}

template<>
Vec4 interpolate_linear(const Vec4& a, const Vec4& b, float t) {
    return glm::mix(a, b, t);
}

template<>
Quat interpolate_linear(const Quat& a, const Quat& b, float t) {
    return glm::slerp(a, b, t);
}

// Catmull-Rom spline evaluation
template<>
Vec3 evaluate_catmull_rom(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
}

template<>
float evaluate_catmull_rom(const float& p0, const float& p1, const float& p2, const float& p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
}

// Cubic bezier evaluation helper
static float cubic_bezier(float p0, float p1, float p2, float p3, float t) {
    float one_minus_t = 1.0f - t;
    float one_minus_t_sq = one_minus_t * one_minus_t;
    float one_minus_t_cu = one_minus_t_sq * one_minus_t;
    float t_sq = t * t;
    float t_cu = t_sq * t;

    return one_minus_t_cu * p0 +
           3.0f * one_minus_t_sq * t * p1 +
           3.0f * one_minus_t * t_sq * p2 +
           t_cu * p3;
}

template<>
float interpolate_bezier(const float& a, const float& b,
                         const Vec2& tangent_out, const Vec2& tangent_in,
                         float t) {
    // Control points for cubic bezier
    float p0 = a;
    float p1 = a + tangent_out.y;  // Using y component as value offset
    float p2 = b + tangent_in.y;
    float p3 = b;

    return cubic_bezier(p0, p1, p2, p3, t);
}

template<>
Vec3 interpolate_bezier(const Vec3& a, const Vec3& b,
                        const Vec2& tangent_out, const Vec2& tangent_in,
                        float t) {
    // Simple bezier - could be extended with full Vec3 tangents
    Vec3 p1 = a + Vec3(tangent_out.x, tangent_out.y, 0.0f);
    Vec3 p2 = b + Vec3(tangent_in.x, tangent_in.y, 0.0f);

    float one_minus_t = 1.0f - t;
    float one_minus_t_sq = one_minus_t * one_minus_t;
    float one_minus_t_cu = one_minus_t_sq * one_minus_t;
    float t_sq = t * t;
    float t_cu = t_sq * t;

    return one_minus_t_cu * a +
           3.0f * one_minus_t_sq * t * p1 +
           3.0f * one_minus_t * t_sq * p2 +
           t_cu * b;
}

template<>
Quat interpolate_bezier(const Quat& a, const Quat& b,
                        const Vec2& /*tangent_out*/, const Vec2& /*tangent_in*/,
                        float t) {
    // Fallback to slerp for quaternion bezier (tangents are 2D, mapping is ambiguous)
    return glm::slerp(a, b, t);
}

template<>
Quat evaluate_catmull_rom(const Quat& /*p0*/, const Quat& p1, const Quat& p2, const Quat& /*p3*/, float t) {
    // Fallback to slerp for now - proper squad implementation requires intermediate point calculation
    return glm::slerp(p1, p2, t);
}

} // namespace engine::cinematic
