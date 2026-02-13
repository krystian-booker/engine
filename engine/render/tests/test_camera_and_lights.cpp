#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/render_pipeline.hpp>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// --- CameraData defaults ---

TEST_CASE("CameraData default values", "[render][camera]") {
    CameraData cam;

    // Matrices default to identity
    REQUIRE(cam.view_matrix == Mat4(1.0f));
    REQUIRE(cam.projection_matrix == Mat4(1.0f));
    REQUIRE(cam.view_projection == Mat4(1.0f));
    REQUIRE(cam.inverse_view == Mat4(1.0f));
    REQUIRE(cam.inverse_projection == Mat4(1.0f));
    REQUIRE(cam.inverse_view_projection == Mat4(1.0f));
    REQUIRE(cam.prev_view_projection == Mat4(1.0f));

    // Default forward is -Z
    REQUIRE_THAT(cam.forward.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.forward.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.forward.z, WithinAbs(-1.0f, 0.001f));

    // Default up is +Y
    REQUIRE_THAT(cam.up.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.up.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(cam.up.z, WithinAbs(0.0f, 0.001f));

    // Default right is +X
    REQUIRE_THAT(cam.right.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(cam.right.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.right.z, WithinAbs(0.0f, 0.001f));

    // Position at origin
    REQUIRE_THAT(cam.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.position.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.position.z, WithinAbs(0.0f, 0.001f));

    // Clip planes
    REQUIRE_THAT(cam.near_plane, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(cam.far_plane, WithinAbs(1000.0f, 0.001f));

    // FOV and aspect ratio
    REQUIRE_THAT(cam.fov_y, WithinAbs(60.0f, 0.001f));
    REQUIRE_THAT(cam.aspect_ratio, WithinAbs(16.0f / 9.0f, 0.001f));

    // Jitter defaults to zero
    REQUIRE_THAT(cam.jitter.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.jitter.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.prev_jitter.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.prev_jitter.y, WithinAbs(0.0f, 0.001f));
}

// --- make_camera_data ---

TEST_CASE("make_camera_data position and forward", "[render][camera]") {
    Vec3 pos(0.0f, 5.0f, 10.0f);
    Vec3 target(0.0f, 0.0f, 0.0f);
    Vec3 up(0.0f, 1.0f, 0.0f);
    auto cam = make_camera_data(pos, target, up, 60.0f, 16.0f / 9.0f, 0.1f, 100.0f);

    REQUIRE_THAT(cam.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.position.y, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(cam.position.z, WithinAbs(10.0f, 0.001f));

    // Forward should point from pos to target (normalized)
    Vec3 expected_forward = glm::normalize(target - pos);
    REQUIRE_THAT(cam.forward.x, WithinAbs(expected_forward.x, 0.001f));
    REQUIRE_THAT(cam.forward.y, WithinAbs(expected_forward.y, 0.001f));
    REQUIRE_THAT(cam.forward.z, WithinAbs(expected_forward.z, 0.001f));
}

TEST_CASE("make_camera_data view matrix matches glm::lookAt", "[render][camera]") {
    Vec3 pos(3.0f, 4.0f, 5.0f);
    Vec3 target(0.0f, 0.0f, 0.0f);
    Vec3 up(0.0f, 1.0f, 0.0f);
    auto cam = make_camera_data(pos, target, up, 60.0f, 1.0f, 0.1f, 100.0f);

    Mat4 expected_view = glm::lookAt(pos, target, up);
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            REQUIRE_THAT(cam.view_matrix[c][r], WithinAbs(expected_view[c][r], 0.001f));
        }
    }
}

TEST_CASE("make_camera_data projection matrix matches glm::perspective", "[render][camera]") {
    Vec3 pos(0.0f, 0.0f, 5.0f);
    Vec3 target(0.0f, 0.0f, 0.0f);
    Vec3 up(0.0f, 1.0f, 0.0f);
    float fov = 45.0f;
    float aspect = 16.0f / 9.0f;
    float near_p = 0.5f;
    float far_p = 500.0f;
    auto cam = make_camera_data(pos, target, up, fov, aspect, near_p, far_p);

    Mat4 expected_proj = glm::perspective(glm::radians(fov), aspect, near_p, far_p);
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            REQUIRE_THAT(cam.projection_matrix[c][r], WithinAbs(expected_proj[c][r], 0.001f));
        }
    }
}

