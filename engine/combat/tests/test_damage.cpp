#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/combat/damage.hpp>

using namespace engine::combat;
using namespace engine::scene;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// DamageInfo Tests
// ============================================================================

TEST_CASE("DamageInfo defaults", "[combat][damage]") {
    DamageInfo info;

    REQUIRE(info.source == NullEntity);
    REQUIRE(info.target == NullEntity);
    REQUIRE_THAT(info.raw_damage, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(info.final_damage, WithinAbs(0.0f, 0.001f));
    REQUIRE(info.damage_type == "physical");
    REQUIRE_THAT(info.hit_point.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(info.hit_point.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(info.hit_point.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(info.hit_normal.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(info.hit_normal.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(info.hit_normal.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(info.knockback.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(info.knockback.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(info.knockback.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(info.poise_damage, WithinAbs(0.0f, 0.001f));
    REQUIRE(info.caused_stagger == false);
    REQUIRE(info.is_critical == false);
    REQUIRE(info.is_blocked == false);
    REQUIRE(info.is_parried == false);
    REQUIRE(info.is_backstab == false);
    REQUIRE(info.hitbox_id.empty());
    REQUIRE(info.hurtbox_type.empty());
    REQUIRE(info.attack_name.empty());
}

TEST_CASE("DamageInfo custom values", "[combat][damage]") {
    DamageInfo info;
    info.source = Entity{1};
    info.target = Entity{2};
    info.raw_damage = 100.0f;
    info.final_damage = 75.0f;
    info.damage_type = "fire";
    info.hit_point = Vec3{10.0f, 5.0f, 3.0f};
    info.hit_normal = Vec3{0.0f, 0.0f, 1.0f};
    info.knockback = Vec3{0.0f, 2.0f, 5.0f};
    info.poise_damage = 25.0f;
    info.caused_stagger = true;
    info.is_critical = true;
    info.is_blocked = false;
    info.is_parried = false;
    info.is_backstab = true;
    info.hitbox_id = "sword_swing";
    info.hurtbox_type = "body";
    info.attack_name = "Heavy Attack";

    REQUIRE(info.source == Entity{1});
    REQUIRE(info.target == Entity{2});
    REQUIRE_THAT(info.raw_damage, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(info.final_damage, WithinAbs(75.0f, 0.001f));
    REQUIRE(info.damage_type == "fire");
    REQUIRE_THAT(info.hit_point.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(info.hit_point.y, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(info.hit_point.z, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(info.knockback.y, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(info.knockback.z, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(info.poise_damage, WithinAbs(25.0f, 0.001f));
    REQUIRE(info.caused_stagger == true);
    REQUIRE(info.is_critical == true);
    REQUIRE(info.is_backstab == true);
    REQUIRE(info.hitbox_id == "sword_swing");
    REQUIRE(info.hurtbox_type == "body");
    REQUIRE(info.attack_name == "Heavy Attack");
}

TEST_CASE("DamageInfo blocked hit", "[combat][damage]") {
    DamageInfo info;
    info.source = Entity{1};
    info.target = Entity{2};
    info.raw_damage = 100.0f;
    info.final_damage = 50.0f;  // Half damage due to block
    info.is_blocked = true;

    REQUIRE(info.is_blocked == true);
    REQUIRE_FALSE(info.is_parried);
    REQUIRE_THAT(info.final_damage, WithinAbs(50.0f, 0.001f));
}

TEST_CASE("DamageInfo parried hit", "[combat][damage]") {
    DamageInfo info;
    info.source = Entity{1};
    info.target = Entity{2};
    info.raw_damage = 100.0f;
    info.final_damage = 0.0f;  // No damage on parry
    info.is_parried = true;

    REQUIRE(info.is_parried == true);
    REQUIRE_THAT(info.final_damage, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("DamageInfo critical hit", "[combat][damage]") {
    DamageInfo info;
    info.raw_damage = 50.0f;
    info.final_damage = 75.0f;  // 1.5x critical multiplier
    info.is_critical = true;

    REQUIRE(info.is_critical == true);
    REQUIRE_THAT(info.final_damage, WithinAbs(75.0f, 0.001f));
}

// ============================================================================
// DamageInfo Damage Types
// ============================================================================

TEST_CASE("DamageInfo various damage types", "[combat][damage]") {
    SECTION("Physical damage") {
        DamageInfo info;
        info.damage_type = "physical";
        REQUIRE(info.damage_type == "physical");
    }

    SECTION("Fire damage") {
        DamageInfo info;
        info.damage_type = "fire";
        REQUIRE(info.damage_type == "fire");
    }

    SECTION("Ice damage") {
        DamageInfo info;
        info.damage_type = "ice";
        REQUIRE(info.damage_type == "ice");
    }

    SECTION("Lightning damage") {
        DamageInfo info;
        info.damage_type = "lightning";
        REQUIRE(info.damage_type == "lightning");
    }

    SECTION("Magic damage") {
        DamageInfo info;
        info.damage_type = "magic";
        REQUIRE(info.damage_type == "magic");
    }
}
