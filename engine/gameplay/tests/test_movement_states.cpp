#include <catch2/catch_test_macros.hpp>
#include <engine/gameplay/character_movement.hpp>

using namespace engine::gameplay;

TEST_CASE("Slide mechanics", "[gameplay][movement]") {
    CharacterMovementComponent movement;

    SECTION("can_slide returns false when not sprinting if required") {
        movement.settings.slide_requires_sprint = true;
        movement.state = MovementState::Running;

        REQUIRE_FALSE(movement.can_slide());
    }

    SECTION("can_slide returns true when sprinting") {
        movement.settings.slide_requires_sprint = true;
        movement.state = MovementState::Sprinting;

        REQUIRE(movement.can_slide());
    }

    SECTION("can_slide returns false when slide_requires_sprint is false but airborne") {
        movement.settings.slide_requires_sprint = false;
        movement.state = MovementState::Jumping;

        REQUIRE_FALSE(movement.can_slide());
    }

    SECTION("Slide blocked during cooldown") {
        movement.state = MovementState::Sprinting;
        movement.slide_cooldown_remaining = 0.5f;

        REQUIRE_FALSE(movement.can_slide());
    }

    SECTION("is_sliding returns true only in Sliding state") {
        movement.state = MovementState::Sliding;
        REQUIRE(movement.is_sliding());

        movement.state = MovementState::Running;
        REQUIRE_FALSE(movement.is_sliding());
    }

    SECTION("Slide has configurable duration") {
        movement.settings.slide_duration = 1.0f;
        REQUIRE(movement.settings.slide_duration == 1.0f);
    }

    SECTION("Slide has configurable cooldown") {
        movement.settings.slide_cooldown = 2.0f;
        REQUIRE(movement.settings.slide_cooldown == 2.0f);
    }
}

TEST_CASE("Movement queries", "[gameplay][movement]") {
    CharacterMovementComponent movement;

    SECTION("is_grounded returns correct value for each state") {
        // Grounded states
        movement.state = MovementState::Idle;
        REQUIRE(movement.is_grounded());

        movement.state = MovementState::Walking;
        REQUIRE(movement.is_grounded());

        movement.state = MovementState::Running;
        REQUIRE(movement.is_grounded());

        movement.state = MovementState::Sprinting;
        REQUIRE(movement.is_grounded());

        movement.state = MovementState::Crouching;
        REQUIRE(movement.is_grounded());

        movement.state = MovementState::CrouchWalking;
        REQUIRE(movement.is_grounded());

        movement.state = MovementState::Sliding;
        REQUIRE(movement.is_grounded());

        movement.state = MovementState::Landing;
        REQUIRE(movement.is_grounded());

        // Airborne states
        movement.state = MovementState::Jumping;
        REQUIRE_FALSE(movement.is_grounded());

        movement.state = MovementState::Falling;
        REQUIRE_FALSE(movement.is_grounded());

        // Special states
        movement.state = MovementState::Mantling;
        REQUIRE_FALSE(movement.is_grounded());

        movement.state = MovementState::Climbing;
        REQUIRE_FALSE(movement.is_grounded());
    }

    SECTION("is_moving returns true for moving states") {
        movement.state = MovementState::Walking;
        REQUIRE(movement.is_moving());

        movement.state = MovementState::Running;
        REQUIRE(movement.is_moving());

        movement.state = MovementState::Sprinting;
        REQUIRE(movement.is_moving());

        movement.state = MovementState::CrouchWalking;
        REQUIRE(movement.is_moving());

        movement.state = MovementState::Sliding;
        REQUIRE(movement.is_moving());

        // Not moving
        movement.state = MovementState::Idle;
        REQUIRE_FALSE(movement.is_moving());

        movement.state = MovementState::Crouching;
        REQUIRE_FALSE(movement.is_moving());
    }

    SECTION("is_airborne returns true for air states") {
        movement.state = MovementState::Jumping;
        REQUIRE(movement.is_airborne());

        movement.state = MovementState::Falling;
        REQUIRE(movement.is_airborne());

        movement.state = MovementState::Running;
        REQUIRE_FALSE(movement.is_airborne());
    }

    SECTION("is_sprinting returns true only in Sprinting state") {
        movement.state = MovementState::Sprinting;
        REQUIRE(movement.is_sprinting());

        movement.state = MovementState::Running;
        REQUIRE_FALSE(movement.is_sprinting());
    }

    SECTION("is_crouching returns true for crouch states") {
        movement.state = MovementState::Crouching;
        REQUIRE(movement.is_crouching());

        movement.state = MovementState::CrouchWalking;
        REQUIRE(movement.is_crouching());

        movement.state = MovementState::Idle;
        REQUIRE_FALSE(movement.is_crouching());
    }
}

TEST_CASE("Movement state string conversion", "[gameplay][movement]") {
    SECTION("All states have string representation") {
        REQUIRE(std::string(movement_state_to_string(MovementState::Idle)) == "Idle");
        REQUIRE(std::string(movement_state_to_string(MovementState::Walking)) == "Walking");
        REQUIRE(std::string(movement_state_to_string(MovementState::Running)) == "Running");
        REQUIRE(std::string(movement_state_to_string(MovementState::Sprinting)) == "Sprinting");
        REQUIRE(std::string(movement_state_to_string(MovementState::Crouching)) == "Crouching");
        REQUIRE(std::string(movement_state_to_string(MovementState::CrouchWalking)) == "CrouchWalking");
        REQUIRE(std::string(movement_state_to_string(MovementState::Sliding)) == "Sliding");
        REQUIRE(std::string(movement_state_to_string(MovementState::Jumping)) == "Jumping");
        REQUIRE(std::string(movement_state_to_string(MovementState::Falling)) == "Falling");
        REQUIRE(std::string(movement_state_to_string(MovementState::Landing)) == "Landing");
        REQUIRE(std::string(movement_state_to_string(MovementState::Climbing)) == "Climbing");
        REQUIRE(std::string(movement_state_to_string(MovementState::Mantling)) == "Mantling");
    }
}

TEST_CASE("Movement settings defaults", "[gameplay][movement]") {
    MovementSettings settings;

    SECTION("Speed defaults are sensible") {
        REQUIRE(settings.walk_speed > 0.0f);
        REQUIRE(settings.run_speed > settings.walk_speed);
        REQUIRE(settings.sprint_speed > settings.run_speed);
        REQUIRE(settings.crouch_speed > 0.0f);
        REQUIRE(settings.crouch_speed < settings.walk_speed);
    }

    SECTION("Crouch settings are valid") {
        REQUIRE(settings.crouch_height_ratio > 0.0f);
        REQUIRE(settings.crouch_height_ratio < 1.0f);
        REQUIRE(settings.crouch_transition_time > 0.0f);
    }

    SECTION("Slide settings are valid") {
        REQUIRE(settings.slide_speed > 0.0f);
        REQUIRE(settings.slide_duration > 0.0f);
        REQUIRE(settings.slide_cooldown >= 0.0f);
    }

    SECTION("Mantle settings are valid") {
        REQUIRE(settings.mantle_min_height > 0.0f);
        REQUIRE(settings.mantle_max_height > settings.mantle_min_height);
        REQUIRE(settings.mantle_duration > 0.0f);
        REQUIRE(settings.mantle_check_distance > 0.0f);
    }
}
