#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/navigation/navmesh_builder.hpp>

using namespace engine::navigation;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("NavAreaType enum", "[navigation][builder]") {
    REQUIRE(static_cast<uint8_t>(NavAreaType::Walkable) == 0);
    REQUIRE(static_cast<uint8_t>(NavAreaType::Water) == 1);
    REQUIRE(static_cast<uint8_t>(NavAreaType::Grass) == 2);
    REQUIRE(static_cast<uint8_t>(NavAreaType::Road) == 3);
    REQUIRE(static_cast<uint8_t>(NavAreaType::Door) == 4);
    REQUIRE(static_cast<uint8_t>(NavAreaType::Jump) == 5);
    REQUIRE(static_cast<uint8_t>(NavAreaType::NotWalkable) == 63);
}

TEST_CASE("NavAreaCosts defaults", "[navigation][builder]") {
    NavAreaCosts costs;

    // Default cost should be 1.0 for walkable areas
    REQUIRE_THAT(costs.get_cost(NavAreaType::Walkable), WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(costs.get_cost(NavAreaType::Water), WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(costs.get_cost(NavAreaType::Road), WithinAbs(1.0f, 0.001f));

    // NotWalkable should have very high cost
    REQUIRE(costs.get_cost(NavAreaType::NotWalkable) > 999999.0f);
}

TEST_CASE("NavAreaCosts set_cost", "[navigation][builder]") {
    NavAreaCosts costs;

    costs.set_cost(NavAreaType::Water, 3.0f);
    costs.set_cost(NavAreaType::Road, 0.5f);
    costs.set_cost(NavAreaType::Grass, 1.5f);

    REQUIRE_THAT(costs.get_cost(NavAreaType::Water), WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(costs.get_cost(NavAreaType::Road), WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(costs.get_cost(NavAreaType::Grass), WithinAbs(1.5f, 0.001f));
}

TEST_CASE("OffMeshConnectionFlags enum", "[navigation][builder]") {
    REQUIRE(static_cast<uint16_t>(OffMeshConnectionFlags::None) == 0);
    REQUIRE(static_cast<uint16_t>(OffMeshConnectionFlags::Bidirectional) == 1);
    REQUIRE(static_cast<uint16_t>(OffMeshConnectionFlags::Jump) == 2);
    REQUIRE(static_cast<uint16_t>(OffMeshConnectionFlags::Ladder) == 4);
    REQUIRE(static_cast<uint16_t>(OffMeshConnectionFlags::Door) == 8);
    REQUIRE(static_cast<uint16_t>(OffMeshConnectionFlags::Teleport) == 16);
    REQUIRE(static_cast<uint16_t>(OffMeshConnectionFlags::Climb) == 32);
}

TEST_CASE("OffMeshConnectionFlags operators", "[navigation][builder]") {
    SECTION("OR operator") {
        auto combined = OffMeshConnectionFlags::Bidirectional | OffMeshConnectionFlags::Jump;
        REQUIRE(static_cast<uint16_t>(combined) == 3);
    }

    SECTION("AND operator") {
        auto combined = OffMeshConnectionFlags::Bidirectional | OffMeshConnectionFlags::Jump;
        auto result = combined & OffMeshConnectionFlags::Jump;
        REQUIRE(static_cast<uint16_t>(result) == 2);
    }

    SECTION("has_flag helper") {
        auto flags = OffMeshConnectionFlags::Bidirectional | OffMeshConnectionFlags::Ladder;

        REQUIRE(has_flag(flags, OffMeshConnectionFlags::Bidirectional));
        REQUIRE(has_flag(flags, OffMeshConnectionFlags::Ladder));
        REQUIRE_FALSE(has_flag(flags, OffMeshConnectionFlags::Jump));
        REQUIRE_FALSE(has_flag(flags, OffMeshConnectionFlags::Door));
    }
}

TEST_CASE("OffMeshConnection defaults", "[navigation][builder]") {
    OffMeshConnection connection;

    REQUIRE_THAT(connection.start.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(connection.start.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(connection.start.z, WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(connection.end.x, WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(connection.radius, WithinAbs(0.5f, 0.001f));
    REQUIRE(connection.flags == OffMeshConnectionFlags::Bidirectional);
    REQUIRE(connection.area == NavAreaType::Walkable);
    REQUIRE(connection.user_id == 0);
}

TEST_CASE("OffMeshConnection custom values", "[navigation][builder]") {
    OffMeshConnection connection;
    connection.start = Vec3{0.0f, 0.0f, 0.0f};
    connection.end = Vec3{0.0f, 5.0f, 0.0f};
    connection.radius = 0.3f;
    connection.flags = OffMeshConnectionFlags::Ladder | OffMeshConnectionFlags::Bidirectional;
    connection.area = NavAreaType::Jump;
    connection.user_id = 42;

    REQUIRE_THAT(connection.end.y, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(connection.radius, WithinAbs(0.3f, 0.001f));
    REQUIRE(has_flag(connection.flags, OffMeshConnectionFlags::Ladder));
    REQUIRE(has_flag(connection.flags, OffMeshConnectionFlags::Bidirectional));
    REQUIRE(connection.area == NavAreaType::Jump);
    REQUIRE(connection.user_id == 42);
}

TEST_CASE("NavMeshInputGeometry defaults", "[navigation][builder]") {
    NavMeshInputGeometry geometry;

    REQUIRE(geometry.vertices.empty());
    REQUIRE(geometry.indices.empty());
    REQUIRE(geometry.area_types.empty());
    REQUIRE(geometry.off_mesh_connections.empty());
    REQUIRE(geometry.triangle_count() == 0);
    REQUIRE(geometry.off_mesh_count() == 0);
}

TEST_CASE("NavMeshInputGeometry add vertices and indices", "[navigation][builder]") {
    NavMeshInputGeometry geometry;

    geometry.vertices = {
        Vec3{0.0f, 0.0f, 0.0f},
        Vec3{10.0f, 0.0f, 0.0f},
        Vec3{5.0f, 0.0f, 10.0f}
    };
    geometry.indices = {0, 1, 2};

    REQUIRE(geometry.vertices.size() == 3);
    REQUIRE(geometry.indices.size() == 3);
    REQUIRE(geometry.triangle_count() == 1);
}

TEST_CASE("NavMeshInputGeometry multiple triangles", "[navigation][builder]") {
    NavMeshInputGeometry geometry;

    geometry.vertices = {
        Vec3{0.0f, 0.0f, 0.0f},
        Vec3{10.0f, 0.0f, 0.0f},
        Vec3{10.0f, 0.0f, 10.0f},
        Vec3{0.0f, 0.0f, 10.0f}
    };
    geometry.indices = {0, 1, 2, 0, 2, 3};

    REQUIRE(geometry.triangle_count() == 2);
}

TEST_CASE("NavMeshInputGeometry add_off_mesh_connection", "[navigation][builder]") {
    NavMeshInputGeometry geometry;

    OffMeshConnection connection;
    connection.start = Vec3{0.0f, 0.0f, 0.0f};
    connection.end = Vec3{0.0f, 3.0f, 0.0f};

    geometry.add_off_mesh_connection(connection);

    REQUIRE(geometry.off_mesh_count() == 1);
    REQUIRE_THAT(geometry.off_mesh_connections[0].end.y, WithinAbs(3.0f, 0.001f));
}

TEST_CASE("NavMeshInputGeometry add_off_mesh_connection convenience", "[navigation][builder]") {
    NavMeshInputGeometry geometry;

    geometry.add_off_mesh_connection(
        Vec3{0.0f, 0.0f, 0.0f},
        Vec3{5.0f, 2.0f, 0.0f},
        0.4f,
        OffMeshConnectionFlags::Jump,
        NavAreaType::Jump,
        100
    );

    REQUIRE(geometry.off_mesh_count() == 1);

    const auto& conn = geometry.off_mesh_connections[0];
    REQUIRE_THAT(conn.end.x, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(conn.end.y, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(conn.radius, WithinAbs(0.4f, 0.001f));
    REQUIRE(conn.flags == OffMeshConnectionFlags::Jump);
    REQUIRE(conn.area == NavAreaType::Jump);
    REQUIRE(conn.user_id == 100);
}

TEST_CASE("NavMeshInputGeometry clear", "[navigation][builder]") {
    NavMeshInputGeometry geometry;
    geometry.vertices = {Vec3{0.0f}, Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}};
    geometry.indices = {0, 1, 2};
    geometry.add_off_mesh_connection(Vec3{0.0f}, Vec3{1.0f, 0.0f, 0.0f});

    REQUIRE_FALSE(geometry.vertices.empty());

    geometry.clear();

    REQUIRE(geometry.vertices.empty());
    REQUIRE(geometry.indices.empty());
    REQUIRE(geometry.off_mesh_connections.empty());
}

TEST_CASE("NavMeshBuildResult defaults", "[navigation][builder]") {
    NavMeshBuildResult result;

    REQUIRE(result.navmesh == nullptr);
    REQUIRE(result.success == false);
    REQUIRE(result.error_message.empty());
    REQUIRE_THAT(result.build_time_ms, WithinAbs(0.0f, 0.001f));
    REQUIRE(result.input_vertices == 0);
    REQUIRE(result.input_triangles == 0);
    REQUIRE(result.output_polygons == 0);
    REQUIRE(result.output_tiles == 0);
}

TEST_CASE("NavMeshSource defaults", "[navigation][builder]") {
    NavMeshSource source;

    REQUIRE(source.vertices.empty());
    REQUIRE(source.indices.empty());
    REQUIRE(source.area_type == 0);
    REQUIRE(source.enabled == true);
}

TEST_CASE("OffMeshLinkComponent defaults", "[navigation][builder]") {
    OffMeshLinkComponent link;

    REQUIRE_THAT(link.start_offset.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(link.end_offset.z, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(link.radius, WithinAbs(0.5f, 0.001f));
    REQUIRE(link.flags == OffMeshConnectionFlags::Bidirectional);
    REQUIRE(link.area == NavAreaType::Walkable);
    REQUIRE(link.enabled == true);
}

TEST_CASE("NavMeshBuilder construction", "[navigation][builder]") {
    NavMeshBuilder builder;

    REQUIRE_FALSE(builder.is_building());
}

TEST_CASE("NavMeshBuilder build empty geometry", "[navigation][builder]") {
    NavMeshBuilder builder;
    NavMeshInputGeometry geometry;
    NavMeshSettings settings;

    auto result = builder.build(geometry, settings);

    // Empty geometry should fail
    REQUIRE_FALSE(result.success);
    REQUIRE(result.navmesh == nullptr);
}
