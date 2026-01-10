#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/gameplay/character_movement.hpp>
#include <engine/gameplay/water_volume.hpp>
#include <engine/scene/entity.hpp>

using namespace engine::gameplay;
using namespace Catch::Matchers;

TEST_CASE("WaterMovementSettings defaults", "[gameplay][water]") {
    WaterMovementSettings settings;

    SECTION("Speed defaults are reasonable") {
        REQUIRE(settings.swim_speed > 0.0f);
        REQUIRE(settings.underwater_speed > 0.0f);
        REQUIRE(settings.swim_sprint_speed > settings.swim_speed);
        REQUIRE(settings.underwater_sprint_speed > settings.underwater_speed);
    }

    SECTION("Breath defaults are reasonable") {
        REQUIRE(settings.max_breath > 0.0f);
        REQUIRE(settings.breath_recovery_rate > 0.0f);
        REQUIRE(settings.drowning_damage_rate > 0.0f);
    }

    SECTION("Diving is enabled by default") {
        REQUIRE(settings.can_dive);
        REQUIRE(settings.auto_surface);
    }
}

TEST_CASE("WaterVolumeComponent defaults", "[gameplay][water]") {
    WaterVolumeComponent water;

    SECTION("Default values are reasonable") {
        REQUIRE(water.water_height == 0.0f);
        REQUIRE(water.buoyancy == 1.0f);
        REQUIRE(water.drag == 2.0f);
        REQUIRE(water.is_swimmable);
        REQUIRE_FALSE(water.causes_damage);
    }

    SECTION("get_depth_at calculates correctly") {
        water.water_height = 10.0f;

        // Above water
        REQUIRE(water.get_depth_at(Vec3(0.0f, 15.0f, 0.0f)) == -5.0f);

        // At surface
        REQUIRE(water.get_depth_at(Vec3(0.0f, 10.0f, 0.0f)) == 0.0f);

        // Underwater
        REQUIRE(water.get_depth_at(Vec3(0.0f, 5.0f, 0.0f)) == 5.0f);
    }

    SECTION("is_position_underwater returns correctly") {
        water.water_height = 10.0f;

        REQUIRE_FALSE(water.is_position_underwater(Vec3(0.0f, 15.0f, 0.0f)));
        REQUIRE_FALSE(water.is_position_underwater(Vec3(0.0f, 10.0f, 0.0f)));
        REQUIRE(water.is_position_underwater(Vec3(0.0f, 5.0f, 0.0f)));
    }

    SECTION("get_current_at returns current vector") {
        water.current_direction = Vec3(1.0f, 0.0f, 0.0f);
        water.current_strength = 2.0f;

        Vec3 current = water.get_current_at(Vec3(0.0f));
        REQUIRE(current.x == 2.0f);
        REQUIRE(current.y == 0.0f);
        REQUIRE(current.z == 0.0f);
    }
}

TEST_CASE("CharacterMovementComponent water states", "[gameplay][water]") {
    CharacterMovementComponent movement;

    SECTION("is_in_water returns true for all water states") {
        movement.state = MovementState::Swimming;
        REQUIRE(movement.is_in_water());

        movement.state = MovementState::SwimmingUnderwater;
        REQUIRE(movement.is_in_water());

        movement.state = MovementState::Diving;
        REQUIRE(movement.is_in_water());

        movement.state = MovementState::Surfacing;
        REQUIRE(movement.is_in_water());

        movement.state = MovementState::Treading;
        REQUIRE(movement.is_in_water());
    }

    SECTION("is_in_water returns false for land states") {
        movement.state = MovementState::Idle;
        REQUIRE_FALSE(movement.is_in_water());

        movement.state = MovementState::Running;
        REQUIRE_FALSE(movement.is_in_water());

        movement.state = MovementState::Jumping;
        REQUIRE_FALSE(movement.is_in_water());
    }

    SECTION("is_underwater returns true for submerged states") {
        movement.state = MovementState::SwimmingUnderwater;
        REQUIRE(movement.is_underwater());

        movement.state = MovementState::Diving;
        REQUIRE(movement.is_underwater());
    }

    SECTION("is_underwater returns false for surface states") {
        movement.state = MovementState::Swimming;
        REQUIRE_FALSE(movement.is_underwater());

        movement.state = MovementState::Treading;
        REQUIRE_FALSE(movement.is_underwater());

        movement.state = MovementState::Surfacing;
        REQUIRE_FALSE(movement.is_underwater());
    }

    SECTION("is_on_water_surface returns correctly") {
        movement.state = MovementState::Swimming;
        REQUIRE(movement.is_on_water_surface());

        movement.state = MovementState::Treading;
        REQUIRE(movement.is_on_water_surface());

        movement.state = MovementState::Surfacing;
        REQUIRE(movement.is_on_water_surface());

        movement.state = MovementState::SwimmingUnderwater;
        REQUIRE_FALSE(movement.is_on_water_surface());
    }

    SECTION("is_swimming returns true for active swimming") {
        movement.state = MovementState::Swimming;
        REQUIRE(movement.is_swimming());

        movement.state = MovementState::SwimmingUnderwater;
        REQUIRE(movement.is_swimming());

        movement.state = MovementState::Treading;
        REQUIRE_FALSE(movement.is_swimming());
    }
}