TEST_CASE("make_camera_data VP = proj * view", "[render][camera]") {
    Vec3 pos(1.0f, 2.0f, 3.0f);
    Vec3 target(0.0f, 0.0f, 0.0f);
    Vec3 up(0.0f, 1.0f, 0.0f);
    auto cam = make_camera_data(pos, target, up, 60.0f, 1.5f, 0.1f, 100.0f);

    Mat4 expected_vp = cam.projection_matrix * cam.view_matrix;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            REQUIRE_THAT(cam.view_projection[c][r], WithinAbs(expected_vp[c][r], 0.001f));
        }
    }
}

TEST_CASE("make_camera_data inverses are correct", "[render][camera]") {
    Vec3 pos(5.0f, 3.0f, 8.0f);
    Vec3 target(0.0f, 0.0f, 0.0f);
    Vec3 up(0.0f, 1.0f, 0.0f);
    auto cam = make_camera_data(pos, target, up, 60.0f, 1.5f, 0.1f, 100.0f);

    // inverse_view * view should be ~identity
    Mat4 identity = cam.inverse_view * cam.view_matrix;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float expected = (c == r) ? 1.0f : 0.0f;
            REQUIRE_THAT(identity[c][r], WithinAbs(expected, 0.01f));
        }
    }

    // inverse_projection * projection should be ~identity
    Mat4 identity2 = cam.inverse_projection * cam.projection_matrix;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float expected = (c == r) ? 1.0f : 0.0f;
            REQUIRE_THAT(identity2[c][r], WithinAbs(expected, 0.01f));
        }
    }

    // inverse_view_projection * view_projection should be ~identity
    Mat4 identity3 = cam.inverse_view_projection * cam.view_projection;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float expected = (c == r) ? 1.0f : 0.0f;
            REQUIRE_THAT(identity3[c][r], WithinAbs(expected, 0.01f));
        }
    }
}

TEST_CASE("make_camera_data orthonormal basis", "[render][camera]") {
    Vec3 pos(0.0f, 10.0f, 0.0f);
    Vec3 target(0.0f, 0.0f, -10.0f);
    Vec3 up(0.0f, 1.0f, 0.0f);
    auto cam = make_camera_data(pos, target, up, 60.0f, 1.0f, 0.1f, 100.0f);

    // forward, up, right should be roughly unit length
    REQUIRE_THAT(glm::length(cam.forward), WithinAbs(1.0f, 0.01f));
    REQUIRE_THAT(glm::length(cam.up), WithinAbs(1.0f, 0.01f));
    REQUIRE_THAT(glm::length(cam.right), WithinAbs(1.0f, 0.01f));

    // forward and up should be perpendicular
    REQUIRE_THAT(glm::dot(cam.forward, cam.up), WithinAbs(0.0f, 0.01f));

    // right should be perpendicular to both
    REQUIRE_THAT(glm::dot(cam.right, cam.forward), WithinAbs(0.0f, 0.01f));
    REQUIRE_THAT(glm::dot(cam.right, cam.up), WithinAbs(0.0f, 0.01f));
}

