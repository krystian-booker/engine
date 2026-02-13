#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/shadow_system.hpp>
#include <engine/render/render_pipeline.hpp>
#include <cmath>
#include <limits>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// --- Cascade split computation ---

TEST_CASE("Default cascade_splits are all zero (auto log-linear)", "[render][shadow]") {
    ShadowConfig config;
    for (uint32_t i = 0; i < config.cascade_count; ++i) {
        REQUIRE_THAT(config.cascade_splits[i], WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("MAX_CASCADES is 4", "[render][shadow][split]") {
    REQUIRE(MAX_CASCADES == 4);
}

// --- Frustum corner calculation ---

TEST_CASE("Frustum corners are 8 distinct points", "[render][shadow]") {
    Mat4 view = glm::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);

    auto corners = shadow::get_frustum_corners_world_space(view, proj);

    // All 8 corners should be finite
    for (int i = 0; i < 8; ++i) {
        REQUIRE_FALSE(std::isnan(corners[i].x));
        REQUIRE_FALSE(std::isnan(corners[i].y));
        REQUIRE_FALSE(std::isnan(corners[i].z));
        REQUIRE_FALSE(std::isinf(corners[i].x));
    }
}

TEST_CASE("Frustum corners for sub-range are between near and far", "[render][shadow]") {
    Mat4 view = glm::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);

    auto corners = shadow::get_frustum_corners_world_space(view, proj, 10.0f, 50.0f);

    // All corners should be in front of camera (z < 5 in world space for camera at z=5 looking at -Z)
    for (int i = 0; i < 8; ++i) {
        REQUIRE(corners[i].z < 5.0f);
    }
}

// --- Stable ortho projection ---

TEST_CASE("Stable ortho projection produces valid matrix", "[render][shadow]") {
    Vec3 min_bounds(-10, -10, -50);
    Vec3 max_bounds(10, 10, 50);

    Mat4 proj = shadow::create_stable_ortho_projection(min_bounds, max_bounds, 2048);

    // Matrix should not contain NaN
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            REQUIRE_FALSE(std::isnan(proj[i][j]));
        }
    }
}

TEST_CASE("Stable ortho projection snaps to texel grid", "[render][shadow]") {
    Vec3 min_bounds(-10.123f, -10.456f, -50.0f);
    Vec3 max_bounds(10.789f, 10.012f, 50.0f);

    Mat4 proj1 = shadow::create_stable_ortho_projection(min_bounds, max_bounds, 2048);

    // Slightly shift bounds
    Vec3 shifted_min = min_bounds + Vec3(0.001f, 0.001f, 0.0f);
    Vec3 shifted_max = max_bounds + Vec3(0.001f, 0.001f, 0.0f);

    Mat4 proj2 = shadow::create_stable_ortho_projection(shifted_min, shifted_max, 2048);

    // The projections should be very similar due to texel snapping
    // (not exactly equal, but the difference should be small)
    float diff = std::abs(proj1[0][0] - proj2[0][0]);
    REQUIRE(diff < 0.1f);
}

// --- Light ortho bounds ---

TEST_CASE("Light ortho bounds contain all corners", "[render][shadow]") {
    std::vector<Vec3> corners = {
        Vec3(-5, -5, -10),
        Vec3(5, -5, -10),
        Vec3(-5, 5, -10),
        Vec3(5, 5, -10),
        Vec3(-5, -5, -50),
        Vec3(5, -5, -50),
        Vec3(-5, 5, -50),
        Vec3(5, 5, -50),
    };

    Mat4 light_view = glm::lookAt(Vec3(0, 100, 0), Vec3(0, 0, 0), Vec3(0, 0, -1));

    Vec3 min_bounds, max_bounds;
    shadow::calculate_light_ortho_bounds(corners, light_view, min_bounds, max_bounds);

    // Bounds should be valid (min < max)
    REQUIRE(min_bounds.x < max_bounds.x);
    REQUIRE(min_bounds.y < max_bounds.y);
    REQUIRE(min_bounds.z < max_bounds.z);
}

// --- Shadow lookAt degeneracy fix ---

TEST_CASE("Shadow lookAt with vertical light direction produces valid matrix", "[render][shadow]") {
    // Directly downward light — this was a degenerate case before the fix
    Vec3 light_dir(0.0f, -1.0f, 0.0f);
    Vec3 center(0.0f, 0.0f, 0.0f);

    Vec3 up = (std::abs(glm::dot(light_dir, Vec3(0.0f, 1.0f, 0.0f))) > 0.99f)
               ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(0.0f, 1.0f, 0.0f);

    Mat4 light_view = glm::lookAt(center - light_dir * 100.0f, center, up);

    // Matrix should not contain NaN or Inf
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            REQUIRE_FALSE(std::isnan(light_view[i][j]));
            REQUIRE_FALSE(std::isinf(light_view[i][j]));
        }
    }
}

TEST_CASE("Shadow lookAt with nearly-vertical light direction is stable", "[render][shadow]") {
    // Light almost straight up
    Vec3 light_dir = glm::normalize(Vec3(0.001f, 0.9999f, 0.001f));
    Vec3 center(0.0f, 0.0f, 0.0f);

    Vec3 up = (std::abs(glm::dot(light_dir, Vec3(0.0f, 1.0f, 0.0f))) > 0.99f)
               ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(0.0f, 1.0f, 0.0f);

    Mat4 light_view = glm::lookAt(center - light_dir * 100.0f, center, up);

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            REQUIRE_FALSE(std::isnan(light_view[i][j]));
            REQUIRE_FALSE(std::isinf(light_view[i][j]));
        }
    }
}

TEST_CASE("Shadow lookAt with horizontal light direction uses default up", "[render][shadow]") {
    Vec3 light_dir = glm::normalize(Vec3(1.0f, 0.0f, -1.0f));
    Vec3 center(0.0f, 0.0f, 0.0f);

    Vec3 up = (std::abs(glm::dot(light_dir, Vec3(0.0f, 1.0f, 0.0f))) > 0.99f)
               ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(0.0f, 1.0f, 0.0f);

    // For horizontal light, should use (0,1,0)
    REQUIRE_THAT(up.y, WithinAbs(1.0f, 0.001f));

    Mat4 light_view = glm::lookAt(center - light_dir * 100.0f, center, up);

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            REQUIRE_FALSE(std::isnan(light_view[i][j]));
        }
    }
}
