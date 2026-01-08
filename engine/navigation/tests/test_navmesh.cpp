#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/navigation/navmesh.hpp>

using namespace engine::navigation;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("NavMeshSettings defaults", "[navigation][navmesh]") {
    NavMeshSettings settings;

    // Rasterization
    REQUIRE_THAT(settings.cell_size, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(settings.cell_height, WithinAbs(0.2f, 0.001f));

    // Agent properties
    REQUIRE_THAT(settings.agent_height, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(settings.agent_radius, WithinAbs(0.6f, 0.001f));
    REQUIRE_THAT(settings.agent_max_climb, WithinAbs(0.9f, 0.001f));
    REQUIRE_THAT(settings.agent_max_slope, WithinAbs(45.0f, 0.001f));

    // Region settings
    REQUIRE(settings.min_region_area == 8);
    REQUIRE(settings.merge_region_area == 20);

    // Edge settings
    REQUIRE_THAT(settings.max_edge_length, WithinAbs(12.0f, 0.001f));
    REQUIRE_THAT(settings.max_edge_error, WithinAbs(1.3f, 0.001f));

    // Detail mesh
    REQUIRE_THAT(settings.detail_sample_distance, WithinAbs(6.0f, 0.001f));
    REQUIRE_THAT(settings.detail_sample_max_error, WithinAbs(1.0f, 0.001f));

    // Polygon settings
    REQUIRE(settings.max_verts_per_poly == 6);

    // Tile settings
    REQUIRE(settings.use_tiles == false);
    REQUIRE_THAT(settings.tile_size, WithinAbs(48.0f, 0.001f));

    // Tile cache
    REQUIRE(settings.max_layers == 32);
}

TEST_CASE("NavMeshSettings custom values", "[navigation][navmesh]") {
    NavMeshSettings settings;
    settings.cell_size = 0.2f;
    settings.cell_height = 0.1f;
    settings.agent_height = 1.8f;
    settings.agent_radius = 0.4f;
    settings.use_tiles = true;
    settings.tile_size = 32.0f;

    REQUIRE_THAT(settings.cell_size, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(settings.cell_height, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(settings.agent_height, WithinAbs(1.8f, 0.001f));
    REQUIRE_THAT(settings.agent_radius, WithinAbs(0.4f, 0.001f));
    REQUIRE(settings.use_tiles == true);
    REQUIRE_THAT(settings.tile_size, WithinAbs(32.0f, 0.001f));
}

TEST_CASE("INVALID_NAV_POLY_REF constant", "[navigation][navmesh]") {
    REQUIRE(INVALID_NAV_POLY_REF == 0);
}

TEST_CASE("NavMesh default construction", "[navigation][navmesh]") {
    NavMesh navmesh;

    REQUIRE_FALSE(navmesh.is_valid());
    REQUIRE(navmesh.get_detour_navmesh() == nullptr);
    REQUIRE(navmesh.supports_tile_cache() == false);
}

TEST_CASE("NavMesh::DebugVertex", "[navigation][navmesh]") {
    NavMesh::DebugVertex vertex;
    vertex.position = Vec3{1.0f, 2.0f, 3.0f};
    vertex.color = Vec4{1.0f, 0.0f, 0.0f, 1.0f};

    REQUIRE_THAT(vertex.position.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(vertex.position.y, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(vertex.position.z, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(vertex.color.r, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(vertex.color.a, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("NavMesh invalid operations", "[navigation][navmesh]") {
    NavMesh navmesh;

    // Operations on invalid navmesh should be safe
    REQUIRE(navmesh.get_tile_count() == 0);
    REQUIRE(navmesh.get_polygon_count() == 0);
    REQUIRE(navmesh.get_vertex_count() == 0);

    auto debug_geo = navmesh.get_debug_geometry();
    REQUIRE(debug_geo.empty());

    auto binary = navmesh.get_binary_data();
    REQUIRE(binary.empty());
}

TEST_CASE("NavMesh load nonexistent file", "[navigation][navmesh]") {
    NavMesh navmesh;

    bool loaded = navmesh.load("nonexistent_navmesh.bin");

    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(navmesh.is_valid());
}

TEST_CASE("NavMesh load_from_memory empty data", "[navigation][navmesh]") {
    NavMesh navmesh;

    bool loaded = navmesh.load_from_memory(nullptr, 0);

    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(navmesh.is_valid());
}

TEST_CASE("NavMesh tile cache layers", "[navigation][navmesh]") {
    NavMesh navmesh;

    std::vector<std::vector<uint8_t>> layers;
    layers.push_back({1, 2, 3});
    layers.push_back({4, 5, 6});

    navmesh.set_tile_cache_layers(std::move(layers));

    const auto& retrieved = navmesh.get_tile_cache_layers();
    REQUIRE(retrieved.size() == 2);
    REQUIRE(retrieved[0].size() == 3);
    REQUIRE(retrieved[1].size() == 3);
}