TEST_CASE("CharacterMovementComponent breath mechanics", "[gameplay][water]") {
    CharacterMovementComponent movement;

    SECTION("Initial breath is at maximum") {
        REQUIRE(movement.current_breath == movement.water_settings.max_breath);
    }

    SECTION("is_drowning returns true when underwater and out of breath") {
        movement.state = MovementState::SwimmingUnderwater;
        movement.current_breath = 0.0f;

        REQUIRE(movement.is_drowning());
    }

    SECTION("is_drowning returns false when underwater with breath") {
        movement.state = MovementState::SwimmingUnderwater;
        movement.current_breath = 10.0f;

        REQUIRE_FALSE(movement.is_drowning());
    }

    SECTION("is_drowning returns false when at surface with no breath") {
        movement.state = MovementState::Swimming;
        movement.current_breath = 0.0f;

        REQUIRE_FALSE(movement.is_drowning());
    }
}

TEST_CASE("CharacterMovementComponent dive/surface capability", "[gameplay][water]") {
    CharacterMovementComponent movement;

    SECTION("can_dive requires being on water surface") {
        movement.water_settings.can_dive = true;

        movement.state = MovementState::Swimming;
        REQUIRE(movement.can_dive());

        movement.state = MovementState::Treading;
        REQUIRE(movement.can_dive());

        movement.state = MovementState::SwimmingUnderwater;
        REQUIRE_FALSE(movement.can_dive());

        movement.state = MovementState::Running;
        REQUIRE_FALSE(movement.can_dive());
    }

    SECTION("can_dive respects settings") {
        movement.state = MovementState::Swimming;

        movement.water_settings.can_dive = true;
        REQUIRE(movement.can_dive());

        movement.water_settings.can_dive = false;
        REQUIRE_FALSE(movement.can_dive());
    }

    SECTION("can_surface requires being underwater") {
        movement.state = MovementState::SwimmingUnderwater;
        REQUIRE(movement.can_surface());

        movement.state = MovementState::Diving;
        REQUIRE(movement.can_surface());

        movement.state = MovementState::Swimming;
        REQUIRE_FALSE(movement.can_surface());
    }
}

TEST_CASE("Water state string conversion", "[gameplay][water]") {
    REQUIRE(std::string(movement_state_to_string(MovementState::Swimming)) == "Swimming");
    REQUIRE(std::string(movement_state_to_string(MovementState::SwimmingUnderwater)) == "SwimmingUnderwater");
    REQUIRE(std::string(movement_state_to_string(MovementState::Diving)) == "Diving");
    REQUIRE(std::string(movement_state_to_string(MovementState::Surfacing)) == "Surfacing");
    REQUIRE(std::string(movement_state_to_string(MovementState::Treading)) == "Treading");
}

TEST_CASE("WaterQueryResult defaults", "[gameplay][water]") {
    WaterQueryResult result;

    SECTION("Default is not in water") {
        REQUIRE_FALSE(result.in_water);
        REQUIRE(result.water_entity == engine::scene::NullEntity);
    }

    SECTION("Default values are safe") {
        REQUIRE(result.depth == 0.0f);
        REQUIRE(result.buoyancy == 1.0f);
        REQUIRE(result.is_swimmable);
        REQUIRE_FALSE(result.causes_damage);
    }
}
