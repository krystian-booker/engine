#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/vegetation/grass.hpp>

using namespace engine::vegetation;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// GrassInstance Tests
// ============================================================================

TEST_CASE("GrassInstance defaults", "[vegetation][grass]") {
    GrassInstance instance{};

    REQUIRE_THAT(instance.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(instance.position.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(instance.position.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(instance.rotation, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(instance.scale, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(instance.bend, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("GrassInstance configuration", "[vegetation][grass]") {
    GrassInstance instance;
    instance.position = Vec3{10.0f, 5.0f, 20.0f};
    instance.rotation = 1.57f;  // ~90 degrees
    instance.scale = 1.2f;
    instance.bend = 0.3f;
    instance.random = 0.5f;

    REQUIRE_THAT(instance.position.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(instance.position.y, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(instance.position.z, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(instance.rotation, WithinAbs(1.57f, 0.001f));
    REQUIRE_THAT(instance.scale, WithinAbs(1.2f, 0.001f));
    REQUIRE_THAT(instance.bend, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(instance.random, WithinAbs(0.5f, 0.001f));
}

// ============================================================================
// GrassWindSettings Tests
// ============================================================================

TEST_CASE("GrassWindSettings defaults", "[vegetation][grass][wind]") {
    GrassWindSettings wind;

    REQUIRE_THAT(wind.direction.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(wind.direction.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(wind.speed, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(wind.strength, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(wind.frequency, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(wind.turbulence, WithinAbs(0.5f, 0.001f));
    REQUIRE(wind.enable_gusts == true);
    REQUIRE_THAT(wind.gust_strength, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(wind.gust_frequency, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(wind.gust_speed, WithinAbs(3.0f, 0.001f));
}

TEST_CASE("GrassWindSettings stormy configuration", "[vegetation][grass][wind]") {
    GrassWindSettings wind;
    wind.direction = Vec2{0.707f, 0.707f};  // NE
    wind.speed = 5.0f;
    wind.strength = 0.8f;
    wind.frequency = 4.0f;
    wind.turbulence = 0.9f;
    wind.enable_gusts = true;
    wind.gust_strength = 1.0f;
    wind.gust_frequency = 0.3f;
    wind.gust_speed = 6.0f;

    REQUIRE_THAT(wind.direction.x, WithinAbs(0.707f, 0.001f));
    REQUIRE_THAT(wind.speed, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(wind.strength, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(wind.turbulence, WithinAbs(0.9f, 0.001f));
}

TEST_CASE("GrassWindSettings calm configuration", "[vegetation][grass][wind]") {
    GrassWindSettings wind;
    wind.speed = 0.2f;
    wind.strength = 0.1f;
    wind.enable_gusts = false;
    wind.turbulence = 0.1f;

    REQUIRE_THAT(wind.speed, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(wind.strength, WithinAbs(0.1f, 0.001f));
    REQUIRE_FALSE(wind.enable_gusts);
}

// ============================================================================
// GrassSettings Tests
// ============================================================================

TEST_CASE("GrassSettings defaults", "[vegetation][grass][settings]") {
    GrassSettings settings;

    // Density
    REQUIRE_THAT(settings.density, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(settings.density_variance, WithinAbs(0.3f, 0.001f));

    // Blade shape
    REQUIRE_THAT(settings.blade_width, WithinAbs(0.03f, 0.001f));
    REQUIRE_THAT(settings.blade_width_variance, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(settings.blade_height, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(settings.blade_height_variance, WithinAbs(0.4f, 0.001f));
    REQUIRE(settings.blade_segments == 3);

    // Colors
    REQUIRE_THAT(settings.base_color.x, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(settings.base_color.y, WithinAbs(0.4f, 0.001f));
    REQUIRE_THAT(settings.base_color.z, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(settings.tip_color.x, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(settings.tip_color.y, WithinAbs(0.6f, 0.001f));
    REQUIRE_THAT(settings.tip_color.z, WithinAbs(0.15f, 0.001f));
    REQUIRE_THAT(settings.color_variance, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(settings.dry_amount, WithinAbs(0.1f, 0.001f));

    // LOD
    REQUIRE_THAT(settings.lod_start_distance, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(settings.lod_end_distance, WithinAbs(60.0f, 0.001f));
    REQUIRE_THAT(settings.cull_distance, WithinAbs(80.0f, 0.001f));
    REQUIRE(settings.use_distance_fade == true);
    REQUIRE_THAT(settings.fade_start_distance, WithinAbs(50.0f, 0.001f));

    // Interaction
    REQUIRE(settings.enable_interaction == true);
    REQUIRE_THAT(settings.interaction_radius, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(settings.interaction_strength, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(settings.interaction_recovery, WithinAbs(2.0f, 0.001f));

    // Rendering
    REQUIRE_FALSE(settings.cast_shadows);
    REQUIRE(settings.receive_shadows == true);
    REQUIRE(settings.use_alpha_cutoff == true);
    REQUIRE_THAT(settings.alpha_cutoff, WithinAbs(0.5f, 0.001f));

    // Performance
    REQUIRE(settings.max_instances == 100000);
    REQUIRE(settings.chunk_size == 16);
}

TEST_CASE("GrassSettings dense meadow", "[vegetation][grass][settings]") {
    GrassSettings settings;
    settings.density = 100.0f;
    settings.blade_height = 0.8f;
    settings.blade_height_variance = 0.5f;
    settings.base_color = Vec3{0.15f, 0.5f, 0.1f};
    settings.tip_color = Vec3{0.3f, 0.7f, 0.2f};

    REQUIRE_THAT(settings.density, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(settings.blade_height, WithinAbs(0.8f, 0.001f));
}

TEST_CASE("GrassSettings sparse dry grass", "[vegetation][grass][settings]") {
    GrassSettings settings;
    settings.density = 20.0f;
    settings.blade_height = 0.3f;
    settings.dry_amount = 0.7f;
    settings.dry_color = Vec3{0.5f, 0.4f, 0.15f};

    REQUIRE_THAT(settings.density, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(settings.dry_amount, WithinAbs(0.7f, 0.001f));
}

// ============================================================================
// GrassChunk Tests
// ============================================================================

TEST_CASE("GrassChunk defaults", "[vegetation][grass][chunk]") {
    GrassChunk chunk;

    REQUIRE(chunk.instance_buffer == UINT32_MAX);
    REQUIRE_FALSE(chunk.visible);
    REQUIRE_FALSE(chunk.dirty);
    REQUIRE_THAT(chunk.distance_to_camera, WithinAbs(0.0f, 0.001f));
    REQUIRE(chunk.lod == 0);
    REQUIRE(chunk.instances.empty());
}

TEST_CASE("GrassChunk configuration", "[vegetation][grass][chunk]") {
    GrassChunk chunk;
    chunk.position = Vec2{64.0f, 128.0f};
    chunk.size = 16.0f;
    chunk.visible = true;
    chunk.distance_to_camera = 50.0f;
    chunk.lod = 1;

    REQUIRE_THAT(chunk.position.x, WithinAbs(64.0f, 0.001f));
    REQUIRE_THAT(chunk.position.y, WithinAbs(128.0f, 0.001f));
    REQUIRE_THAT(chunk.size, WithinAbs(16.0f, 0.001f));
    REQUIRE(chunk.visible);
    REQUIRE_THAT(chunk.distance_to_camera, WithinAbs(50.0f, 0.001f));
    REQUIRE(chunk.lod == 1);
}

// ============================================================================
// GrassInteractor Tests
// ============================================================================

TEST_CASE("GrassInteractor defaults", "[vegetation][grass][interactor]") {
    GrassInteractor interactor;

    REQUIRE_THAT(interactor.radius, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(interactor.strength, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("GrassInteractor player configuration", "[vegetation][grass][interactor]") {
    GrassInteractor interactor;
    interactor.position = Vec3{100.0f, 5.0f, 200.0f};
    interactor.velocity = Vec3{3.0f, 0.0f, 2.0f};
    interactor.radius = 0.5f;
    interactor.strength = 1.5f;

    REQUIRE_THAT(interactor.position.x, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(interactor.position.y, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(interactor.position.z, WithinAbs(200.0f, 0.001f));
    REQUIRE_THAT(interactor.velocity.x, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(interactor.radius, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(interactor.strength, WithinAbs(1.5f, 0.001f));
}

TEST_CASE("GrassInteractor vehicle configuration", "[vegetation][grass][interactor]") {
    GrassInteractor interactor;
    interactor.position = Vec3{50.0f, 2.0f, 75.0f};
    interactor.velocity = Vec3{10.0f, 0.0f, 0.0f};
    interactor.radius = 2.0f;      // Larger radius for vehicle
    interactor.strength = 3.0f;    // Stronger push

    REQUIRE_THAT(interactor.radius, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(interactor.strength, WithinAbs(3.0f, 0.001f));
}

// ============================================================================
// GrassSystem::Stats Tests
// ============================================================================

TEST_CASE("GrassSystem Stats defaults", "[vegetation][grass][stats]") {
    GrassSystem::Stats stats;

    REQUIRE(stats.total_instances == 0);
    REQUIRE(stats.visible_instances == 0);
    REQUIRE(stats.visible_chunks == 0);
    REQUIRE(stats.total_chunks == 0);
}

// ============================================================================
// GrassComponent Tests
// ============================================================================

TEST_CASE("GrassComponent defaults", "[vegetation][grass][component]") {
    GrassComponent comp;

    REQUIRE(comp.auto_generate == true);
    REQUIRE(comp.density_map_path.empty());
    // settings member has default GrassSettings values
}

TEST_CASE("GrassComponent configuration", "[vegetation][grass][component]") {
    GrassComponent comp;
    comp.settings.density = 75.0f;
    comp.settings.blade_height = 0.6f;
    comp.auto_generate = false;
    comp.density_map_path = "terrain/grass_density.png";

    REQUIRE_THAT(comp.settings.density, WithinAbs(75.0f, 0.001f));
    REQUIRE_THAT(comp.settings.blade_height, WithinAbs(0.6f, 0.001f));
    REQUIRE_FALSE(comp.auto_generate);
    REQUIRE(comp.density_map_path == "terrain/grass_density.png");
}
