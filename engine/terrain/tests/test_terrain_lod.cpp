#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/terrain/terrain_lod.hpp>

using namespace engine::terrain;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// TerrainLODSettings Tests
// ============================================================================

TEST_CASE("TerrainLODSettings defaults", "[terrain][lod]") {
    TerrainLODSettings settings;

    REQUIRE(settings.num_lods == 4);
    REQUIRE_THAT(settings.lod_distance_ratio, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(settings.base_lod_distance, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(settings.morph_range, WithinAbs(0.2f, 0.001f));
    REQUIRE(settings.use_geomorphing == true);
}

TEST_CASE("TerrainLODSettings custom values", "[terrain][lod]") {
    TerrainLODSettings settings;
    settings.num_lods = 6;
    settings.lod_distance_ratio = 3.0f;
    settings.base_lod_distance = 100.0f;
    settings.morph_range = 0.3f;
    settings.use_geomorphing = false;

    REQUIRE(settings.num_lods == 6);
    REQUIRE_THAT(settings.lod_distance_ratio, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(settings.base_lod_distance, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(settings.morph_range, WithinAbs(0.3f, 0.001f));
    REQUIRE_FALSE(settings.use_geomorphing);
}

// ============================================================================
// ChunkLOD Tests
// ============================================================================

TEST_CASE("ChunkLOD defaults", "[terrain][lod]") {
    ChunkLOD lod;

    REQUIRE(lod.lod_level == 0);
    REQUIRE_THAT(lod.morph_factor, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(lod.distance_to_camera, WithinAbs(0.0f, 0.001f));
    REQUIRE(lod.north_lod == 0);
    REQUIRE(lod.south_lod == 0);
    REQUIRE(lod.east_lod == 0);
    REQUIRE(lod.west_lod == 0);
    REQUIRE_FALSE(lod.needs_stitch);
}

TEST_CASE("ChunkLOD edge transitions", "[terrain][lod]") {
    ChunkLOD lod;
    lod.lod_level = 2;
    lod.north_lod = 1;  // Higher detail neighbor
    lod.south_lod = 3;  // Lower detail neighbor
    lod.east_lod = 2;   // Same detail
    lod.west_lod = 2;   // Same detail
    lod.needs_stitch = true;

    REQUIRE(lod.lod_level == 2);
    REQUIRE(lod.north_lod == 1);
    REQUIRE(lod.south_lod == 3);
    REQUIRE(lod.needs_stitch);
}

// ============================================================================
// TerrainChunk Tests
// ============================================================================

TEST_CASE("TerrainChunk defaults", "[terrain][chunk]") {
    TerrainChunk chunk;

    REQUIRE(chunk.grid_x == 0);
    REQUIRE(chunk.grid_z == 0);
    REQUIRE(chunk.mesh_id == UINT32_MAX);
    REQUIRE(chunk.index_offset == 0);
    REQUIRE(chunk.index_count == 0);
    REQUIRE(chunk.visible == true);
    REQUIRE(chunk.in_frustum == true);
}

TEST_CASE("TerrainChunk grid position", "[terrain][chunk]") {
    TerrainChunk chunk;
    chunk.grid_x = 5;
    chunk.grid_z = 10;
    chunk.center = Vec3{80.0f, 50.0f, 160.0f};

    REQUIRE(chunk.grid_x == 5);
    REQUIRE(chunk.grid_z == 10);
    REQUIRE_THAT(chunk.center.x, WithinAbs(80.0f, 0.001f));
    REQUIRE_THAT(chunk.center.y, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(chunk.center.z, WithinAbs(160.0f, 0.001f));
}

TEST_CASE("TerrainChunk visibility", "[terrain][chunk]") {
    TerrainChunk chunk;
    chunk.visible = false;
    chunk.in_frustum = false;
    chunk.lod.lod_level = 3;
    chunk.lod.distance_to_camera = 500.0f;

    REQUIRE_FALSE(chunk.visible);
    REQUIRE_FALSE(chunk.in_frustum);
    REQUIRE(chunk.lod.lod_level == 3);
    REQUIRE_THAT(chunk.lod.distance_to_camera, WithinAbs(500.0f, 0.001f));
}

// ============================================================================
// TerrainLODSelector Tests
// ============================================================================

TEST_CASE("TerrainLODSelector default settings", "[terrain][lod]") {
    TerrainLODSelector selector;
    const auto& settings = selector.get_settings();

    REQUIRE(settings.num_lods == 4);
    REQUIRE_THAT(settings.base_lod_distance, WithinAbs(50.0f, 0.001f));
}

TEST_CASE("TerrainLODSelector set settings", "[terrain][lod]") {
    TerrainLODSelector selector;

    TerrainLODSettings settings;
    settings.num_lods = 5;
    settings.base_lod_distance = 75.0f;
    selector.set_settings(settings);

    const auto& result = selector.get_settings();
    REQUIRE(result.num_lods == 5);
    REQUIRE_THAT(result.base_lod_distance, WithinAbs(75.0f, 0.001f));
}

TEST_CASE("TerrainLODSelector get LOD for distance", "[terrain][lod]") {
    TerrainLODSelector selector;

    TerrainLODSettings settings;
    settings.num_lods = 4;
    settings.base_lod_distance = 50.0f;
    settings.lod_distance_ratio = 2.0f;
    selector.set_settings(settings);

    // LOD 0 at close distance
    REQUIRE(selector.get_lod_for_distance(25.0f) == 0);

    // LOD 1 at medium distance
    REQUIRE(selector.get_lod_for_distance(75.0f) == 1);

    // Higher LODs at greater distances
    REQUIRE(selector.get_lod_for_distance(150.0f) >= 2);
}

// ============================================================================
// QuadtreeNode Tests
// ============================================================================

TEST_CASE("QuadtreeNode defaults", "[terrain][quadtree]") {
    QuadtreeNode node;

    REQUIRE(node.depth == 0);
    REQUIRE(node.lod == 0);
    REQUIRE(node.is_leaf == true);
    REQUIRE(node.chunk_index == UINT32_MAX);
    REQUIRE_FALSE(node.has_children());
}

TEST_CASE("QuadtreeNode children", "[terrain][quadtree]") {
    QuadtreeNode node;
    node.is_leaf = false;
    node.depth = 1;

    // Create children
    for (int i = 0; i < 4; ++i) {
        node.children[i] = std::make_unique<QuadtreeNode>();
        node.children[i]->depth = 2;
        node.children[i]->is_leaf = true;
    }

    REQUIRE(node.has_children());
    REQUIRE(node.children[0]->depth == 2);
    REQUIRE(node.children[3]->is_leaf);
}

// ============================================================================
// TerrainQuadtree Tests
// ============================================================================

TEST_CASE("TerrainQuadtree default state", "[terrain][quadtree]") {
    TerrainQuadtree quadtree;

    REQUIRE(quadtree.get_root() == nullptr);
}

TEST_CASE("TerrainQuadtree build", "[terrain][quadtree]") {
    TerrainQuadtree quadtree;

    AABB bounds;
    bounds.min = Vec3{0.0f, 0.0f, 0.0f};
    bounds.max = Vec3{512.0f, 100.0f, 512.0f};

    quadtree.build(bounds, 4);

    REQUIRE(quadtree.get_root() != nullptr);
    REQUIRE(quadtree.get_root()->depth == 0);
}

TEST_CASE("TerrainQuadtree get leaves", "[terrain][quadtree]") {
    TerrainQuadtree quadtree;

    AABB bounds;
    bounds.min = Vec3{0.0f, 0.0f, 0.0f};
    bounds.max = Vec3{256.0f, 50.0f, 256.0f};

    quadtree.build(bounds, 2);

    std::vector<const QuadtreeNode*> leaves;
    quadtree.get_leaves(leaves);

    // Should have at least one leaf node
    REQUIRE(!leaves.empty());

    for (const auto* leaf : leaves) {
        REQUIRE(leaf->is_leaf);
    }
}
