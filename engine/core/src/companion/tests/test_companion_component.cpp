#include <catch2/catch_test_macros.hpp>
#include <engine/companion/companion.hpp>
#include <engine/scene/entity.hpp>

namespace engine::companion::tests {

class CompanionComponentTests {};

using namespace engine::companion;
using namespace engine::scene;

TEST_CASE_METHOD(CompanionComponentTests, "CompanionComponent state management", "[companion][@engine.companion][#CompanionComponentTests]") {
    CompanionComponent comp;

    SECTION("Default state is Following") {
        REQUIRE(comp.state == CompanionState::Following);
        REQUIRE(comp.previous_state == CompanionState::Following);
    }

    SECTION("set_state updates state and resets timer") {
        comp.state_time = 1.0f;
        comp.set_state(CompanionState::Waiting);

        REQUIRE(comp.state == CompanionState::Waiting);
        REQUIRE(comp.previous_state == CompanionState::Following);
        REQUIRE(comp.state_time == 0.0f);
    }

    SECTION("set_state to same state does nothing") {
        comp.set_state(CompanionState::Waiting);
        comp.state_time = 0.5f;

        comp.set_state(CompanionState::Waiting);

        REQUIRE(comp.state_time == 0.5f);  // Timer not reset
    }

    SECTION("State transitions are valid") {
        comp.set_state(CompanionState::Following);
        REQUIRE(comp.is_following());

        comp.set_state(CompanionState::Attacking);
        REQUIRE(comp.is_in_combat());

        comp.set_state(CompanionState::Dead);
        REQUIRE(comp.is_dead());
    }

    SECTION("Owner entity is tracked") {
        REQUIRE(comp.owner == NullEntity);

        comp.owner = Entity{42};
        REQUIRE(comp.owner != NullEntity);
    }
}

TEST_CASE_METHOD(CompanionComponentTests, "CompanionComponent settings", "[companion][@engine.companion][#CompanionComponentTests]") {
    CompanionComponent comp;

    SECTION("Follow distance is configurable") {
        REQUIRE(comp.follow_distance == 2.5f);

        comp.follow_distance = 5.0f;
        REQUIRE(comp.follow_distance == 5.0f);
    }

    SECTION("Teleport threshold works correctly") {
        REQUIRE(comp.teleport_if_too_far == true);
        REQUIRE(comp.teleport_distance == 30.0f);

        comp.teleport_if_too_far = false;
        REQUIRE(comp.teleport_if_too_far == false);
    }

    SECTION("Combat engagement range is respected") {
        REQUIRE(comp.engagement_range == 15.0f);
        REQUIRE(comp.disengage_range == 25.0f);
        REQUIRE(comp.disengage_range > comp.engagement_range);

        comp.engagement_range = 10.0f;
        comp.disengage_range = 20.0f;
        REQUIRE(comp.engagement_range == 10.0f);
        REQUIRE(comp.disengage_range == 20.0f);
    }
}

TEST_CASE_METHOD(CompanionComponentTests, "CompanionComponent state queries", "[companion][@engine.companion][#CompanionComponentTests]") {
    CompanionComponent comp;

    SECTION("is_following returns true only in Following state") {
        comp.state = CompanionState::Following;
        REQUIRE(comp.is_following());

        comp.state = CompanionState::Waiting;
        REQUIRE_FALSE(comp.is_following());
    }

    SECTION("is_waiting returns true only in Waiting state") {
        comp.state = CompanionState::Waiting;
        REQUIRE(comp.is_waiting());

        comp.state = CompanionState::Following;
        REQUIRE_FALSE(comp.is_waiting());
    }

    SECTION("is_in_combat returns true for combat states") {
        comp.state = CompanionState::Attacking;
        REQUIRE(comp.is_in_combat());

        comp.state = CompanionState::Defending;
        REQUIRE(comp.is_in_combat());

        comp.state = CompanionState::Following;
        REQUIRE_FALSE(comp.is_in_combat());
    }

    SECTION("is_dead returns true only in Dead state") {
        comp.state = CompanionState::Dead;
        REQUIRE(comp.is_dead());

        comp.state = CompanionState::Following;
        REQUIRE_FALSE(comp.is_dead());
    }

    SECTION("is_idle returns true for idle states") {
        comp.state = CompanionState::Following;
        REQUIRE(comp.is_idle());

        comp.state = CompanionState::Waiting;
        REQUIRE(comp.is_idle());

        comp.state = CompanionState::Attacking;
        REQUIRE_FALSE(comp.is_idle());
    }
}

TEST_CASE_METHOD(CompanionComponentTests, "CompanionState string conversion", "[companion][@engine.companion][#CompanionComponentTests]") {
    SECTION("All states have string representation") {
        REQUIRE(std::string(companion_state_to_string(CompanionState::Following)) == "Following");
        REQUIRE(std::string(companion_state_to_string(CompanionState::Waiting)) == "Waiting");
        REQUIRE(std::string(companion_state_to_string(CompanionState::Attacking)) == "Attacking");
        REQUIRE(std::string(companion_state_to_string(CompanionState::Defending)) == "Defending");
        REQUIRE(std::string(companion_state_to_string(CompanionState::Moving)) == "Moving");
        REQUIRE(std::string(companion_state_to_string(CompanionState::Interacting)) == "Interacting");
        REQUIRE(std::string(companion_state_to_string(CompanionState::Dead)) == "Dead");
        REQUIRE(std::string(companion_state_to_string(CompanionState::Custom)) == "Custom");
    }
}

} // namespace engine::companion::tests
