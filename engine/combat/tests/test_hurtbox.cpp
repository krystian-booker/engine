#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/combat/hurtbox.hpp>

using namespace engine::combat;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// HurtboxComponent Tests
// ============================================================================

TEST_CASE("HurtboxComponent defaults", "[combat][hurtbox]") {
    HurtboxComponent hurtbox;

    REQUIRE(hurtbox.enabled == true);
    REQUIRE(hurtbox.shape == CollisionShape::Sphere);
    REQUIRE_THAT(hurtbox.center_offset.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(hurtbox.center_offset.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(hurtbox.center_offset.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(hurtbox.half_extents.x, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(hurtbox.half_extents.y, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(hurtbox.half_extents.z, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(hurtbox.radius, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(hurtbox.height, WithinAbs(1.0f, 0.001f));
    REQUIRE(hurtbox.hurtbox_type == "body");
    REQUIRE_THAT(hurtbox.damage_multiplier, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(hurtbox.poise_multiplier, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(hurtbox.physical_resistance, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(hurtbox.fire_resistance, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(hurtbox.ice_resistance, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(hurtbox.lightning_resistance, WithinAbs(0.0f, 0.001f));
    REQUIRE(hurtbox.faction == "enemy");
}

TEST_CASE("HurtboxComponent hurtbox types", "[combat][hurtbox]") {
    SECTION("Head - high damage multiplier") {
        HurtboxComponent hurtbox;
        hurtbox.hurtbox_type = "head";
        hurtbox.damage_multiplier = 2.0f;

        REQUIRE(hurtbox.hurtbox_type == "head");
        REQUIRE_THAT(hurtbox.damage_multiplier, WithinAbs(2.0f, 0.001f));
    }

    SECTION("Body - normal damage") {
        HurtboxComponent hurtbox;
        hurtbox.hurtbox_type = "body";
        hurtbox.damage_multiplier = 1.0f;

        REQUIRE(hurtbox.hurtbox_type == "body");
        REQUIRE_THAT(hurtbox.damage_multiplier, WithinAbs(1.0f, 0.001f));
    }

    SECTION("Limb - reduced damage") {
        HurtboxComponent hurtbox;
        hurtbox.hurtbox_type = "limb";
        hurtbox.damage_multiplier = 0.75f;

        REQUIRE(hurtbox.hurtbox_type == "limb");
        REQUIRE_THAT(hurtbox.damage_multiplier, WithinAbs(0.75f, 0.001f));
    }

    SECTION("Armor - heavily reduced damage") {
        HurtboxComponent hurtbox;
        hurtbox.hurtbox_type = "armor";
        hurtbox.damage_multiplier = 0.25f;

        REQUIRE(hurtbox.hurtbox_type == "armor");
        REQUIRE_THAT(hurtbox.damage_multiplier, WithinAbs(0.25f, 0.001f));
    }

    SECTION("Weakpoint - very high damage multiplier") {
        HurtboxComponent hurtbox;
        hurtbox.hurtbox_type = "weakpoint";
        hurtbox.damage_multiplier = 3.0f;

        REQUIRE(hurtbox.hurtbox_type == "weakpoint");
        REQUIRE_THAT(hurtbox.damage_multiplier, WithinAbs(3.0f, 0.001f));
    }
}

TEST_CASE("HurtboxComponent get_resistance", "[combat][hurtbox]") {
    HurtboxComponent hurtbox;
    hurtbox.physical_resistance = 0.3f;
    hurtbox.fire_resistance = 0.5f;
    hurtbox.ice_resistance = 0.7f;
    hurtbox.lightning_resistance = 0.2f;

    SECTION("Physical resistance") {
        float res = hurtbox.get_resistance("physical");
        REQUIRE_THAT(res, WithinAbs(0.3f, 0.001f));
    }

    SECTION("Fire resistance") {
        float res = hurtbox.get_resistance("fire");
        REQUIRE_THAT(res, WithinAbs(0.5f, 0.001f));
    }

    SECTION("Ice resistance") {
        float res = hurtbox.get_resistance("ice");
        REQUIRE_THAT(res, WithinAbs(0.7f, 0.001f));
    }

    SECTION("Lightning resistance") {
        float res = hurtbox.get_resistance("lightning");
        REQUIRE_THAT(res, WithinAbs(0.2f, 0.001f));
    }

    SECTION("Unknown type returns 0") {
        float res = hurtbox.get_resistance("dark");
        REQUIRE_THAT(res, WithinAbs(0.0f, 0.001f));

        res = hurtbox.get_resistance("holy");
        REQUIRE_THAT(res, WithinAbs(0.0f, 0.001f));
    }
}

// ============================================================================
// DamageReceiverComponent Tests
// ============================================================================

TEST_CASE("DamageReceiverComponent defaults", "[combat][hurtbox]") {
    DamageReceiverComponent receiver;

    REQUIRE(receiver.can_receive_damage == true);
    REQUIRE_THAT(receiver.max_poise, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(receiver.current_poise, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(receiver.poise_recovery_rate, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(receiver.poise_recovery_delay, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(receiver.time_since_hit, WithinAbs(0.0f, 0.001f));
    REQUIRE(receiver.is_blocking == false);
    REQUIRE(receiver.is_parrying == false);
    REQUIRE_THAT(receiver.block_damage_reduction, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(receiver.parry_window, WithinAbs(0.0f, 0.001f));
    REQUIRE(receiver.backstab_vulnerable == true);
    REQUIRE_THAT(receiver.backstab_multiplier, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(receiver.backstab_angle_threshold, WithinAbs(60.0f, 0.001f));
}

TEST_CASE("DamageReceiverComponent apply_poise_damage", "[combat][hurtbox]") {
    DamageReceiverComponent receiver;
    receiver.max_poise = 100.0f;
    receiver.current_poise = 100.0f;

    SECTION("Damage without stagger") {
        bool staggered = receiver.apply_poise_damage(30.0f);
        REQUIRE_FALSE(staggered);
        REQUIRE_THAT(receiver.current_poise, WithinAbs(70.0f, 0.001f));
        REQUIRE_THAT(receiver.time_since_hit, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Damage causing stagger") {
        bool staggered = receiver.apply_poise_damage(150.0f);
        REQUIRE(staggered);
        REQUIRE_THAT(receiver.current_poise, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Multiple hits causing stagger") {
        receiver.apply_poise_damage(40.0f);
        REQUIRE_THAT(receiver.current_poise, WithinAbs(60.0f, 0.001f));

        receiver.apply_poise_damage(40.0f);
        REQUIRE_THAT(receiver.current_poise, WithinAbs(20.0f, 0.001f));

        bool staggered = receiver.apply_poise_damage(40.0f);
        REQUIRE(staggered);
        REQUIRE_THAT(receiver.current_poise, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Exact poise depletion") {
        bool staggered = receiver.apply_poise_damage(100.0f);
        REQUIRE(staggered);
        REQUIRE_THAT(receiver.current_poise, WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("DamageReceiverComponent recover_poise", "[combat][hurtbox]") {
    DamageReceiverComponent receiver;
    receiver.max_poise = 100.0f;
    receiver.current_poise = 50.0f;
    receiver.poise_recovery_rate = 20.0f;
    receiver.poise_recovery_delay = 2.0f;
    receiver.time_since_hit = 0.0f;

    SECTION("No recovery during delay") {
        receiver.recover_poise(1.0f);
        REQUIRE_THAT(receiver.current_poise, WithinAbs(50.0f, 0.001f));
        REQUIRE_THAT(receiver.time_since_hit, WithinAbs(1.0f, 0.001f));
    }

    SECTION("Recovery after delay") {
        receiver.time_since_hit = 2.0f;
        receiver.recover_poise(1.0f);
        // Should recover 20.0f per second
        REQUIRE_THAT(receiver.current_poise, WithinAbs(70.0f, 0.001f));
    }

    SECTION("Recovery capped at max") {
        receiver.time_since_hit = 10.0f;
        receiver.current_poise = 95.0f;
        receiver.recover_poise(1.0f);
        // Would recover 20.0f but capped at max 100.0f
        REQUIRE_THAT(receiver.current_poise, WithinAbs(100.0f, 0.001f));
    }
}

TEST_CASE("DamageReceiverComponent reset_poise", "[combat][hurtbox]") {
    DamageReceiverComponent receiver;
    receiver.max_poise = 100.0f;
    receiver.current_poise = 25.0f;
    receiver.poise_recovery_delay = 2.0f;
    receiver.time_since_hit = 0.5f;

    receiver.reset_poise();

    REQUIRE_THAT(receiver.current_poise, WithinAbs(100.0f, 0.001f));
    // Time since hit should be set to delay so recovery can happen immediately if hit
    REQUIRE_THAT(receiver.time_since_hit, WithinAbs(2.0f, 0.001f));
}

TEST_CASE("DamageReceiverComponent blocking", "[combat][hurtbox]") {
    DamageReceiverComponent receiver;
    receiver.is_blocking = true;
    receiver.block_damage_reduction = 0.7f;  // 70% damage reduction

    REQUIRE(receiver.is_blocking == true);
    REQUIRE_THAT(receiver.block_damage_reduction, WithinAbs(0.7f, 0.001f));
}

TEST_CASE("DamageReceiverComponent parrying", "[combat][hurtbox]") {
    DamageReceiverComponent receiver;
    receiver.is_parrying = true;
    receiver.parry_window = 0.15f;

    REQUIRE(receiver.is_parrying == true);
    REQUIRE_THAT(receiver.parry_window, WithinAbs(0.15f, 0.001f));
}

TEST_CASE("DamageReceiverComponent backstab configuration", "[combat][hurtbox]") {
    DamageReceiverComponent receiver;
    receiver.backstab_vulnerable = true;
    receiver.backstab_multiplier = 3.0f;
    receiver.backstab_angle_threshold = 45.0f;

    REQUIRE(receiver.backstab_vulnerable == true);
    REQUIRE_THAT(receiver.backstab_multiplier, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(receiver.backstab_angle_threshold, WithinAbs(45.0f, 0.001f));
}

TEST_CASE("DamageReceiverComponent immune to damage", "[combat][hurtbox]") {
    DamageReceiverComponent receiver;
    receiver.can_receive_damage = false;

    REQUIRE_FALSE(receiver.can_receive_damage);
}
