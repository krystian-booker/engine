#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/core/math.hpp>
#include <array>
#include <cmath>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// We test the frustum plane extraction and AABB culling logic
// by reproducing the static helper functions from render_pipeline.cpp.
// These are internal free functions, so we re-implement them here for testing.

namespace {

void extract_frustum_planes(const Mat4& vp, std::array<Vec4, 6>& planes) {
    // Left
    planes[0] = Vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0],
                     vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    // Right
    planes[1] = Vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0],
                     vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    // Bottom
    planes[2] = Vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1],
                     vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    // Top
    planes[3] = Vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1],
                     vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    // Near
    planes[4] = Vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2],
                     vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    // Far
    planes[5] = Vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2],
                     vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);

    for (auto& plane : planes) {
        float len = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
        if (len > 0.0001f) {
            plane /= len;
        }
    }
}

bool point_inside_frustum(const Vec3& p, const std::array<Vec4, 6>& planes) {
    for (const auto& plane : planes) {
        float dist = plane.x * p.x + plane.y * p.y + plane.z * p.z + plane.w;
        if (dist < 0.0f) return false;
    }
    return true;
}

bool aabb_outside_frustum(const Vec3& min, const Vec3& max, const std::array<Vec4, 6>& planes) {
    for (const auto& plane : planes) {
        Vec3 positive_corner(
            plane.x >= 0 ? max.x : min.x,
            plane.y >= 0 ? max.y : min.y,
            plane.z >= 0 ? max.z : min.z
        );
        float dist = plane.x * positive_corner.x + plane.y * positive_corner.y +
                     plane.z * positive_corner.z + plane.w;
        if (dist < 0.0f) return true;
    }
    return false;
}

} // anonymous namespace

// Create a standard perspective VP matrix for testing
static Mat4 make_test_vp(const Vec3& pos = Vec3(0, 0, 5),
                          const Vec3& target = Vec3(0, 0, 0),
                          float fov = 60.0f, float aspect = 1.0f,
                          float near_p = 0.1f, float far_p = 100.0f) {
    Mat4 view = glm::lookAt(pos, target, Vec3(0, 1, 0));
    Mat4 proj = glm::perspective(glm::radians(fov), aspect, near_p, far_p);
    return proj * view;
}

// --- Frustum plane extraction ---

TEST_CASE("Frustum planes are extracted from VP matrix", "[render][frustum]") {
    Mat4 vp = make_test_vp();
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // All 6 planes should exist (non-zero normal)
    for (int i = 0; i < 6; ++i) {
        float len = std::sqrt(planes[i].x * planes[i].x +
                              planes[i].y * planes[i].y +
                              planes[i].z * planes[i].z);
        REQUIRE_THAT(len, WithinAbs(1.0f, 0.01f));
    }
}

TEST_CASE("Frustum planes are normalized", "[render][frustum]") {
    Mat4 vp = make_test_vp(Vec3(10, 20, 30), Vec3(0, 0, 0), 90.0f, 1.5f, 1.0f, 500.0f);
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    for (int i = 0; i < 6; ++i) {
        float len = std::sqrt(planes[i].x * planes[i].x +
                              planes[i].y * planes[i].y +
                              planes[i].z * planes[i].z);
        REQUIRE_THAT(len, WithinAbs(1.0f, 0.01f));
    }
}

// --- Point containment ---

TEST_CASE("Origin is inside default frustum", "[render][frustum]") {
    Mat4 vp = make_test_vp();
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    REQUIRE(point_inside_frustum(Vec3(0, 0, 0), planes));
}

TEST_CASE("Point behind camera is outside frustum", "[render][frustum]") {
    // Camera at (0,0,5) looking at origin. Point behind camera at (0,0,10)
    Mat4 vp = make_test_vp();
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    REQUIRE_FALSE(point_inside_frustum(Vec3(0, 0, 10), planes));
}

TEST_CASE("Point beyond far plane is outside frustum", "[render][frustum]") {
    Mat4 vp = make_test_vp(Vec3(0, 0, 5), Vec3(0, 0, 0), 60.0f, 1.0f, 0.1f, 10.0f);
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // Camera at (0,0,5) looking at -Z. Far plane at distance 10, so world z = 5-10 = -5
    // Point at z=-20 is beyond far plane
    REQUIRE_FALSE(point_inside_frustum(Vec3(0, 0, -20), planes));
}

TEST_CASE("Point far to the side is outside frustum", "[render][frustum]") {
    Mat4 vp = make_test_vp(Vec3(0, 0, 5), Vec3(0, 0, 0), 60.0f, 1.0f, 0.1f, 100.0f);
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // Point far to the right
    REQUIRE_FALSE(point_inside_frustum(Vec3(1000, 0, 0), planes));
}

// --- AABB containment ---

TEST_CASE("Small AABB at origin is inside frustum", "[render][frustum]") {
    Mat4 vp = make_test_vp();
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    Vec3 min(-0.5f, -0.5f, -0.5f);
    Vec3 max(0.5f, 0.5f, 0.5f);
    REQUIRE_FALSE(aabb_outside_frustum(min, max, planes));
}

TEST_CASE("AABB behind camera is outside frustum", "[render][frustum]") {
    Mat4 vp = make_test_vp();
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    Vec3 min(0, 0, 10);
    Vec3 max(1, 1, 15);
    REQUIRE(aabb_outside_frustum(min, max, planes));
}

TEST_CASE("AABB straddling near plane is inside", "[render][frustum]") {
    // Camera at (0,0,5), near = 0.1
    // Near plane at world z ~ 4.9
    Mat4 vp = make_test_vp(Vec3(0, 0, 5), Vec3(0, 0, 0), 60.0f, 1.0f, 0.1f, 100.0f);
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // AABB that straddles the near plane
    Vec3 min(-0.1f, -0.1f, 4.85f);
    Vec3 max(0.1f, 0.1f, 4.95f);
    REQUIRE_FALSE(aabb_outside_frustum(min, max, planes));
}