TEST_CASE("make_camera_data stores clip plane values", "[render][camera]") {
    auto cam = make_camera_data(
        Vec3(0), Vec3(0, 0, -1), Vec3(0, 1, 0),
        90.0f, 2.0f, 0.5f, 200.0f);

    REQUIRE_THAT(cam.near_plane, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(cam.far_plane, WithinAbs(200.0f, 0.001f));
    REQUIRE_THAT(cam.fov_y, WithinAbs(90.0f, 0.001f));
    REQUIRE_THAT(cam.aspect_ratio, WithinAbs(2.0f, 0.001f));
}

// --- make_directional_light ---

TEST_CASE("make_directional_light", "[render][lights]") {
    auto light = make_directional_light(
        Vec3(0.0f, -1.0f, -1.0f),
        Vec3(1.0f, 0.9f, 0.8f),
        2.5f, true);

    REQUIRE(light.type == 0);
    REQUIRE_THAT(light.intensity, WithinAbs(2.5f, 0.001f));
    REQUIRE(light.cast_shadows == true);
    REQUIRE_THAT(light.range, WithinAbs(0.0f, 0.001f));

    // Direction should be normalized
    float len = glm::length(light.direction);
    REQUIRE_THAT(len, WithinAbs(1.0f, 0.001f));

    REQUIRE_THAT(light.color.r, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(light.color.g, WithinAbs(0.9f, 0.001f));
    REQUIRE_THAT(light.color.b, WithinAbs(0.8f, 0.001f));
}

// --- make_point_light ---

TEST_CASE("make_point_light", "[render][lights]") {
    auto light = make_point_light(
        Vec3(5.0f, 3.0f, -2.0f),
        Vec3(0.0f, 1.0f, 0.0f),
        10.0f, 25.0f, false);

    REQUIRE(light.type == 1);
    REQUIRE_THAT(light.position.x, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(light.position.y, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(light.position.z, WithinAbs(-2.0f, 0.001f));
    REQUIRE_THAT(light.intensity, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(light.range, WithinAbs(25.0f, 0.001f));
    REQUIRE(light.cast_shadows == false);
}

TEST_CASE("make_point_light default shadows", "[render][lights]") {
    auto light = make_point_light(Vec3(0), Vec3(1), 1.0f, 10.0f);
    REQUIRE(light.cast_shadows == false);
}

// --- make_spot_light ---

TEST_CASE("make_spot_light", "[render][lights]") {
    auto light = make_spot_light(
        Vec3(0.0f, 10.0f, 0.0f),
        Vec3(0.0f, -1.0f, 0.0f),
        Vec3(1.0f, 1.0f, 1.0f),
        5.0f, 30.0f,
        15.0f, 30.0f, true);

    REQUIRE(light.type == 2);

    // Direction should be normalized
    REQUIRE_THAT(glm::length(light.direction), WithinAbs(1.0f, 0.001f));

    REQUIRE_THAT(light.inner_angle, WithinAbs(15.0f, 0.001f));
    REQUIRE_THAT(light.outer_angle, WithinAbs(30.0f, 0.001f));
    REQUIRE_THAT(light.range, WithinAbs(30.0f, 0.001f));
    REQUIRE(light.cast_shadows == true);
}

TEST_CASE("make_spot_light default shadows", "[render][lights]") {
    auto light = make_spot_light(
        Vec3(0), Vec3(0, -1, 0), Vec3(1), 1.0f, 10.0f, 20.0f, 40.0f);
    REQUIRE(light.cast_shadows == false);
}

// --- RenderObject defaults ---

TEST_CASE("RenderObject defaults", "[render][camera]") {
    RenderObject obj;

    REQUIRE(obj.transform == Mat4(1.0f));
    REQUIRE(obj.blend_mode == 0);
    REQUIRE(obj.visible == true);
    REQUIRE(obj.casts_shadows == true);
    REQUIRE(obj.receives_shadows == true);
    REQUIRE(obj.layer_mask == 0xFFFFFFFF);
    REQUIRE(obj.skinned == false);
    REQUIRE(obj.bone_matrices == nullptr);
    REQUIRE(obj.bone_count == 0);
    REQUIRE_FALSE(obj.mesh.valid());
    REQUIRE_FALSE(obj.material.valid());
}

// --- Edge case: camera at same position as target ---

TEST_CASE("make_camera_data with very close position and target", "[render][camera]") {
    // Position very close to target - tests degenerate forward vector
    Vec3 pos(0.0f, 0.0f, 0.0f);
    Vec3 target(0.0f, 0.0f, -0.001f);
    Vec3 up(0.0f, 1.0f, 0.0f);
    auto cam = make_camera_data(pos, target, up, 60.0f, 1.0f, 0.1f, 100.0f);

    // Forward should still be roughly -Z
    REQUIRE_THAT(cam.forward.z, WithinAbs(-1.0f, 0.01f));
    // Matrices should still be valid (not NaN)
    REQUIRE_FALSE(std::isnan(cam.view_matrix[0][0]));
}

// --- Edge case: camera looking straight down ---

TEST_CASE("make_camera_data looking straight down", "[render][camera]") {
    Vec3 pos(0.0f, 10.0f, 0.0f);
    Vec3 target(0.0f, 0.0f, 0.0f);
    Vec3 up(0.0f, 0.0f, -1.0f);  // Must use non-parallel up vector
    auto cam = make_camera_data(pos, target, up, 60.0f, 1.0f, 0.1f, 100.0f);

    // Forward should be -Y (looking down)
    REQUIRE_THAT(cam.forward.y, WithinAbs(-1.0f, 0.01f));
    REQUIRE_THAT(glm::length(cam.forward), WithinAbs(1.0f, 0.01f));
}

// --- Edge case: extreme FOV values ---

TEST_CASE("make_camera_data with extreme FOV", "[render][camera]") {
    Vec3 pos(0, 0, 5);
    Vec3 target(0, 0, 0);
    Vec3 up(0, 1, 0);

    // Very narrow FOV
    auto cam_narrow = make_camera_data(pos, target, up, 5.0f, 1.0f, 0.1f, 100.0f);
    REQUIRE_THAT(cam_narrow.fov_y, WithinAbs(5.0f, 0.001f));

    // Wide FOV
    auto cam_wide = make_camera_data(pos, target, up, 120.0f, 1.0f, 0.1f, 100.0f);
    REQUIRE_THAT(cam_wide.fov_y, WithinAbs(120.0f, 0.001f));

    // Both should produce valid VP matrices
    Mat4 id1 = cam_narrow.inverse_view_projection * cam_narrow.view_projection;
    Mat4 id2 = cam_wide.inverse_view_projection * cam_wide.view_projection;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float expected = (c == r) ? 1.0f : 0.0f;
            REQUIRE_THAT(id1[c][r], WithinAbs(expected, 0.01f));
            REQUIRE_THAT(id2[c][r], WithinAbs(expected, 0.01f));
        }
    }
}

// --- prev_view_projection defaults to identity ---

TEST_CASE("make_camera_data prev_view_projection defaults to identity", "[render][camera]") {
    auto cam = make_camera_data(
        Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0),
        60.0f, 1.0f, 0.1f, 100.0f);

    // prev_view_projection is not set by make_camera_data, so it should be identity
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float expected = (c == r) ? 1.0f : 0.0f;
            REQUIRE_THAT(cam.prev_view_projection[c][r], WithinAbs(expected, 0.001f));
        }
    }
}

// --- Jitter defaults to zero ---

TEST_CASE("make_camera_data jitter defaults to zero", "[render][camera]") {
    auto cam = make_camera_data(
        Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0),
        60.0f, 1.0f, 0.1f, 100.0f);

    REQUIRE_THAT(cam.jitter.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.jitter.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.prev_jitter.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cam.prev_jitter.y, WithinAbs(0.0f, 0.001f));
}

// --- Directional light direction is always normalized ---

TEST_CASE("make_directional_light normalizes direction", "[render][lights]") {
    auto light = make_directional_light(Vec3(10.0f, -20.0f, 5.0f), Vec3(1), 1.0f);
    REQUIRE_THAT(glm::length(light.direction), WithinAbs(1.0f, 0.001f));
}

// --- Spot light direction is always normalized ---

TEST_CASE("make_spot_light normalizes arbitrary direction", "[render][lights]") {
    auto light = make_spot_light(
        Vec3(0), Vec3(100, -200, 50), Vec3(1), 1.0f, 10.0f, 15.0f, 30.0f);
    REQUIRE_THAT(glm::length(light.direction), WithinAbs(1.0f, 0.001f));
}
