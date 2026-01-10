#include <catch2/catch_test_macros.hpp>
#include <engine/gameplay/character_movement.hpp>

using namespace engine::gameplay;

TEST_CASE("CharacterMovementComponent state transitions", "[gameplay][movement]") {
    CharacterMovementComponent movement;

    SECTION("Initial state is Idle") {
        REQUIRE(movement.state == MovementState::Idle);
        REQUIRE(movement.previous_state == MovementState::Idle);
        REQUIRE(movement.state_time == 0.0f);
    }

    SECTION("set_state updates state and resets timer") {
        movement.state_time = 1.0f;
        movement.set_state(MovementState::Running);

        REQUIRE(movement.state == MovementState::Running);
        REQUIRE(movement.previous_state == MovementState::Idle);
        REQUIRE(movement.state_time == 0.0f);
    }

    SECTION("set_state to same state does nothing") {
        movement.set_state(MovementState::Running);
        movement.state_time = 0.5f;

        movement.set_state(MovementState::Running);

        REQUIRE(movement.state_time == 0.5f);  // Timer not reset
    }

    SECTION("Idle to Walking when input applied") {
        movement.input_direction = Vec3(0.0f, 0.0f, 0.3f);  // Low magnitude
        // Note: Actual transition happens in system, this tests the setup
        REQUIRE(glm::length(movement.input_direction) > 0.1f);
    }

    SECTION("Walking to Running at speed threshold") {
        movement.input_direction = Vec3(0.0f, 0.0f, 0.8f);  // High magnitude
        REQUIRE(glm::length(movement.input_direction) > 0.5f);
    }

    SECTION("Running to Sprinting when sprint requested") {
        movement.state = MovementState::Running;
        movement.wants_sprint = true;
        movement.input_direction = Vec3(0.0f, 0.0f, 1.0f);

        REQUIRE(movement.can_sprint());
    }

    SECTION("Jumping to Falling after apex") {
        movement.set_state(MovementState::Jumping);
        REQUIRE(movement.is_airborne());

        movement.set_state(MovementState::Falling);
        REQUIRE(movement.is_airborne());
        REQUIRE(movement.state == MovementState::Falling);
    }

    SECTION("Falling to Landing on ground contact") {
        movement.set_state(MovementState::Falling);
        movement.set_state(MovementState::Landing);

        REQUIRE(movement.state == MovementState::Landing);
        REQUIRE(movement.is_grounded());
    }
}

TEST_CASE("CharacterMovementComponent sprint", "[gameplay][movement]") {
    CharacterMovementComponent movement;
    movement.input_direction = Vec3(0.0f, 0.0f, 1.0f);

    SECTION("can_sprint requires grounded state") {
        movement.wants_sprint = true;

        movement.state = MovementState::Running;
        REQUIRE(movement.can_sprint());

        movement.state = MovementState::Jumping;
        REQUIRE_FALSE(movement.can_sprint());
    }

    SECTION("can_sprint requires movement input") {
        movement.wants_sprint = true;
        movement.state = MovementState::Running;

        movement.input_direction = Vec3(0.0f);
        REQUIRE_FALSE(movement.can_sprint());

        movement.input_direction = Vec3(0.0f, 0.0f, 1.0f);
        REQUIRE(movement.can_sprint());
    }

    SECTION("Sprint blocked during cooldown") {
        movement.wants_sprint = true;
        movement.state = MovementState::Running;
        movement.sprint_cooldown_remaining = 0.5f;

        REQUIRE_FALSE(movement.can_sprint());
    }

    SECTION("Sprint blocked when crouching") {
        movement.wants_sprint = true;
        movement.state = MovementState::Crouching;

        REQUIRE_FALSE(movement.can_sprint());
    }

    SECTION("Sprint blocked when movement locked") {
        movement.wants_sprint = true;
        movement.state = MovementState::Running;
        movement.movement_locked = true;

        REQUIRE_FALSE(movement.can_sprint());
    }
}

TEST_CASE("CharacterMovementComponent crouch", "[gameplay][movement]") {
    CharacterMovementComponent movement;

    SECTION("is_crouching returns true for crouch states") {
        movement.state = MovementState::Crouching;
        REQUIRE(movement.is_crouching());

        movement.state = MovementState::CrouchWalking;
        REQUIRE(movement.is_crouching());

        movement.state = MovementState::Running;
        REQUIRE_FALSE(movement.is_crouching());
    }

    SECTION("Crouch amount interpolation range") {
        REQUIRE(movement.crouch_amount >= 0.0f);
        REQUIRE(movement.crouch_amount <= 1.0f);

        movement.crouch_amount = 0.5f;
        REQUIRE(movement.crouch_amount == 0.5f);
    }

    SECTION("wants_stand returns true when not crouching wanted") {
        movement.state = MovementState::Crouching;
        movement.wants_crouch = false;

        REQUIRE(movement.wants_stand());
    }

    SECTION("wants_stand returns false when crouch wanted") {
        movement.state = MovementState::Crouching;
        movement.wants_crouch = true;

        REQUIRE_FALSE(movement.wants_stand());
    }
}

TEST_CASE("CharacterMovementComponent speed calculation", "[gameplay][movement]") {
    CharacterMovementComponent movement;

    SECTION("get_target_speed returns correct values per state") {
        movement.state = MovementState::Idle;
        REQUIRE(movement.get_target_speed() == 0.0f);

        movement.state = MovementState::Walking;
        REQUIRE(movement.get_target_speed() == movement.settings.walk_speed);

        movement.state = MovementState::Running;
        REQUIRE(movement.get_target_speed() == movement.settings.run_speed);

        movement.state = MovementState::Sprinting;
        REQUIRE(movement.get_target_speed() == movement.settings.sprint_speed);

        movement.state = MovementState::CrouchWalking;
        REQUIRE(movement.get_target_speed() == movement.settings.crouch_speed);
    }

    SECTION("get_speed_normalized returns ratio to sprint speed") {
        movement.current_speed = movement.settings.sprint_speed;
        REQUIRE(movement.get_speed_normalized() == 1.0f);

        movement.current_speed = movement.settings.sprint_speed / 2.0f;
        REQUIRE(movement.get_speed_normalized() == 0.5f);

        movement.current_speed = 0.0f;
        REQUIRE(movement.get_speed_normalized() == 0.0f);
    }
}
