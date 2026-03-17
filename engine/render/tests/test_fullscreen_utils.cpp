#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/fullscreen_utils.hpp>

using namespace engine::render;
using Catch::Matchers::WithinAbs;

TEST_CASE("Fullscreen quad uses top-left UV origin when render targets are top-left", "[render][fullscreen]") {
    const auto vertices = make_fullscreen_quad_vertices(false);

    REQUIRE_THAT(vertices[0].u, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(vertices[0].v, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(vertices[1].u, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(vertices[1].v, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(vertices[2].u, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(vertices[2].v, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(vertices[3].u, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(vertices[3].v, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("Fullscreen quad flips only V when render targets are bottom-left", "[render][fullscreen]") {
    const auto vertices = make_fullscreen_quad_vertices(true);

    REQUIRE_THAT(vertices[0].u, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(vertices[0].v, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(vertices[1].u, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(vertices[1].v, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(vertices[2].u, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(vertices[2].v, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(vertices[3].u, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(vertices[3].v, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Fullscreen quad indices keep a consistent winding order", "[render][fullscreen]") {
    const auto indices = make_fullscreen_quad_indices();

    REQUIRE(indices[0] == 0);
    REQUIRE(indices[1] == 1);
    REQUIRE(indices[2] == 2);
    REQUIRE(indices[3] == 0);
    REQUIRE(indices[4] == 2);
    REQUIRE(indices[5] == 3);
}
