#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/vegetation/foliage.hpp>

using namespace engine::vegetation;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// FoliageLOD Tests
// ============================================================================

TEST_CASE("FoliageLOD defaults", "[vegetation][foliage][lod]") {
    FoliageLOD lod;

    REQUIRE(lod.mesh_id == UINT32_MAX);
    REQUIRE(lod.material_id == UINT32_MAX);
    REQUIRE_THAT(lod.screen_size, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(lod.transition_width, WithinAbs(0.1f, 0.001f));
}

TEST_CASE("FoliageLOD configuration", "[vegetation][foliage][lod]") {
    FoliageLOD lod;
    lod.mesh_id = 5;
    lod.material_id = 10;
    lod.screen_size = 0.05f;
    lod.transition_width = 0.15f;

    REQUIRE(lod.mesh_id == 5);
    REQUIRE(lod.material_id == 10);
    REQUIRE_THAT(lod.screen_size, WithinAbs(0.05f, 0.001f));
    REQUIRE_THAT(lod.transition_width, WithinAbs(0.15f, 0.001f));
}

// ============================================================================
// FoliageBillboard Tests
// ============================================================================

TEST_CASE("FoliageBillboard defaults", "[vegetation][foliage][billboard]") {
    FoliageBillboard billboard;

    REQUIRE(billboard.texture == UINT32_MAX);
    REQUIRE_THAT(billboard.size.x, WithinAbs(4.0f, 0.001f));
    REQUIRE_THAT(billboard.size.y, WithinAbs(6.0f, 0.001f));
    REQUIRE_THAT(billboard.uv_min.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(billboard.uv_min.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(billboard.uv_max.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(billboard.uv_max.y, WithinAbs(1.0f, 0.001f));
    REQUIRE(billboard.rotate_to_camera == true);
    REQUIRE_THAT(billboard.start_distance, WithinAbs(100.0f, 0.001f));
}

TEST_CASE("FoliageBillboard tree configuration", "[vegetation][foliage][billboard]") {
    FoliageBillboard billboard;
    billboard.texture = 42;
    billboard.size = Vec2{8.0f, 12.0f};
    billboard.uv_min = Vec2{0.0f, 0.5f};
    billboard.uv_max = Vec2{0.5f, 1.0f};
    billboard.rotate_to_camera = true;
    billboard.start_distance = 150.0f;

    REQUIRE(billboard.texture == 42);
    REQUIRE_THAT(billboard.size.x, WithinAbs(8.0f, 0.001f));
    REQUIRE_THAT(billboard.size.y, WithinAbs(12.0f, 0.001f));
    REQUIRE_THAT(billboard.uv_min.y, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(billboard.start_distance, WithinAbs(150.0f, 0.001f));
}

// ============================================================================
// FoliageType Tests
// ============================================================================

TEST_CASE("FoliageType defaults", "[vegetation][foliage][type]") {
    FoliageType type;

    REQUIRE(type.name.empty());
    REQUIRE(type.id.empty());
    REQUIRE(type.lods.empty());
    REQUIRE(type.use_billboard == true);
    REQUIRE_THAT(type.min_scale, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(type.max_scale, WithinAbs(1.2f, 0.001f));
    REQUIRE(type.random_rotation == true);
    REQUIRE_THAT(type.min_rotation, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(type.max_rotation, WithinAbs(360.0f, 0.001f));
    REQUIRE(type.align_to_terrain == true);
    REQUIRE_THAT(type.max_slope, WithinAbs(45.0f, 0.001f));
    REQUIRE_THAT(type.terrain_offset, WithinAbs(0.0f, 0.001f));
    REQUIRE(type.has_collision == true);
    REQUIRE_THAT(type.collision_radius, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(type.collision_height, WithinAbs(5.0f, 0.001f));
    REQUIRE(type.affected_by_wind == true);
    REQUIRE_THAT(type.wind_strength, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(type.wind_frequency, WithinAbs(1.0f, 0.001f));
    REQUIRE(type.cast_shadows == true);
    REQUIRE(type.receive_shadows == true);
    REQUIRE_THAT(type.cull_distance, WithinAbs(500.0f, 0.001f));
    REQUIRE_THAT(type.fade_distance, WithinAbs(50.0f, 0.001f));
}

TEST_CASE("FoliageType oak tree configuration", "[vegetation][foliage][type]") {
    FoliageType type;
    type.name = "Oak Tree";
    type.id = "oak_tree";
    type.min_scale = 0.9f;
    type.max_scale = 1.3f;
    type.max_slope = 30.0f;
    type.collision_radius = 1.0f;
    type.collision_height = 8.0f;
    type.wind_strength = 0.2f;
    type.cull_distance = 600.0f;

    REQUIRE(type.name == "Oak Tree");
    REQUIRE(type.id == "oak_tree");
    REQUIRE_THAT(type.min_scale, WithinAbs(0.9f, 0.001f));
    REQUIRE_THAT(type.max_scale, WithinAbs(1.3f, 0.001f));
    REQUIRE_THAT(type.max_slope, WithinAbs(30.0f, 0.001f));
    REQUIRE_THAT(type.collision_radius, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(type.collision_height, WithinAbs(8.0f, 0.001f));
}

TEST_CASE("FoliageType bush configuration", "[vegetation][foliage][type]") {
    FoliageType type;
    type.name = "Bush";
    type.id = "bush_01";
    type.min_scale = 0.7f;
    type.max_scale = 1.1f;
    type.has_collision = false;  // Player can walk through
    type.wind_strength = 0.5f;   // More responsive to wind
    type.cast_shadows = false;   // Optimize performance

    REQUIRE(type.name == "Bush");
    REQUIRE_FALSE(type.has_collision);
    REQUIRE_THAT(type.wind_strength, WithinAbs(0.5f, 0.001f));
    REQUIRE_FALSE(type.cast_shadows);
}

// ============================================================================
// FoliageInstance Tests
// ============================================================================

TEST_CASE("FoliageInstance defaults", "[vegetation][foliage][instance]") {
    FoliageInstance instance;

    REQUIRE_THAT(instance.scale, WithinAbs(1.0f, 0.001f));
    REQUIRE(instance.type_index == 0);
    REQUIRE(instance.random_seed == 0);
    REQUIRE(instance.current_lod == 0);
    REQUIRE_THAT(instance.lod_blend, WithinAbs(0.0f, 0.001f));
    REQUIRE(instance.visible == true);
    REQUIRE_FALSE(instance.use_billboard);
}

TEST_CASE("FoliageInstance configuration", "[vegetation][foliage][instance]") {
    FoliageInstance instance;
    instance.position = Vec3{100.0f, 25.0f, 200.0f};
    instance.rotation = Quat{0.707f, 0.0f, 0.707f, 0.0f};
    instance.scale = 1.1f;
    instance.type_index = 5;
    instance.random_seed = 12345;

    REQUIRE_THAT(instance.position.x, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(instance.position.y, WithinAbs(25.0f, 0.001f));
    REQUIRE_THAT(instance.position.z, WithinAbs(200.0f, 0.001f));
    REQUIRE_THAT(instance.scale, WithinAbs(1.1f, 0.001f));
    REQUIRE(instance.type_index == 5);
    REQUIRE(instance.random_seed == 12345);
}

TEST_CASE("FoliageInstance LOD state", "[vegetation][foliage][instance]") {
    FoliageInstance instance;
    instance.current_lod = 2;
    instance.lod_blend = 0.5f;
    instance.visible = false;
    instance.use_billboard = true;

    REQUIRE(instance.current_lod == 2);
    REQUIRE_THAT(instance.lod_blend, WithinAbs(0.5f, 0.001f));
    REQUIRE_FALSE(instance.visible);
    REQUIRE(instance.use_billboard);
}

// ============================================================================
// FoliagePlacementRule Tests
// ============================================================================

TEST_CASE("FoliagePlacementRule defaults", "[vegetation][foliage][placement]") {
    FoliagePlacementRule rule;

    REQUIRE(rule.type_id.empty());
    REQUIRE_THAT(rule.density, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(rule.min_height, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(rule.max_height, WithinAbs(1000.0f, 0.001f));
    REQUIRE_THAT(rule.min_slope, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(rule.max_slope, WithinAbs(30.0f, 0.001f));
    REQUIRE_THAT(rule.noise_scale, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(rule.noise_threshold, WithinAbs(0.3f, 0.001f));
    REQUIRE(rule.enable_clustering == true);
    REQUIRE_THAT(rule.cluster_radius, WithinAbs(5.0f, 0.001f));
    REQUIRE(rule.cluster_count == 3);
    REQUIRE(rule.exclusion_zones.empty());
}

TEST_CASE("FoliagePlacementRule forest configuration", "[vegetation][foliage][placement]") {
    FoliagePlacementRule rule;
    rule.type_id = "oak_tree";
    rule.density = 0.05f;
    rule.min_height = 10.0f;
    rule.max_height = 500.0f;
    rule.max_slope = 25.0f;
    rule.enable_clustering = true;
    rule.cluster_radius = 10.0f;
    rule.cluster_count = 5;

    REQUIRE(rule.type_id == "oak_tree");
    REQUIRE_THAT(rule.density, WithinAbs(0.05f, 0.001f));
    REQUIRE_THAT(rule.min_height, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(rule.max_height, WithinAbs(500.0f, 0.001f));
    REQUIRE_THAT(rule.max_slope, WithinAbs(25.0f, 0.001f));
}

TEST_CASE("FoliagePlacementRule alpine configuration", "[vegetation][foliage][placement]") {
    FoliagePlacementRule rule;
    rule.type_id = "pine_tree";
    rule.density = 0.03f;
    rule.min_height = 200.0f;
    rule.max_height = 800.0f;
    rule.min_slope = 0.0f;
    rule.max_slope = 35.0f;
    rule.noise_scale = 15.0f;
    rule.noise_threshold = 0.4f;

    REQUIRE(rule.type_id == "pine_tree");
    REQUIRE_THAT(rule.min_height, WithinAbs(200.0f, 0.001f));
    REQUIRE_THAT(rule.max_height, WithinAbs(800.0f, 0.001f));
    REQUIRE_THAT(rule.noise_scale, WithinAbs(15.0f, 0.001f));
}

TEST_CASE("FoliagePlacementRule exclusion zones", "[vegetation][foliage][placement]") {
    FoliagePlacementRule rule;
    rule.type_id = "bush";
    rule.density = 0.2f;

    AABB zone1;
    zone1.min = Vec3{50.0f, 0.0f, 50.0f};
    zone1.max = Vec3{100.0f, 10.0f, 100.0f};

    AABB zone2;
    zone2.min = Vec3{200.0f, 0.0f, 200.0f};
    zone2.max = Vec3{250.0f, 10.0f, 250.0f};

    rule.exclusion_zones.push_back(zone1);
    rule.exclusion_zones.push_back(zone2);

    REQUIRE(rule.exclusion_zones.size() == 2);
}

// ============================================================================
// FoliageChunk Tests
// ============================================================================

TEST_CASE("FoliageChunk defaults", "[vegetation][foliage][chunk]") {
    FoliageChunk chunk;

    REQUIRE_FALSE(chunk.visible);
    REQUIRE_THAT(chunk.distance_to_camera, WithinAbs(0.0f, 0.001f));
    REQUIRE(chunk.instance_indices.empty());
}

TEST_CASE("FoliageChunk configuration", "[vegetation][foliage][chunk]") {
    FoliageChunk chunk;
    chunk.bounds.min = Vec3{0.0f, 0.0f, 0.0f};
    chunk.bounds.max = Vec3{32.0f, 50.0f, 32.0f};
    chunk.visible = true;
    chunk.distance_to_camera = 75.0f;
    chunk.instance_indices = {0, 1, 2, 5, 8, 10};

    REQUIRE(chunk.visible);
    REQUIRE_THAT(chunk.distance_to_camera, WithinAbs(75.0f, 0.001f));
    REQUIRE(chunk.instance_indices.size() == 6);
}

// ============================================================================
// FoliageSettings Tests
// ============================================================================

TEST_CASE("FoliageSettings defaults", "[vegetation][foliage][settings]") {
    FoliageSettings settings;

    REQUIRE(settings.max_instances == 50000);
    REQUIRE_THAT(settings.lod_bias, WithinAbs(0.0f, 0.001f));
    REQUIRE(settings.use_gpu_culling == true);
    REQUIRE(settings.enable_billboards == true);
    REQUIRE_THAT(settings.billboard_start_distance, WithinAbs(100.0f, 0.001f));
    REQUIRE(settings.cast_shadows == true);
    REQUIRE(settings.shadow_lod == 1);
    REQUIRE(settings.enable_wind == true);
    REQUIRE_THAT(settings.wind_direction.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(settings.wind_direction.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(settings.wind_speed, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(settings.wind_strength, WithinAbs(0.3f, 0.001f));
    REQUIRE(settings.chunk_size == 32);
    REQUIRE_THAT(settings.update_distance, WithinAbs(20.0f, 0.001f));
}

TEST_CASE("FoliageSettings high quality", "[vegetation][foliage][settings]") {
    FoliageSettings settings;
    settings.max_instances = 100000;
    settings.use_gpu_culling = true;
    settings.cast_shadows = true;
    settings.shadow_lod = 0;  // Use highest LOD for shadows
    settings.lod_bias = -0.5f;  // Bias towards higher quality

    REQUIRE(settings.max_instances == 100000);
    REQUIRE(settings.shadow_lod == 0);
    REQUIRE_THAT(settings.lod_bias, WithinAbs(-0.5f, 0.001f));
}

TEST_CASE("FoliageSettings low quality", "[vegetation][foliage][settings]") {
    FoliageSettings settings;
    settings.max_instances = 20000;
    settings.cast_shadows = false;
    settings.enable_billboards = true;
    settings.billboard_start_distance = 50.0f;  // Use billboards sooner
    settings.lod_bias = 1.0f;  // Bias towards lower quality

    REQUIRE(settings.max_instances == 20000);
    REQUIRE_FALSE(settings.cast_shadows);
    REQUIRE_THAT(settings.billboard_start_distance, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(settings.lod_bias, WithinAbs(1.0f, 0.001f));
}

// ============================================================================
// FoliageSystem::Stats Tests
// ============================================================================

TEST_CASE("FoliageSystem Stats defaults", "[vegetation][foliage][stats]") {
    FoliageSystem::Stats stats;

    REQUIRE(stats.total_instances == 0);
    REQUIRE(stats.visible_instances == 0);
    REQUIRE(stats.billboard_instances == 0);
    REQUIRE(stats.total_types == 0);
    REQUIRE(stats.visible_chunks == 0);
}

// ============================================================================
// FoliageComponent Tests
// ============================================================================

TEST_CASE("FoliageComponent defaults", "[vegetation][foliage][component]") {
    FoliageComponent comp;

    REQUIRE(comp.type_id.empty());
    REQUIRE_THAT(comp.scale, WithinAbs(1.0f, 0.001f));
    REQUIRE(comp.cast_shadows == true);
    REQUIRE(comp.instance_index == UINT32_MAX);
}

TEST_CASE("FoliageComponent configuration", "[vegetation][foliage][component]") {
    FoliageComponent comp;
    comp.type_id = "oak_tree";
    comp.scale = 1.2f;
    comp.cast_shadows = true;
    comp.instance_index = 42;

    REQUIRE(comp.type_id == "oak_tree");
    REQUIRE_THAT(comp.scale, WithinAbs(1.2f, 0.001f));
    REQUIRE(comp.cast_shadows);
    REQUIRE(comp.instance_index == 42);
}
