#include <catch2/catch_test_macros.hpp>
#include <engine/gameplay/character_movement.hpp>

using namespace engine::gameplay;

TEST_CASE("MantleCheckResult structure", "[gameplay][movement]") {
    SECTION("Default construction has can_mantle false") {
        MantleCheckResult result;

        REQUIRE_FALSE(result.can_mantle);
        REQUIRE(result.height == 0.0f);
    }

    SECTION("Result stores position data") {
        MantleCheckResult result;
        result.can_mantle = true;
        result.start_position = Vec3(1.0f, 0.0f, 0.0f);
        result.end_position = Vec3(1.0f, 2.0f, 1.0f);
        result.height = 2.0f;

        REQUIRE(result.can_mantle);
        REQUIRE(result.start_position.x == 1.0f);
        REQUIRE(result.end_position.y == 2.0f);
        REQUIRE(result.height == 2.0f);
    }

    SECTION("Ledge normal defaults to up") {
        MantleCheckResult result;

        REQUIRE(result.ledge_normal.y == 1.0f);
    }
}

TEST_CASE("Mantle capability checks", "[gameplay][movement]") {
    CharacterMovementComponent movement;

    SECTION("can_mantle returns true when airborne") {
        movement.state = MovementState::Jumping;
        REQUIRE(movement.can_mantle());

        movement.state = MovementState::Falling;
        REQUIRE(movement.can_mantle());
    }

    SECTION("can_mantle returns true when grounded and wants_jump") {
        movement.state = MovementState::Running;
        movement.wants_jump = true;

        REQUIRE(movement.can_mantle());
    }

    SECTION("can_mantle returns false when already mantling") {
        movement.state = MovementState::Mantling;

        REQUIRE_FALSE(movement.can_mantle());
    }

    SECTION("can_mantle returns false when sliding") {
        movement.state = MovementState::Sliding;

        REQUIRE_FALSE(movement.can_mantle());
    }

    SECTION("can_mantle returns false when movement locked") {
        movement.state = MovementState::Jumping;
        movement.movement_locked = true;

        REQUIRE_FALSE(movement.can_mantle());
    }

    SECTION("can_mantle returns false when grounded without jump") {
        movement.state = MovementState::Running;
        movement.wants_jump = false;

        REQUIRE_FALSE(movement.can_mantle());
    }
}

TEST_CASE("Mantle state behavior", "[gameplay][movement]") {
    CharacterMovementComponent movement;

    SECTION("is_mantling returns true only in Mantling state") {
        movement.state = MovementState::Mantling;
        REQUIRE(movement.is_mantling());

        movement.state = MovementState::Climbing;
        REQUIRE_FALSE(movement.is_mantling());

        movement.state = MovementState::Jumping;
        REQUIRE_FALSE(movement.is_mantling());
    }

    SECTION("Mantle progress tracks completion") {
        movement.state = MovementState::Mantling;
        movement.mantle_progress = 0.0f;

        REQUIRE(movement.mantle_progress == 0.0f);

        movement.mantle_progress = 0.5f;
        REQUIRE(movement.mantle_progress == 0.5f);

        movement.mantle_progress = 1.0f;
        REQUIRE(movement.mantle_progress == 1.0f);
    }

    SECTION("Mantle stores start and end positions") {
        movement.mantle_start = Vec3(0.0f, 0.0f, 0.0f);
        movement.mantle_end = Vec3(0.0f, 2.0f, 1.0f);

        REQUIRE(movement.mantle_start.y == 0.0f);
        REQUIRE(movement.mantle_end.y == 2.0f);
    }
}

TEST_CASE("Mantle settings validation", "[gameplay][movement]") {
    MovementSettings settings;

    SECTION("Mantle height range is valid") {
        REQUIRE(settings.mantle_min_height > 0.0f);
        REQUIRE(settings.mantle_max_height > settings.mantle_min_height);
    }

    SECTION("Mantle check distance is positive") {
        REQUIRE(settings.mantle_check_distance > 0.0f);
    }

    SECTION("Mantle duration is positive") {
        REQUIRE(settings.mantle_duration > 0.0f);
    }

    SECTION("Auto trigger is configurable") {
        REQUIRE(settings.mantle_auto_trigger == true);  // Default

        settings.mantle_auto_trigger = false;
        REQUIRE(settings.mantle_auto_trigger == false);
    }
}
