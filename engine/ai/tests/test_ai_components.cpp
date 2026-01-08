// engine/ai/tests/test_ai_components.cpp
// Tests for AI component structs and helper methods

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/ai/ai_components.hpp>

using namespace engine::ai;
using namespace engine::scene;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// AIControllerComponent Tests
// ============================================================================

TEST_CASE("AIControllerComponent defaults", "[ai][components]") {
    AIControllerComponent controller;

    REQUIRE(controller.enabled == true);
    REQUIRE(controller.behavior_tree == nullptr);
    REQUIRE(controller.blackboard == nullptr);
    REQUIRE_THAT(controller.update_interval, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(controller.time_since_update, WithinAbs(0.0f, 0.001f));
    REQUIRE(controller.current_state.empty());
    REQUIRE(controller.last_status == BTStatus::Failure);
    REQUIRE(controller.current_target == NullEntity);
    REQUIRE_THAT(controller.time_with_target, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("AIControllerComponent ensure_blackboard", "[ai][components]") {
    AIControllerComponent controller;

    REQUIRE(controller.blackboard == nullptr);

    controller.ensure_blackboard();

    REQUIRE(controller.blackboard != nullptr);

    // Calling again should not create a new blackboard
    auto* first = controller.blackboard.get();
    controller.ensure_blackboard();
    REQUIRE(controller.blackboard.get() == first);
}

TEST_CASE("AIControllerComponent should_update", "[ai][components]") {
    AIControllerComponent controller;
    controller.update_interval = 0.1f;
    controller.time_since_update = 0.0f;

    SECTION("Below interval - returns false") {
        REQUIRE_FALSE(controller.should_update(0.05f));
        REQUIRE_THAT(controller.time_since_update, WithinAbs(0.05f, 0.001f));
    }

    SECTION("At interval - returns true and resets") {
        controller.time_since_update = 0.05f;
        REQUIRE(controller.should_update(0.05f));
        REQUIRE_THAT(controller.time_since_update, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Above interval - returns true and resets") {
        REQUIRE(controller.should_update(0.2f));
        REQUIRE_THAT(controller.time_since_update, WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("AIControllerComponent state tracking", "[ai][components]") {
    AIControllerComponent controller;

    controller.current_state = "Patrol";
    controller.last_status = BTStatus::Success;
    controller.current_target = Entity{42};
    controller.time_with_target = 5.0f;

    REQUIRE(controller.current_state == "Patrol");
    REQUIRE(controller.last_status == BTStatus::Success);
    REQUIRE(controller.current_target == Entity{42});
    REQUIRE_THAT(controller.time_with_target, WithinAbs(5.0f, 0.001f));
}

// ============================================================================
// AICombatComponent Tests
// ============================================================================

TEST_CASE("AICombatComponent defaults", "[ai][components]") {
    AICombatComponent combat;

    // Target
    REQUIRE(combat.threat == NullEntity);
    REQUIRE_THAT(combat.threat_level, WithinAbs(0.0f, 0.001f));

    // Combat parameters
    REQUIRE_THAT(combat.attack_range, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(combat.ranged_attack_range, WithinAbs(15.0f, 0.001f));
    REQUIRE_THAT(combat.preferred_distance, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(combat.min_distance, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(combat.max_chase_distance, WithinAbs(30.0f, 0.001f));

    // Attack timing
    REQUIRE_THAT(combat.attack_cooldown, WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(combat.time_since_attack, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(combat.combo_window, WithinAbs(0.5f, 0.001f));
    REQUIRE(combat.current_combo == 0);
    REQUIRE(combat.max_combo == 3);

    // Defense
    REQUIRE_THAT(combat.block_chance, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(combat.dodge_chance, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(combat.parry_window, WithinAbs(0.1f, 0.001f));

    // Behavior weights
    REQUIRE_THAT(combat.aggression, WithinAbs(0.7f, 0.001f));
    REQUIRE_THAT(combat.caution, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(combat.patience, WithinAbs(0.5f, 0.001f));

    // Thresholds
    REQUIRE_THAT(combat.flee_health_threshold, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(combat.stagger_threshold, WithinAbs(30.0f, 0.001f));

    // State
    REQUIRE(combat.is_attacking == false);
    REQUIRE(combat.is_blocking == false);
    REQUIRE(combat.is_staggered == false);
    REQUIRE(combat.is_fleeing == false);

    // Attack selection
    REQUIRE(combat.available_attacks.empty());
    REQUIRE(combat.current_attack.empty());
    REQUIRE(combat.attack_pattern_index == 0);
}

TEST_CASE("AICombatComponent can_attack", "[ai][components]") {
    AICombatComponent combat;
    combat.attack_cooldown = 1.0f;

    SECTION("Cannot attack when cooldown not elapsed") {
        combat.time_since_attack = 0.5f;
        REQUIRE_FALSE(combat.can_attack());
    }

    SECTION("Can attack when cooldown elapsed") {
        combat.time_since_attack = 1.0f;
        REQUIRE(combat.can_attack());
    }

    SECTION("Cannot attack while attacking") {
        combat.time_since_attack = 2.0f;
        combat.is_attacking = true;
        REQUIRE_FALSE(combat.can_attack());
    }

    SECTION("Cannot attack while staggered") {
        combat.time_since_attack = 2.0f;
        combat.is_staggered = true;
        REQUIRE_FALSE(combat.can_attack());
    }
}

TEST_CASE("AICombatComponent in_attack_range", "[ai][components]") {
    AICombatComponent combat;
    combat.attack_range = 2.0f;

    REQUIRE(combat.in_attack_range(1.0f));
    REQUIRE(combat.in_attack_range(2.0f));
    REQUIRE_FALSE(combat.in_attack_range(2.5f));
    REQUIRE_FALSE(combat.in_attack_range(10.0f));
}

TEST_CASE("AICombatComponent in_ranged_range", "[ai][components]") {
    AICombatComponent combat;
    combat.attack_range = 2.0f;
    combat.ranged_attack_range = 15.0f;

    // Too close (melee range)
    REQUIRE_FALSE(combat.in_ranged_range(1.0f));
    REQUIRE_FALSE(combat.in_ranged_range(2.0f));

    // In ranged range
    REQUIRE(combat.in_ranged_range(5.0f));
    REQUIRE(combat.in_ranged_range(10.0f));
    REQUIRE(combat.in_ranged_range(15.0f));

    // Too far
    REQUIRE_FALSE(combat.in_ranged_range(20.0f));
}

TEST_CASE("AICombatComponent start_attack", "[ai][components]") {
    AICombatComponent combat;
    combat.time_since_attack = 5.0f;

    combat.start_attack();

    REQUIRE(combat.is_attacking == true);
    REQUIRE_THAT(combat.time_since_attack, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("AICombatComponent end_attack", "[ai][components]") {
    AICombatComponent combat;
    combat.is_attacking = true;
    combat.current_combo = 0;
    combat.max_combo = 3;

    SECTION("Increments combo") {
        combat.end_attack();
        REQUIRE(combat.is_attacking == false);
        REQUIRE(combat.current_combo == 1);
    }

    SECTION("Resets combo at max") {
        combat.current_combo = 2;
        combat.end_attack();
        REQUIRE(combat.current_combo == 0);
    }
}

// ============================================================================
// AIPatrolComponent Tests
// ============================================================================

TEST_CASE("AIPatrolComponent::PatrolType enum", "[ai][components]") {
    REQUIRE(static_cast<uint8_t>(AIPatrolComponent::PatrolType::None) == 0);
    REQUIRE(static_cast<uint8_t>(AIPatrolComponent::PatrolType::Loop) == 1);
    REQUIRE(static_cast<uint8_t>(AIPatrolComponent::PatrolType::PingPong) == 2);
    REQUIRE(static_cast<uint8_t>(AIPatrolComponent::PatrolType::Random) == 3);
}

TEST_CASE("AIPatrolComponent defaults", "[ai][components]") {
    AIPatrolComponent patrol;

    REQUIRE(patrol.type == AIPatrolComponent::PatrolType::Loop);
    REQUIRE(patrol.waypoints.empty());
    REQUIRE(patrol.current_waypoint == 0);
    REQUIRE(patrol.reverse_direction == false);
    REQUIRE_THAT(patrol.wait_time_min, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(patrol.wait_time_max, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(patrol.current_wait_time, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(patrol.time_at_waypoint, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(patrol.patrol_speed, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(patrol.arrival_distance, WithinAbs(0.5f, 0.001f));
    REQUIRE(patrol.is_waiting == false);
    REQUIRE(patrol.patrol_active == true);
}

TEST_CASE("AIPatrolComponent get_current_waypoint", "[ai][components]") {
    AIPatrolComponent patrol;

    SECTION("Empty waypoints returns zero") {
        Vec3 wp = patrol.get_current_waypoint();
        REQUIRE_THAT(wp.x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(wp.y, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(wp.z, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Returns correct waypoint") {
        patrol.waypoints = {
            Vec3{0.0f, 0.0f, 0.0f},
            Vec3{10.0f, 0.0f, 0.0f},
            Vec3{10.0f, 0.0f, 10.0f}
        };
        patrol.current_waypoint = 1;

        Vec3 wp = patrol.get_current_waypoint();
        REQUIRE_THAT(wp.x, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(wp.z, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Wraps around with modulo") {
        patrol.waypoints = {Vec3{5.0f, 0.0f, 0.0f}};
        patrol.current_waypoint = 10;

        Vec3 wp = patrol.get_current_waypoint();
        REQUIRE_THAT(wp.x, WithinAbs(5.0f, 0.001f));
    }
}

TEST_CASE("AIPatrolComponent advance_waypoint Loop", "[ai][components]") {
    AIPatrolComponent patrol;
    patrol.type = AIPatrolComponent::PatrolType::Loop;
    patrol.waypoints = {
        Vec3{0.0f},
        Vec3{1.0f, 0.0f, 0.0f},
        Vec3{2.0f, 0.0f, 0.0f}
    };

    REQUIRE(patrol.current_waypoint == 0);
    patrol.advance_waypoint();
    REQUIRE(patrol.current_waypoint == 1);
    patrol.advance_waypoint();
    REQUIRE(patrol.current_waypoint == 2);
    patrol.advance_waypoint();
    REQUIRE(patrol.current_waypoint == 0); // Loops back
}

TEST_CASE("AIPatrolComponent advance_waypoint PingPong", "[ai][components]") {
    AIPatrolComponent patrol;
    patrol.type = AIPatrolComponent::PatrolType::PingPong;
    patrol.waypoints = {
        Vec3{0.0f},
        Vec3{1.0f, 0.0f, 0.0f},
        Vec3{2.0f, 0.0f, 0.0f}
    };

    // Forward direction
    REQUIRE(patrol.reverse_direction == false);
    REQUIRE(patrol.current_waypoint == 0);

    patrol.advance_waypoint();
    REQUIRE(patrol.current_waypoint == 1);

    patrol.advance_waypoint();
    REQUIRE(patrol.current_waypoint == 2);
    REQUIRE(patrol.reverse_direction == true); // At end, reverses

    patrol.advance_waypoint();
    REQUIRE(patrol.current_waypoint == 1);

    patrol.advance_waypoint();
    REQUIRE(patrol.current_waypoint == 0);
    REQUIRE(patrol.reverse_direction == false); // At start, reverses back
}

TEST_CASE("AIPatrolComponent advance_waypoint None", "[ai][components]") {
    AIPatrolComponent patrol;
    patrol.type = AIPatrolComponent::PatrolType::None;
    patrol.waypoints = {Vec3{0.0f}, Vec3{1.0f, 0.0f, 0.0f}};
    patrol.current_waypoint = 0;

    patrol.advance_waypoint();
    REQUIRE(patrol.current_waypoint == 0); // No change
}

TEST_CASE("AIPatrolComponent advance_waypoint empty", "[ai][components]") {
    AIPatrolComponent patrol;
    patrol.type = AIPatrolComponent::PatrolType::Loop;

    patrol.advance_waypoint(); // Should not crash
    REQUIRE(patrol.current_waypoint == 0);
}

// ============================================================================
// AIInvestigateComponent Tests
// ============================================================================

TEST_CASE("AIInvestigateComponent defaults", "[ai][components]") {
    AIInvestigateComponent investigate;

    REQUIRE(investigate.is_investigating == false);
    REQUIRE_THAT(investigate.investigation_point.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(investigate.investigation_point.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(investigate.investigation_point.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(investigate.investigation_time, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(investigate.max_investigation_time, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(investigate.search_radius, WithinAbs(5.0f, 0.001f));
    REQUIRE(investigate.search_points_checked == 0);
    REQUIRE(investigate.max_search_points == 3);
}

TEST_CASE("AIInvestigateComponent custom values", "[ai][components]") {
    AIInvestigateComponent investigate;
    investigate.is_investigating = true;
    investigate.investigation_point = Vec3{10.0f, 0.0f, 15.0f};
    investigate.investigation_time = 3.5f;
    investigate.search_points_checked = 2;

    REQUIRE(investigate.is_investigating == true);
    REQUIRE_THAT(investigate.investigation_point.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(investigate.investigation_point.z, WithinAbs(15.0f, 0.001f));
    REQUIRE_THAT(investigate.investigation_time, WithinAbs(3.5f, 0.001f));
    REQUIRE(investigate.search_points_checked == 2);
}

// ============================================================================
// AI Events Tests
// ============================================================================

TEST_CASE("AIStateChangedEvent", "[ai][events]") {
    AIStateChangedEvent event;
    event.entity = Entity{42};
    event.old_state = "Patrol";
    event.new_state = "Combat";

    REQUIRE(event.entity == Entity{42});
    REQUIRE(event.old_state == "Patrol");
    REQUIRE(event.new_state == "Combat");
}

TEST_CASE("AITargetChangedEvent", "[ai][events]") {
    AITargetChangedEvent event;
    event.entity = Entity{1};
    event.old_target = NullEntity;
    event.new_target = Entity{100};

    REQUIRE(event.entity == Entity{1});
    REQUIRE(event.old_target == NullEntity);
    REQUIRE(event.new_target == Entity{100});
}