TEST_CASE("AABB beyond far plane is outside", "[render][frustum]") {
    Mat4 vp = make_test_vp(Vec3(0, 0, 5), Vec3(0, 0, 0), 60.0f, 1.0f, 0.1f, 10.0f);
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // Far plane at world z = 5 - 10 = -5. AABB at z = -20 to -15
    Vec3 min(-1, -1, -20);
    Vec3 max(1, 1, -15);
    REQUIRE(aabb_outside_frustum(min, max, planes));
}

TEST_CASE("Large AABB containing camera is inside", "[render][frustum]") {
    Mat4 vp = make_test_vp();
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    Vec3 min(-1000, -1000, -1000);
    Vec3 max(1000, 1000, 1000);
    REQUIRE_FALSE(aabb_outside_frustum(min, max, planes));
}

// --- Translated/rotated camera ---

TEST_CASE("Frustum culling with translated camera", "[render][frustum]") {
    // Camera at (100, 0, 0) looking at (100, 0, -10)
    Mat4 vp = make_test_vp(Vec3(100, 0, 0), Vec3(100, 0, -10), 60.0f, 1.0f, 0.1f, 50.0f);
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // Object at (100, 0, -5) should be visible
    REQUIRE_FALSE(aabb_outside_frustum(Vec3(99, -1, -6), Vec3(101, 1, -4), planes));

    // Object at (0, 0, 0) should be outside (far from camera)
    REQUIRE(aabb_outside_frustum(Vec3(-1, -1, -1), Vec3(1, 1, 1), planes));
}

TEST_CASE("Frustum culling with camera looking up", "[render][frustum]") {
    // Camera at origin, looking up (+Y)
    Mat4 view = glm::lookAt(Vec3(0, 0, 0), Vec3(0, 10, 0), Vec3(0, 0, -1));
    Mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 50.0f);
    Mat4 vp = proj * view;
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // Object above camera should be visible
    REQUIRE_FALSE(aabb_outside_frustum(Vec3(-1, 5, -1), Vec3(1, 7, 1), planes));

    // Object below camera should be outside
    REQUIRE(aabb_outside_frustum(Vec3(-1, -10, -1), Vec3(1, -8, 1), planes));
}

// --- Orthographic projection tests ---

TEST_CASE("Frustum planes from orthographic projection", "[render][frustum]") {
    Mat4 view = glm::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 proj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 50.0f);
    Mat4 vp = proj * view;
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // Planes should be normalized
    for (int i = 0; i < 6; ++i) {
        float len = std::sqrt(planes[i].x * planes[i].x +
                              planes[i].y * planes[i].y +
                              planes[i].z * planes[i].z);
        REQUIRE_THAT(len, WithinAbs(1.0f, 0.01f));
    }
}

TEST_CASE("Ortho frustum: object at origin is inside", "[render][frustum]") {
    Mat4 view = glm::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 proj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 50.0f);
    Mat4 vp = proj * view;
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    REQUIRE_FALSE(aabb_outside_frustum(Vec3(-1, -1, -1), Vec3(1, 1, 1), planes));
}

TEST_CASE("Ortho frustum: object outside left boundary", "[render][frustum]") {
    Mat4 view = glm::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 proj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 50.0f);
    Mat4 vp = proj * view;
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // Object entirely outside the left boundary
    REQUIRE(aabb_outside_frustum(Vec3(-20, -1, -1), Vec3(-15, 1, 1), planes));
}

TEST_CASE("Ortho frustum: object behind camera", "[render][frustum]") {
    Mat4 view = glm::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 proj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 50.0f);
    Mat4 vp = proj * view;
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // Object behind camera (z > 5 in world space)
    REQUIRE(aabb_outside_frustum(Vec3(-1, -1, 10), Vec3(1, 1, 15), planes));
}

// --- Boundary/edge case tests ---

TEST_CASE("Zero-size AABB at origin is inside frustum", "[render][frustum]") {
    Mat4 vp = make_test_vp();
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // Degenerate AABB (point)
    REQUIRE_FALSE(aabb_outside_frustum(Vec3(0), Vec3(0), planes));
}

TEST_CASE("Very small near plane still works", "[render][frustum]") {
    Mat4 vp = make_test_vp(Vec3(0, 0, 5), Vec3(0, 0, 0), 60.0f, 1.0f, 0.001f, 100.0f);
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // Object just in front of camera should be visible
    REQUIRE_FALSE(aabb_outside_frustum(Vec3(-0.1f, -0.1f, 4.99f), Vec3(0.1f, 0.1f, 4.999f), planes));

    // Object at origin should be visible
    REQUIRE(point_inside_frustum(Vec3(0, 0, 0), planes));
}

TEST_CASE("Narrow FOV culls wide objects", "[render][frustum]") {
    // Very narrow FOV (5 degrees)
    Mat4 vp = make_test_vp(Vec3(0, 0, 5), Vec3(0, 0, 0), 5.0f, 1.0f, 0.1f, 100.0f);
    std::array<Vec4, 6> planes;
    extract_frustum_planes(vp, planes);

    // Object on the center axis should be visible
    REQUIRE_FALSE(aabb_outside_frustum(Vec3(-0.1f, -0.1f, -0.1f), Vec3(0.1f, 0.1f, 0.1f), planes));

    // Object far to the side should be culled with narrow FOV
    REQUIRE(aabb_outside_frustum(Vec3(10, 10, 0), Vec3(11, 11, 1), planes));
}
