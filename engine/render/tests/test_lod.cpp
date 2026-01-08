#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/lod.hpp>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("LODFadeMode enum", "[render][lod]") {
    REQUIRE(static_cast<int>(LODFadeMode::None) == 0);
    REQUIRE(static_cast<int>(LODFadeMode::CrossFade) == 1);
    REQUIRE(static_cast<int>(LODFadeMode::SpeedTree) == 2);
    REQUIRE(static_cast<int>(LODFadeMode::Dither) == 3);
}

TEST_CASE("LODLevel defaults", "[render][lod]") {
    LODLevel level;

    REQUIRE_FALSE(level.mesh.valid());
    REQUIRE_FALSE(level.material.valid());
    REQUIRE_THAT(level.screen_height_ratio, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(level.transition_width, WithinAbs(0.1f, 0.001f));
    REQUIRE_FALSE(level.shadow_mesh.valid());
    REQUIRE(level.cast_shadows == true);
}

TEST_CASE("LODLevel constructor", "[render][lod]") {
    MeshHandle mesh;
    mesh.id = 42;

    LODLevel level(mesh, 0.5f);

    REQUIRE(level.mesh.valid());
    REQUIRE(level.mesh.id == 42);
    REQUIRE_THAT(level.screen_height_ratio, WithinAbs(0.5f, 0.001f));
}

TEST_CASE("LODLevel custom values", "[render][lod]") {
    LODLevel level;
    level.mesh.id = 1;
    level.material.id = 2;
    level.screen_height_ratio = 0.3f;
    level.transition_width = 0.2f;
    level.shadow_mesh.id = 3;
    level.cast_shadows = false;

    REQUIRE(level.mesh.id == 1);
    REQUIRE(level.material.id == 2);
    REQUIRE_THAT(level.screen_height_ratio, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(level.transition_width, WithinAbs(0.2f, 0.001f));
    REQUIRE(level.shadow_mesh.id == 3);
    REQUIRE(level.cast_shadows == false);
}

TEST_CASE("LODGroup defaults", "[render][lod]") {
    LODGroup group;

    REQUIRE(group.empty());
    REQUIRE(group.level_count() == 0);
    REQUIRE(group.fade_mode == LODFadeMode::Dither);
    REQUIRE_THAT(group.fade_duration, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(group.lod_bias, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(group.cull_distance, WithinAbs(0.0f, 0.001f));
    REQUIRE(group.use_cull_distance == false);
    REQUIRE(group.reduce_animation_at_distance == true);
    REQUIRE_THAT(group.animation_lod_distance, WithinAbs(50.0f, 0.001f));
}

TEST_CASE("LODGroup add_level", "[render][lod]") {
    LODGroup group;

    MeshHandle mesh;
    mesh.id = 1;

    group.add_level(mesh, 0.5f);

    REQUIRE_FALSE(group.empty());
    REQUIRE(group.level_count() == 1);
    REQUIRE(group.levels[0].mesh.id == 1);
    REQUIRE_THAT(group.levels[0].screen_height_ratio, WithinAbs(0.5f, 0.001f));
}

TEST_CASE("LODGroup add_level with LODLevel", "[render][lod]") {
    LODGroup group;

    LODLevel level;
    level.mesh.id = 42;
    level.screen_height_ratio = 0.3f;

    group.add_level(level);

    REQUIRE(group.level_count() == 1);
    REQUIRE(group.levels[0].mesh.id == 42);
}

TEST_CASE("LODGroup multiple levels", "[render][lod]") {
    LODGroup group;

    MeshHandle high, medium, low;
    high.id = 1;
    medium.id = 2;
    low.id = 3;

    group.add_level(high, 0.5f);    // High detail at 50% screen
    group.add_level(medium, 0.3f);  // Medium at 30% screen
    group.add_level(low, 0.1f);     // Low at 10% screen

    REQUIRE(group.level_count() == 3);
    REQUIRE(group.levels[0].mesh.id == 1);
    REQUIRE(group.levels[1].mesh.id == 2);
    REQUIRE(group.levels[2].mesh.id == 3);
}

TEST_CASE("LODGroup custom settings", "[render][lod]") {
    LODGroup group;
    group.fade_mode = LODFadeMode::CrossFade;
    group.fade_duration = 1.0f;
    group.lod_bias = -0.5f;
    group.cull_distance = 100.0f;
    group.use_cull_distance = true;
    group.reduce_animation_at_distance = false;
    group.animation_lod_distance = 30.0f;

    REQUIRE(group.fade_mode == LODFadeMode::CrossFade);
    REQUIRE_THAT(group.fade_duration, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(group.lod_bias, WithinAbs(-0.5f, 0.001f));
    REQUIRE_THAT(group.cull_distance, WithinAbs(100.0f, 0.001f));
    REQUIRE(group.use_cull_distance == true);
    REQUIRE(group.reduce_animation_at_distance == false);
    REQUIRE_THAT(group.animation_lod_distance, WithinAbs(30.0f, 0.001f));
}

TEST_CASE("LODSelectionResult defaults", "[render][lod]") {
    LODSelectionResult result;

    REQUIRE(result.current_lod == 0);
    REQUIRE(result.target_lod == 0);
    REQUIRE_THAT(result.fade_progress, WithinAbs(1.0f, 0.001f));
    REQUIRE(result.is_transitioning == false);
    REQUIRE(result.is_culled == false);
    REQUIRE_THAT(result.screen_ratio, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("LODSelectionResult custom values", "[render][lod]") {
    LODSelectionResult result;
    result.current_lod = 1;
    result.target_lod = 2;
    result.fade_progress = 0.5f;
    result.is_transitioning = true;
    result.is_culled = false;
    result.screen_ratio = 0.25f;

    REQUIRE(result.current_lod == 1);
    REQUIRE(result.target_lod == 2);
    REQUIRE_THAT(result.fade_progress, WithinAbs(0.5f, 0.001f));
    REQUIRE(result.is_transitioning == true);
    REQUIRE_THAT(result.screen_ratio, WithinAbs(0.25f, 0.001f));
}

TEST_CASE("LODSelector defaults", "[render][lod]") {
    LODSelector selector;

    REQUIRE_THAT(selector.get_global_bias(), WithinAbs(0.0f, 0.001f));
    REQUIRE(selector.get_force_lod() == -1);
    REQUIRE(selector.get_max_lod_level() == -1);
}

TEST_CASE("LODSelector global_bias", "[render][lod]") {
    LODSelector selector;

    selector.set_global_bias(0.5f);
    REQUIRE_THAT(selector.get_global_bias(), WithinAbs(0.5f, 0.001f));

    selector.set_global_bias(-0.5f);
    REQUIRE_THAT(selector.get_global_bias(), WithinAbs(-0.5f, 0.001f));
}

TEST_CASE("LODSelector force_lod", "[render][lod]") {
    LODSelector selector;

    selector.set_force_lod(2);
    REQUIRE(selector.get_force_lod() == 2);

    selector.clear_force_lod();
    REQUIRE(selector.get_force_lod() == -1);
}

TEST_CASE("LODSelector max_lod_level", "[render][lod]") {
    LODSelector selector;

    selector.set_max_lod_level(3);
    REQUIRE(selector.get_max_lod_level() == 3);

    selector.set_max_lod_level(-1);
    REQUIRE(selector.get_max_lod_level() == -1);
}

TEST_CASE("LODState defaults", "[render][lod]") {
    LODState state;

    REQUIRE(state.current_lod == 0);
    REQUIRE(state.target_lod == 0);
    REQUIRE_THAT(state.fade_time, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(state.fade_duration, WithinAbs(0.5f, 0.001f));
    REQUIRE(state.is_transitioning == false);
}

TEST_CASE("LODState start_transition", "[render][lod]") {
    LODState state;
    state.current_lod = 0;

    state.start_transition(2, 1.0f);

    REQUIRE(state.target_lod == 2);
    REQUIRE_THAT(state.fade_duration, WithinAbs(1.0f, 0.001f));
    REQUIRE(state.is_transitioning == true);
}

TEST_CASE("LODState get_fade_progress", "[render][lod]") {
    LODState state;

    // Not transitioning - full progress
    REQUIRE_THAT(state.get_fade_progress(), WithinAbs(1.0f, 0.001f));

    // Start transition
    state.start_transition(1, 1.0f);
    REQUIRE_THAT(state.get_fade_progress(), WithinAbs(0.0f, 0.001f));
}

TEST_CASE("LODComponent defaults", "[render][lod]") {
    LODComponent component;

    REQUIRE(component.lod_group.empty());
    REQUIRE(component.enabled == true);
    REQUIRE(component.use_custom_bias == false);
    REQUIRE_THAT(component.custom_bias, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("LODComponent custom settings", "[render][lod]") {
    LODComponent component;
    component.enabled = false;
    component.use_custom_bias = true;
    component.custom_bias = -0.25f;

    REQUIRE(component.enabled == false);
    REQUIRE(component.use_custom_bias == true);
    REQUIRE_THAT(component.custom_bias, WithinAbs(-0.25f, 0.001f));
}

TEST_CASE("LODQualityPreset ultra", "[render][lod]") {
    auto preset = LODQualityPreset::ultra();

    REQUIRE_THAT(preset.global_bias, WithinAbs(-0.5f, 0.01f));
    REQUIRE(preset.max_lod_level == -1);
    REQUIRE(preset.use_crossfade == true);
}

TEST_CASE("LODQualityPreset high", "[render][lod]") {
    auto preset = LODQualityPreset::high();

    REQUIRE_THAT(preset.global_bias, WithinAbs(0.0f, 0.01f));
    REQUIRE(preset.use_crossfade == true);
}

TEST_CASE("LODQualityPreset medium", "[render][lod]") {
    auto preset = LODQualityPreset::medium();

    REQUIRE(preset.global_bias > 0.0f);  // Higher bias = lower detail
    REQUIRE(preset.use_crossfade == true);
}

TEST_CASE("LODQualityPreset low", "[render][lod]") {
    auto preset = LODQualityPreset::low();

    REQUIRE(preset.global_bias > 0.0f);  // Higher bias = lower detail
    REQUIRE(preset.max_lod_level > 0);   // May limit LOD levels
}
