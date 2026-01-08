#include <catch2/catch_test_macros.hpp>
#include <engine/script/script_component.hpp>

using namespace engine::script;

// ============================================================================
// ScriptComponent Tests
// ============================================================================

TEST_CASE("ScriptComponent default constructor", "[script][component]") {
    ScriptComponent comp;

    REQUIRE(comp.script_path.empty());
    REQUIRE_FALSE(comp.loaded);
    REQUIRE(comp.enabled == true);
    REQUIRE(comp.properties.empty());
}

TEST_CASE("ScriptComponent with path", "[script][component]") {
    ScriptComponent comp("scripts/player.lua");

    REQUIRE(comp.script_path == "scripts/player.lua");
    REQUIRE_FALSE(comp.loaded);
    REQUIRE(comp.enabled == true);
}

TEST_CASE("ScriptComponent properties map", "[script][component]") {
    ScriptComponent comp;
    comp.script_path = "scripts/enemy.lua";

    // Properties map should be accessible
    REQUIRE(comp.properties.empty());

    // In real use, properties would be sol::object values
    // Here we just verify the map is accessible
}

TEST_CASE("ScriptComponent enabled state", "[script][component]") {
    ScriptComponent comp("scripts/test.lua");

    REQUIRE(comp.enabled);

    comp.enabled = false;
    REQUIRE_FALSE(comp.enabled);

    comp.enabled = true;
    REQUIRE(comp.enabled);
}

TEST_CASE("ScriptComponent loaded state", "[script][component]") {
    ScriptComponent comp("scripts/test.lua");

    REQUIRE_FALSE(comp.loaded);

    // In real use, this would be set by the script system
    comp.loaded = true;
    REQUIRE(comp.loaded);
}

TEST_CASE("ScriptComponent various paths", "[script][component]") {
    SECTION("Relative path") {
        ScriptComponent comp("scripts/npc/merchant.lua");
        REQUIRE(comp.script_path == "scripts/npc/merchant.lua");
    }

    SECTION("Simple filename") {
        ScriptComponent comp("main.lua");
        REQUIRE(comp.script_path == "main.lua");
    }

    SECTION("Deep nested path") {
        ScriptComponent comp("game/entities/enemies/boss/final_boss.lua");
        REQUIRE(comp.script_path == "game/entities/enemies/boss/final_boss.lua");
    }
}
