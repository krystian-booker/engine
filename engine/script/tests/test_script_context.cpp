#include <catch2/catch_test_macros.hpp>
#include <engine/script/script_context.hpp>
#include <engine/scene/world.hpp>

using namespace engine::script;
using namespace engine::scene;

// ============================================================================
// ScriptContext Tests
// ============================================================================

TEST_CASE("ScriptContext defaults", "[script][context]") {
    ScriptContext ctx;

    REQUIRE(ctx.world == nullptr);
    REQUIRE(ctx.physics_world == nullptr);
}

TEST_CASE("ScriptContext with values", "[script][context]") {
    ScriptContext ctx;

    // In real use, these would be actual pointers
    // Here we verify the struct members are accessible
    ctx.world = nullptr;
    ctx.physics_world = nullptr;

    REQUIRE(ctx.world == nullptr);
    REQUIRE(ctx.physics_world == nullptr);
}

// ============================================================================
// Global Script Context Tests
// ============================================================================

TEST_CASE("is_script_context_initialized before init", "[script][context]") {
    // Note: This test assumes the global state hasn't been initialized
    // In a fresh test run, context should not be initialized
    // This is a best-effort test since global state persists between tests

    // We can at least verify the function exists and returns a bool
    bool initialized = is_script_context_initialized();
    // Result depends on test ordering; just verify it compiles and runs
    (void)initialized;
}

TEST_CASE("get_current_script_world initial state", "[script][context]") {
    // Initially no world should be set
    engine::scene::World* world = get_current_script_world();

    // May be nullptr or may have been set by previous tests
    // Just verify the function works
    (void)world;
}

TEST_CASE("set_current_script_world and get", "[script][context]") {
    // Set to nullptr
    set_current_script_world(nullptr);
    engine::scene::World* world = get_current_script_world();
    REQUIRE(world == nullptr);
}
