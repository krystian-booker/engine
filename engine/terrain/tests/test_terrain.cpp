#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/terrain/terrain.hpp>

using namespace engine::terrain;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// TerrainConfig Tests
// ============================================================================

TEST_CASE("TerrainConfig defaults", "[terrain][config]") {
    TerrainConfig config;

    REQUIRE_THAT(config.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(config.position.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(config.position.z, WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(config.scale.x, WithinAbs(512.0f, 0.001f));
    REQUIRE_THAT(config.scale.y, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(config.scale.z, WithinAbs(512.0f, 0.001f));

    REQUIRE(config.heightmap_path.empty());
    REQUIRE(config.splat_map_path.empty());
    REQUIRE(config.hole_map_path.empty());

    REQUIRE(config.generate_collision == true);
    REQUIRE(config.collision_resolution == 0);
    REQUIRE(config.enable_streaming == true);
    REQUIRE_THAT(config.streaming_distance, WithinAbs(500.0f, 0.001f));
}

TEST_CASE("TerrainConfig custom dimensions", "[terrain][config]") {
    TerrainConfig config;
    config.position = Vec3{100.0f, 0.0f, 200.0f};
    config.scale = Vec3{1024.0f, 200.0f, 1024.0f};

    REQUIRE_THAT(config.position.x, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(config.position.z, WithinAbs(200.0f, 0.001f));
    REQUIRE_THAT(config.scale.x, WithinAbs(1024.0f, 0.001f));
    REQUIRE_THAT(config.scale.y, WithinAbs(200.0f, 0.001f));
}

TEST_CASE("TerrainConfig paths", "[terrain][config]") {
    TerrainConfig config;
    config.heightmap_path = "terrain/heightmap.r16";
    config.splat_map_path = "terrain/splat.png";
    config.hole_map_path = "terrain/holes.png";

    REQUIRE(config.heightmap_path == "terrain/heightmap.r16");
    REQUIRE(config.splat_map_path == "terrain/splat.png");
    REQUIRE(config.hole_map_path == "terrain/holes.png");
}

TEST_CASE("TerrainConfig physics settings", "[terrain][config]") {
    TerrainConfig config;
    config.generate_collision = false;
    config.collision_resolution = 128;

    REQUIRE_FALSE(config.generate_collision);
    REQUIRE(config.collision_resolution == 128);
}

TEST_CASE("TerrainConfig streaming settings", "[terrain][config]") {
    TerrainConfig config;
    config.enable_streaming = true;
    config.streaming_distance = 1000.0f;

    REQUIRE(config.enable_streaming);
    REQUIRE_THAT(config.streaming_distance, WithinAbs(1000.0f, 0.001f));
}

// ============================================================================
// TerrainBrush Tests
// ============================================================================

TEST_CASE("TerrainBrush::Mode enum", "[terrain][brush]") {
    REQUIRE(static_cast<int>(TerrainBrush::Mode::Raise) == 0);
    REQUIRE(static_cast<int>(TerrainBrush::Mode::Lower) == 1);
    REQUIRE(static_cast<int>(TerrainBrush::Mode::Flatten) == 2);
    REQUIRE(static_cast<int>(TerrainBrush::Mode::Smooth) == 3);
    REQUIRE(static_cast<int>(TerrainBrush::Mode::Noise) == 4);
    REQUIRE(static_cast<int>(TerrainBrush::Mode::Paint) == 5);
}

TEST_CASE("TerrainBrush defaults", "[terrain][brush]") {
    TerrainBrush brush;

    REQUIRE(brush.mode == TerrainBrush::Mode::Raise);
    REQUIRE_THAT(brush.radius, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(brush.strength, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(brush.falloff, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(brush.target_height, WithinAbs(0.0f, 0.001f));
    REQUIRE(brush.paint_channel == 0);
}

TEST_CASE("TerrainBrush raise configuration", "[terrain][brush]") {
    TerrainBrush brush;
    brush.mode = TerrainBrush::Mode::Raise;
    brush.radius = 20.0f;
    brush.strength = 0.5f;
    brush.falloff = 0.7f;

    REQUIRE(brush.mode == TerrainBrush::Mode::Raise);
    REQUIRE_THAT(brush.radius, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(brush.strength, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(brush.falloff, WithinAbs(0.7f, 0.001f));
}

TEST_CASE("TerrainBrush flatten configuration", "[terrain][brush]") {
    TerrainBrush brush;
    brush.mode = TerrainBrush::Mode::Flatten;
    brush.target_height = 50.0f;
    brush.radius = 15.0f;

    REQUIRE(brush.mode == TerrainBrush::Mode::Flatten);
    REQUIRE_THAT(brush.target_height, WithinAbs(50.0f, 0.001f));
}

TEST_CASE("TerrainBrush paint configuration", "[terrain][brush]") {
    TerrainBrush brush;
    brush.mode = TerrainBrush::Mode::Paint;
    brush.paint_channel = 2;  // Third texture layer
    brush.radius = 8.0f;
    brush.strength = 0.8f;

    REQUIRE(brush.mode == TerrainBrush::Mode::Paint);
    REQUIRE(brush.paint_channel == 2);
    REQUIRE_THAT(brush.radius, WithinAbs(8.0f, 0.001f));
    REQUIRE_THAT(brush.strength, WithinAbs(0.8f, 0.001f));
}

// ============================================================================
// TerrainComponent Tests
// ============================================================================

TEST_CASE("TerrainComponent defaults", "[terrain][component]") {
    TerrainComponent comp;

    REQUIRE(comp.terrain_id == UINT32_MAX);
    REQUIRE(comp.terrain_ptr == nullptr);
}

TEST_CASE("TerrainComponent with terrain ID", "[terrain][component]") {
    TerrainComponent comp;
    comp.terrain_id = 5;

    REQUIRE(comp.terrain_id == 5);
    REQUIRE(comp.terrain_ptr == nullptr);
}

// ============================================================================
// Terrain Class Tests
// ============================================================================

TEST_CASE("Terrain default state", "[terrain][class]") {
    Terrain terrain;

    REQUIRE_FALSE(terrain.is_valid());
    REQUIRE(terrain.get_physics_body() == UINT32_MAX);
}

TEST_CASE("Terrain create flat", "[terrain][class]") {
    Terrain terrain;

    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 scale{256.0f, 50.0f, 256.0f};

    bool created = terrain.create_flat(position, scale, 65);

    if (created) {
        REQUIRE(terrain.is_valid());
        REQUIRE_THAT(terrain.get_position().x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(terrain.get_scale().x, WithinAbs(256.0f, 0.001f));
    }
}

TEST_CASE("Terrain point on terrain check", "[terrain][class]") {
    Terrain terrain;

    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 scale{256.0f, 50.0f, 256.0f};

    bool created = terrain.create_flat(position, scale, 33);

    if (created && terrain.is_valid()) {
        // Point inside terrain bounds
        REQUIRE(terrain.is_point_on_terrain(128.0f, 128.0f));

        // Point outside terrain bounds
        REQUIRE_FALSE(terrain.is_point_on_terrain(-10.0f, 128.0f));
        REQUIRE_FALSE(terrain.is_point_on_terrain(300.0f, 128.0f));
    }
}

TEST_CASE("Terrain heightmap access", "[terrain][class]") {
    Terrain terrain;

    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 scale{128.0f, 25.0f, 128.0f};

    bool created = terrain.create_flat(position, scale, 33);

    if (created && terrain.is_valid()) {
        const Heightmap& hm = terrain.get_heightmap();
        REQUIRE(hm.is_valid());
        REQUIRE(hm.get_width() > 0);
        REQUIRE(hm.get_height() > 0);
    }
}
