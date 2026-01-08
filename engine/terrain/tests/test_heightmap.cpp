#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/terrain/heightmap.hpp>

using namespace engine::terrain;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// HeightmapFormat Tests
// ============================================================================

TEST_CASE("HeightmapFormat enum", "[terrain][heightmap]") {
    REQUIRE(static_cast<uint8_t>(HeightmapFormat::R8) == 0);
    REQUIRE(static_cast<uint8_t>(HeightmapFormat::R16) == 1);
    REQUIRE(static_cast<uint8_t>(HeightmapFormat::R32F) == 2);
    REQUIRE(static_cast<uint8_t>(HeightmapFormat::Raw16) == 3);
}

// ============================================================================
// HeightmapFilter Tests
// ============================================================================

TEST_CASE("HeightmapFilter enum", "[terrain][heightmap]") {
    REQUIRE(static_cast<uint8_t>(HeightmapFilter::Nearest) == 0);
    REQUIRE(static_cast<uint8_t>(HeightmapFilter::Bilinear) == 1);
    REQUIRE(static_cast<uint8_t>(HeightmapFilter::Bicubic) == 2);
}

// ============================================================================
// Heightmap Tests
// ============================================================================

TEST_CASE("Heightmap default state", "[terrain][heightmap]") {
    Heightmap heightmap;

    REQUIRE(heightmap.get_width() == 0);
    REQUIRE(heightmap.get_height() == 0);
    REQUIRE_FALSE(heightmap.is_valid());
    REQUIRE(heightmap.get_data().empty());
}

TEST_CASE("Heightmap generate flat", "[terrain][heightmap]") {
    Heightmap heightmap;
    heightmap.generate_flat(64, 64, 0.5f);

    REQUIRE(heightmap.get_width() == 64);
    REQUIRE(heightmap.get_height() == 64);
    REQUIRE(heightmap.is_valid());
    REQUIRE(heightmap.get_data().size() == 64 * 64);

    // All heights should be 0.5f
    REQUIRE_THAT(heightmap.get_height(0, 0), WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(heightmap.get_height(32, 32), WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(heightmap.get_height(63, 63), WithinAbs(0.5f, 0.001f));
}

TEST_CASE("Heightmap set/get height", "[terrain][heightmap]") {
    Heightmap heightmap;
    heightmap.generate_flat(32, 32, 0.0f);

    heightmap.set_height(10, 10, 0.75f);
    heightmap.set_height(20, 20, 1.0f);

    REQUIRE_THAT(heightmap.get_height(10, 10), WithinAbs(0.75f, 0.001f));
    REQUIRE_THAT(heightmap.get_height(20, 20), WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(heightmap.get_height(0, 0), WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Heightmap bounds", "[terrain][heightmap]") {
    Heightmap heightmap;
    heightmap.generate_flat(64, 64, 0.5f);

    heightmap.set_height(0, 0, 0.0f);
    heightmap.set_height(63, 63, 1.0f);
    heightmap.recalculate_bounds();

    REQUIRE_THAT(heightmap.get_min_height(), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(heightmap.get_max_height(), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("Heightmap sample world", "[terrain][heightmap]") {
    Heightmap heightmap;
    heightmap.generate_flat(65, 65, 0.5f);

    Vec3 terrain_scale{512.0f, 100.0f, 512.0f};

    // Sample at terrain center should return 0.5
    float height = heightmap.sample_world(256.0f, 256.0f, terrain_scale);
    REQUIRE_THAT(height, WithinAbs(0.5f, 0.001f));
}

// ============================================================================
// SplatMap Tests
// ============================================================================

TEST_CASE("SplatMap default state", "[terrain][splatmap]") {
    SplatMap splatmap;

    REQUIRE(splatmap.get_width() == 0);
    REQUIRE(splatmap.get_height() == 0);
    REQUIRE(splatmap.get_channels() == 4);
    REQUIRE_FALSE(splatmap.is_valid());
}

TEST_CASE("SplatMap sampling", "[terrain][splatmap]") {
    SplatMap splatmap;
    // Note: Would need to generate/load data for full test
    // This tests the interface exists
    REQUIRE(splatmap.get_channels() == 4);
}

// ============================================================================
// HoleMap Tests
// ============================================================================

TEST_CASE("HoleMap default state", "[terrain][holemap]") {
    HoleMap holemap;

    REQUIRE(holemap.get_width() == 0);
    REQUIRE(holemap.get_height() == 0);
}

TEST_CASE("HoleMap generate", "[terrain][holemap]") {
    HoleMap holemap;
    holemap.generate(32, 32, false);

    REQUIRE(holemap.get_width() == 32);
    REQUIRE(holemap.get_height() == 32);

    // All should be non-holes initially
    REQUIRE_FALSE(holemap.is_hole_at(0, 0));
    REQUIRE_FALSE(holemap.is_hole_at(16, 16));
    REQUIRE_FALSE(holemap.is_hole_at(31, 31));
}

TEST_CASE("HoleMap set hole", "[terrain][holemap]") {
    HoleMap holemap;
    holemap.generate(32, 32, false);

    holemap.set_hole(10, 10, true);
    holemap.set_hole(20, 20, true);

    REQUIRE(holemap.is_hole_at(10, 10));
    REQUIRE(holemap.is_hole_at(20, 20));
    REQUIRE_FALSE(holemap.is_hole_at(15, 15));
}

TEST_CASE("HoleMap generate with holes", "[terrain][holemap]") {
    HoleMap holemap;
    holemap.generate(32, 32, true);  // All holes

    REQUIRE(holemap.is_hole_at(0, 0));
    REQUIRE(holemap.is_hole_at(16, 16));
    REQUIRE(holemap.is_hole_at(31, 31));

    // Clear a hole
    holemap.set_hole(16, 16, false);
    REQUIRE_FALSE(holemap.is_hole_at(16, 16));
}
