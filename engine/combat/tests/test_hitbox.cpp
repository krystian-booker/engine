#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/combat/hitbox.hpp>

using namespace engine::combat;
using namespace engine::scene;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// CollisionShape Tests
// ============================================================================

TEST_CASE("CollisionShape enum", "[combat][hitbox]") {
    REQUIRE(static_cast<uint8_t>(CollisionShape::Sphere) == 0);
    REQUIRE(static_cast<uint8_t>(CollisionShape::Box) == 1);
    REQUIRE(static_cast<uint8_t>(CollisionShape::Capsule) == 2);
}

// ============================================================================
// HitboxComponent Tests
// ============================================================================

TEST_CASE("HitboxComponent defaults", "[combat][hitbox]") {
    HitboxComponent hitbox;

    REQUIRE(hitbox.active == false);
    REQUIRE(hitbox.hitbox_id.empty());
    REQUIRE(hitbox.shape == CollisionShape::Sphere);
    REQUIRE_THAT(hitbox.center_offset.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(hitbox.center_offset.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(hitbox.center_offset.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(hitbox.half_extents.x, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(hitbox.half_extents.y, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(hitbox.half_extents.z, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(hitbox.radius, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(hitbox.height, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(hitbox.base_damage, WithinAbs(10.0f, 0.001f));
    REQUIRE(hitbox.damage_type == "physical");
    REQUIRE_THAT(hitbox.knockback_force, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(hitbox.knockback_direction.z, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(hitbox.poise_damage, WithinAbs(10.0f, 0.001f));
    REQUIRE(hitbox.causes_stagger == true);
    REQUIRE_THAT(hitbox.critical_multiplier, WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(hitbox.critical_chance, WithinAbs(0.0f, 0.001f));
    REQUIRE(hitbox.already_hit.empty());
    REQUIRE(hitbox.max_hits == -1);
    REQUIRE_THAT(hitbox.hit_cooldown_per_target, WithinAbs(0.5f, 0.001f));
    REQUIRE(hitbox.faction == "player");
    REQUIRE(hitbox.target_factions.size() == 1);
    REQUIRE(hitbox.target_factions[0] == "enemy");
    REQUIRE(hitbox.hit_sound.empty());
    REQUIRE(hitbox.hit_effect.empty());
}

TEST_CASE("HitboxComponent activate/deactivate", "[combat][hitbox]") {
    HitboxComponent hitbox;

    SECTION("Activate") {
        REQUIRE_FALSE(hitbox.active);
        hitbox.activate();
        REQUIRE(hitbox.active);
        REQUIRE(hitbox.already_hit.empty());  // Should clear hit list
    }

    SECTION("Deactivate") {
        hitbox.active = true;
        hitbox.deactivate();
        REQUIRE_FALSE(hitbox.active);
    }
}

TEST_CASE("HitboxComponent hit tracking", "[combat][hitbox]") {
    HitboxComponent hitbox;

    SECTION("was_hit - empty list") {
        REQUIRE_FALSE(hitbox.was_hit(Entity{1}));
    }

    SECTION("was_hit - entity in list") {
        hitbox.already_hit.push_back(Entity{1});
        hitbox.already_hit.push_back(Entity{2});

        REQUIRE(hitbox.was_hit(Entity{1}));
        REQUIRE(hitbox.was_hit(Entity{2}));
        REQUIRE_FALSE(hitbox.was_hit(Entity{3}));
    }

    SECTION("clear_hit_list") {
        hitbox.already_hit.push_back(Entity{1});
        hitbox.already_hit.push_back(Entity{2});
        REQUIRE(hitbox.already_hit.size() == 2);

        hitbox.clear_hit_list();
        REQUIRE(hitbox.already_hit.empty());
    }

    SECTION("activate clears hit list") {
        hitbox.already_hit.push_back(Entity{1});
        hitbox.activate();
        REQUIRE(hitbox.already_hit.empty());
    }
}

TEST_CASE("HitboxComponent sphere shape", "[combat][hitbox]") {
    HitboxComponent hitbox;
    hitbox.shape = CollisionShape::Sphere;
    hitbox.radius = 1.5f;
    hitbox.center_offset = Vec3{0.0f, 1.0f, 0.0f};

    REQUIRE(hitbox.shape == CollisionShape::Sphere);
    REQUIRE_THAT(hitbox.radius, WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(hitbox.center_offset.y, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("HitboxComponent box shape", "[combat][hitbox]") {
    HitboxComponent hitbox;
    hitbox.shape = CollisionShape::Box;
    hitbox.half_extents = Vec3{1.0f, 0.5f, 2.0f};

    REQUIRE(hitbox.shape == CollisionShape::Box);
    REQUIRE_THAT(hitbox.half_extents.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(hitbox.half_extents.y, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(hitbox.half_extents.z, WithinAbs(2.0f, 0.001f));
}

TEST_CASE("HitboxComponent capsule shape", "[combat][hitbox]") {
    HitboxComponent hitbox;
    hitbox.shape = CollisionShape::Capsule;
    hitbox.radius = 0.3f;
    hitbox.height = 1.8f;

    REQUIRE(hitbox.shape == CollisionShape::Capsule);
    REQUIRE_THAT(hitbox.radius, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(hitbox.height, WithinAbs(1.8f, 0.001f));
}

TEST_CASE("HitboxComponent damage configuration", "[combat][hitbox]") {
    HitboxComponent hitbox;
    hitbox.base_damage = 50.0f;
    hitbox.damage_type = "fire";
    hitbox.knockback_force = 10.0f;
    hitbox.knockback_direction = Vec3{0.0f, 0.5f, 0.5f};
    hitbox.poise_damage = 30.0f;
    hitbox.causes_stagger = false;

    REQUIRE_THAT(hitbox.base_damage, WithinAbs(50.0f, 0.001f));
    REQUIRE(hitbox.damage_type == "fire");
    REQUIRE_THAT(hitbox.knockback_force, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(hitbox.knockback_direction.y, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(hitbox.poise_damage, WithinAbs(30.0f, 0.001f));
    REQUIRE_FALSE(hitbox.causes_stagger);
}

TEST_CASE("HitboxComponent critical hit configuration", "[combat][hitbox]") {
    HitboxComponent hitbox;
    hitbox.critical_multiplier = 2.0f;
    hitbox.critical_chance = 0.25f;

    REQUIRE_THAT(hitbox.critical_multiplier, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(hitbox.critical_chance, WithinAbs(0.25f, 0.001f));
}

TEST_CASE("HitboxComponent faction targeting", "[combat][hitbox]") {
    HitboxComponent hitbox;
    hitbox.faction = "enemy";
    hitbox.target_factions = {"player", "neutral", "wildlife"};

    REQUIRE(hitbox.faction == "enemy");
    REQUIRE(hitbox.target_factions.size() == 3);
    REQUIRE(hitbox.target_factions[0] == "player");
    REQUIRE(hitbox.target_factions[1] == "neutral");
    REQUIRE(hitbox.target_factions[2] == "wildlife");
}

TEST_CASE("HitboxComponent audio/visual feedback", "[combat][hitbox]") {
    HitboxComponent hitbox;
    hitbox.hit_sound = "sfx/sword_hit.wav";
    hitbox.hit_effect = "vfx/blood_splatter";

    REQUIRE(hitbox.hit_sound == "sfx/sword_hit.wav");
    REQUIRE(hitbox.hit_effect == "vfx/blood_splatter");
}

// ============================================================================
// HitboxOverlap Tests
// ============================================================================

TEST_CASE("HitboxOverlap defaults", "[combat][hitbox]") {
    HitboxOverlap overlap;

    REQUIRE(overlap.attacker == NullEntity);
    REQUIRE(overlap.target == NullEntity);
    REQUIRE(overlap.hitbox_id.empty());
    REQUIRE(overlap.hurtbox_type.empty());
}

TEST_CASE("HitboxOverlap custom values", "[combat][hitbox]") {
    HitboxOverlap overlap;
    overlap.attacker = Entity{1};
    overlap.target = Entity{2};
    overlap.hit_point = Vec3{5.0f, 1.0f, 3.0f};
    overlap.hit_normal = Vec3{0.0f, 0.0f, 1.0f};
    overlap.hitbox_id = "sword_slash";
    overlap.hurtbox_type = "head";

    REQUIRE(overlap.attacker == Entity{1});
    REQUIRE(overlap.target == Entity{2});
    REQUIRE_THAT(overlap.hit_point.x, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(overlap.hit_point.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(overlap.hit_point.z, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(overlap.hit_normal.z, WithinAbs(1.0f, 0.001f));
    REQUIRE(overlap.hitbox_id == "sword_slash");
    REQUIRE(overlap.hurtbox_type == "head");
}
