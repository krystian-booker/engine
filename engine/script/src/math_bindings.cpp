#include <engine/script/bindings.hpp>
#include <engine/core/math.hpp>

namespace engine::script {

void register_math_bindings(sol::state& lua) {
    using namespace engine::core;

    // Vec2
    lua.new_usertype<Vec2>("Vec2",
        sol::constructors<Vec2(), Vec2(float), Vec2(float, float)>(),
        "x", &Vec2::x,
        "y", &Vec2::y,
        sol::meta_function::addition, [](const Vec2& a, const Vec2& b) { return a + b; },
        sol::meta_function::subtraction, [](const Vec2& a, const Vec2& b) { return a - b; },
        sol::meta_function::multiplication, sol::overload(
            [](const Vec2& v, float s) { return v * s; },
            [](float s, const Vec2& v) { return s * v; }
        ),
        sol::meta_function::division, [](const Vec2& v, float s) { return v / s; },
        sol::meta_function::unary_minus, [](const Vec2& v) { return -v; },
        sol::meta_function::to_string, [](const Vec2& v) {
            return "Vec2(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
        },
        "length", [](const Vec2& v) { return glm::length(v); },
        "length_squared", [](const Vec2& v) { return glm::dot(v, v); },
        "normalized", [](const Vec2& v) { return glm::normalize(v); },
        "dot", [](const Vec2& a, const Vec2& b) { return glm::dot(a, b); }
    );

    // Vec3
    lua.new_usertype<Vec3>("Vec3",
        sol::constructors<Vec3(), Vec3(float), Vec3(float, float, float)>(),
        "x", &Vec3::x,
        "y", &Vec3::y,
        "z", &Vec3::z,
        sol::meta_function::addition, [](const Vec3& a, const Vec3& b) { return a + b; },
        sol::meta_function::subtraction, [](const Vec3& a, const Vec3& b) { return a - b; },
        sol::meta_function::multiplication, sol::overload(
            [](const Vec3& v, float s) { return v * s; },
            [](float s, const Vec3& v) { return s * v; }
        ),
        sol::meta_function::division, [](const Vec3& v, float s) { return v / s; },
        sol::meta_function::unary_minus, [](const Vec3& v) { return -v; },
        sol::meta_function::to_string, [](const Vec3& v) {
            return "Vec3(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
        },
        "length", [](const Vec3& v) { return glm::length(v); },
        "length_squared", [](const Vec3& v) { return glm::dot(v, v); },
        "normalized", [](const Vec3& v) { return glm::normalize(v); },
        "dot", [](const Vec3& a, const Vec3& b) { return glm::dot(a, b); },
        "cross", [](const Vec3& a, const Vec3& b) { return glm::cross(a, b); }
    );

    // Vec4
    lua.new_usertype<Vec4>("Vec4",
        sol::constructors<Vec4(), Vec4(float), Vec4(float, float, float, float)>(),
        "x", &Vec4::x,
        "y", &Vec4::y,
        "z", &Vec4::z,
        "w", &Vec4::w,
        sol::meta_function::addition, [](const Vec4& a, const Vec4& b) { return a + b; },
        sol::meta_function::subtraction, [](const Vec4& a, const Vec4& b) { return a - b; },
        sol::meta_function::multiplication, sol::overload(
            [](const Vec4& v, float s) { return v * s; },
            [](float s, const Vec4& v) { return s * v; }
        ),
        sol::meta_function::division, [](const Vec4& v, float s) { return v / s; },
        sol::meta_function::to_string, [](const Vec4& v) {
            return "Vec4(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " +
                   std::to_string(v.z) + ", " + std::to_string(v.w) + ")";
        }
    );

    // Quat
    lua.new_usertype<Quat>("Quat",
        sol::constructors<Quat(), Quat(float, float, float, float)>(),
        "x", &Quat::x,
        "y", &Quat::y,
        "z", &Quat::z,
        "w", &Quat::w,
        sol::meta_function::multiplication, sol::overload(
            [](const Quat& a, const Quat& b) { return a * b; },
            [](const Quat& q, const Vec3& v) { return q * v; }
        ),
        sol::meta_function::to_string, [](const Quat& q) {
            return "Quat(" + std::to_string(q.x) + ", " + std::to_string(q.y) + ", " +
                   std::to_string(q.z) + ", " + std::to_string(q.w) + ")";
        },
        "normalized", [](const Quat& q) { return glm::normalize(q); },
        "inverse", [](const Quat& q) { return glm::inverse(q); },
        "conjugate", [](const Quat& q) { return glm::conjugate(q); }
    );

    // Helper functions in a math namespace
    auto math = lua.create_named_table("math3d");

    math.set_function("vec2", [](float x, float y) { return Vec2(x, y); });
    math.set_function("vec3", [](float x, float y, float z) { return Vec3(x, y, z); });
    math.set_function("vec4", [](float x, float y, float z, float w) { return Vec4(x, y, z, w); });

    math.set_function("lerp", sol::overload(
        [](float a, float b, float t) { return glm::mix(a, b, t); },
        [](const Vec2& a, const Vec2& b, float t) { return glm::mix(a, b, t); },
        [](const Vec3& a, const Vec3& b, float t) { return glm::mix(a, b, t); },
        [](const Vec4& a, const Vec4& b, float t) { return glm::mix(a, b, t); }
    ));

    math.set_function("slerp", [](const Quat& a, const Quat& b, float t) {
        return glm::slerp(a, b, t);
    });

    math.set_function("distance", sol::overload(
        [](const Vec2& a, const Vec2& b) { return glm::distance(a, b); },
        [](const Vec3& a, const Vec3& b) { return glm::distance(a, b); }
    ));

    math.set_function("angle_axis", [](float angle, const Vec3& axis) {
        return glm::angleAxis(angle, axis);
    });

    math.set_function("euler_to_quat", [](const Vec3& euler) {
        return Quat(euler);
    });

    math.set_function("quat_to_euler", [](const Quat& q) {
        return glm::eulerAngles(q);
    });

    math.set_function("look_at_rotation", [](const Vec3& dir, const Vec3& up) {
        return glm::quatLookAt(glm::normalize(dir), up);
    });

    math.set_function("clamp", sol::overload(
        [](float v, float min, float max) { return glm::clamp(v, min, max); },
        [](const Vec3& v, const Vec3& min, const Vec3& max) { return glm::clamp(v, min, max); }
    ));

    // Constants
    math["PI"] = glm::pi<float>();
    math["TWO_PI"] = glm::two_pi<float>();
    math["HALF_PI"] = glm::half_pi<float>();
    math["DEG_TO_RAD"] = glm::pi<float>() / 180.0f;
    math["RAD_TO_DEG"] = 180.0f / glm::pi<float>();

    math["UP"] = Vec3(0.0f, 1.0f, 0.0f);
    math["DOWN"] = Vec3(0.0f, -1.0f, 0.0f);
    math["LEFT"] = Vec3(-1.0f, 0.0f, 0.0f);
    math["RIGHT"] = Vec3(1.0f, 0.0f, 0.0f);
    math["FORWARD"] = Vec3(0.0f, 0.0f, -1.0f);
    math["BACK"] = Vec3(0.0f, 0.0f, 1.0f);
}

} // namespace engine::script
