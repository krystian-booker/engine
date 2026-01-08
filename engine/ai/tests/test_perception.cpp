// engine/ai/tests/test_perception.cpp
// Happy path tests for AI Perception components

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <engine/ai/perception.hpp>
#include <engine/ai/ai_components.hpp>
#include <engine/scene/entity.hpp>

using namespace engine::ai;
using namespace engine::core;
using namespace engine::scene;
using Catch::Matchers::WithinAbs;

TEST_CASE("AIPerceptionComponent defaults", "[ai][perception]") {
    AIPerceptionComponent perception;

    SECTION("Enabled by default") {
        REQUIRE(perception.enabled == true);
    }

    SECTION("Sight enabled by default") {
        REQUIRE(perception.sight_enabled == true);
        REQUIRE_THAT(perception.sight_range, WithinAbs(20.0f, 0.1f));
        REQUIRE_THAT(perception.sight_angle, WithinAbs(120.0f, 0.1f));
    }

    SECTION("Hearing enabled by default") {
        REQUIRE(perception.hearing_enabled == true);
        REQUIRE_THAT(perception.hearing_range, WithinAbs(15.0f, 0.1f));
    }

    SECTION("Awareness defaults") {
        REQUIRE_THAT(perception.awareness_threshold, WithinAbs(0.8f, 0.01f));
        REQUIRE_THAT(perception.awareness_gain_rate, WithinAbs(2.0f, 0.1f));
        REQUIRE_THAT(perception.awareness_decay_rate, WithinAbs(0.5f, 0.1f));
    }

    SECTION("Memory defaults") {
        REQUIRE_THAT(perception.memory_duration, WithinAbs(10.0f, 0.1f));
    }

    SECTION("Faction defaults") {
        REQUIRE(perception.faction == "enemy");
        REQUIRE(perception.hostile_factions.size() == 1);
        REQUIRE(perception.hostile_factions[0] == "player");
    }

    SECTION("Perceived entities starts empty") {
        REQUIRE(perception.perceived_entities.empty());
    }
}

TEST_CASE("PerceivedEntity struct", "[ai][perception]") {
    PerceivedEntity pe;

    SECTION("Default values") {
        REQUIRE(pe.entity == NullEntity);
        REQUIRE(pe.sense == PerceptionSense::Sight);
        REQUIRE_THAT(pe.stimulation, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(pe.awareness, WithinAbs(0.0f, 0.01f));
        REQUIRE(pe.currently_perceived == false);
        REQUIRE(pe.is_hostile == false);
    }

    SECTION("Can set properties") {
        pe.entity = Entity{42};
        pe.sense = PerceptionSense::Hearing;
        pe.awareness = 0.75f;
        pe.currently_perceived = true;
        pe.is_hostile = true;
        pe.last_known_position = Vec3{10.0f, 0.0f, 5.0f};

        REQUIRE(pe.entity == Entity{42});
        REQUIRE(pe.sense == PerceptionSense::Hearing);
        REQUIRE_THAT(pe.awareness, WithinAbs(0.75f, 0.01f));
        REQUIRE(pe.currently_perceived == true);
        REQUIRE(pe.is_hostile == true);
    }
}

TEST_CASE("AIPerceptionComponent helper methods", "[ai][perception]") {
    AIPerceptionComponent perception;

    // Add some perceived entities for testing
    PerceivedEntity friendly;
    friendly.entity = Entity{1};
    friendly.is_hostile = false;
    friendly.awareness = 1.0f;
    friendly.currently_perceived = true;
    friendly.last_known_position = Vec3{5.0f, 0.0f, 0.0f};

    PerceivedEntity hostile1;
    hostile1.entity = Entity{2};
    hostile1.is_hostile = true;
    hostile1.awareness = 0.5f; // Below threshold
    hostile1.currently_perceived = true;
    hostile1.stimulation = 0.8f;
    hostile1.last_known_position = Vec3{10.0f, 0.0f, 0.0f};

    PerceivedEntity hostile2;
    hostile2.entity = Entity{3};
    hostile2.is_hostile = true;
    hostile2.awareness = 0.9f; // Above threshold
    hostile2.currently_perceived = true;
    hostile2.stimulation = 1.0f;
    hostile2.last_known_position = Vec3{3.0f, 0.0f, 0.0f};

    perception.perceived_entities = {friendly, hostile1, hostile2};

    SECTION("has_threat returns true when hostile above threshold") {
        REQUIRE(perception.has_threat() == true);
    }

    SECTION("has_threat returns false when no hostile above threshold") {
        perception.perceived_entities = {friendly, hostile1}; // Only hostile below threshold
        REQUIRE(perception.has_threat() == false);
    }

    SECTION("get_primary_threat returns highest threat entity") {
        Entity primary = perception.get_primary_threat();
        // hostile2 has higher awareness * stimulation * 2 (currently_perceived)
        REQUIRE(primary == Entity{3});
    }

    SECTION("get_primary_threat returns NullEntity when no hostiles") {
        perception.perceived_entities = {friendly};
        REQUIRE(perception.get_primary_threat() == NullEntity);
    }

    SECTION("get_nearest_threat returns closest above-threshold hostile") {
        Vec3 observer_pos{0.0f, 0.0f, 0.0f};
        Entity nearest = perception.get_nearest_threat(observer_pos);
        // hostile2 at distance 3 is closer than hostile1 at distance 10
        // but hostile1 is below threshold, so hostile2 should be returned
        REQUIRE(nearest == Entity{3});
    }

    SECTION("can_see returns true for currently perceived sight entity") {
        hostile2.sense = PerceptionSense::Sight;
        perception.perceived_entities = {hostile2};
        REQUIRE(perception.can_see(Entity{3}) == true);
    }

    SECTION("can_see returns false for not currently perceived") {
        hostile2.currently_perceived = false;
        perception.perceived_entities = {hostile2};
        REQUIRE(perception.can_see(Entity{3}) == false);
    }

    SECTION("can_see returns false for hearing sense") {
        hostile2.sense = PerceptionSense::Hearing;
        perception.perceived_entities = {hostile2};
        REQUIRE(perception.can_see(Entity{3}) == false);
    }

    SECTION("is_aware_of returns true when awareness >= threshold") {
        REQUIRE(perception.is_aware_of(Entity{3}) == true); // 0.9 >= 0.8
    }

    SECTION("is_aware_of returns false when awareness < threshold") {
        REQUIRE(perception.is_aware_of(Entity{2}) == false); // 0.5 < 0.8
    }

    SECTION("get_last_known_position returns position") {
        auto pos = perception.get_last_known_position(Entity{3});
        REQUIRE(pos.has_value());
        REQUIRE_THAT(pos->x, WithinAbs(3.0f, 0.01f));
    }

    SECTION("get_last_known_position returns nullopt for unknown entity") {
        auto pos = perception.get_last_known_position(Entity{999});
        REQUIRE_FALSE(pos.has_value());
    }

    SECTION("get_awareness_of returns awareness level") {
        REQUIRE_THAT(perception.get_awareness_of(Entity{3}), WithinAbs(0.9f, 0.01f));
    }

    SECTION("get_awareness_of returns 0 for unknown entity") {
        REQUIRE_THAT(perception.get_awareness_of(Entity{999}), WithinAbs(0.0f, 0.01f));
    }

    SECTION("get_predicted_position extrapolates from velocity") {
        hostile2.last_known_velocity = Vec3{1.0f, 0.0f, 0.0f};
        perception.perceived_entities = {hostile2};

        Vec3 predicted = perception.get_predicted_position(Entity{3}, 2.0f);
        // 3 + 1*2 = 5
        REQUIRE_THAT(predicted.x, WithinAbs(5.0f, 0.01f));
    }
}

TEST_CASE("AICombatComponent", "[ai][components]") {
    AICombatComponent combat;

    SECTION("Default values") {
        REQUIRE_THAT(combat.attack_range, WithinAbs(2.0f, 0.1f));
        REQUIRE_THAT(combat.ranged_attack_range, WithinAbs(15.0f, 0.1f));
        REQUIRE_THAT(combat.attack_cooldown, WithinAbs(1.5f, 0.1f));
    }

    SECTION("can_attack respects cooldown") {
        combat.time_since_attack = 0.0f;
        REQUIRE(combat.can_attack() == false);

        combat.time_since_attack = 2.0f;
        REQUIRE(combat.can_attack() == true);
    }

    SECTION("can_attack returns false when attacking") {
        combat.time_since_attack = 10.0f;
        combat.is_attacking = true;
        REQUIRE(combat.can_attack() == false);
    }

    SECTION("can_attack returns false when staggered") {
        combat.time_since_attack = 10.0f;
        combat.is_staggered = true;
        REQUIRE(combat.can_attack() == false);
    }

    SECTION("in_attack_range") {
        REQUIRE(combat.in_attack_range(1.5f) == true);
        REQUIRE(combat.in_attack_range(2.5f) == false);
    }

    SECTION("in_ranged_range") {
        REQUIRE(combat.in_ranged_range(10.0f) == true);  // 2 < 10 < 15
        REQUIRE(combat.in_ranged_range(1.0f) == false);  // Too close (melee range)
        REQUIRE(combat.in_ranged_range(20.0f) == false); // Too far
    }

    SECTION("start_attack and end_attack") {
        combat.start_attack();
        REQUIRE(combat.is_attacking == true);
        REQUIRE_THAT(combat.time_since_attack, WithinAbs(0.0f, 0.01f));

        combat.end_attack();
        REQUIRE(combat.is_attacking == false);
        REQUIRE(combat.current_combo == 1);
    }

    SECTION("combo resets at max") {
        combat.max_combo = 3;
        combat.current_combo = 2;
        combat.end_attack();
        REQUIRE(combat.current_combo == 0);
    }
}

TEST_CASE("AIPatrolComponent", "[ai][components]") {
    AIPatrolComponent patrol;

    SECTION("Default values") {
        REQUIRE(patrol.type == AIPatrolComponent::PatrolType::Loop);
        REQUIRE(patrol.waypoints.empty());
        REQUIRE(patrol.current_waypoint == 0);
        REQUIRE_THAT(patrol.patrol_speed, WithinAbs(2.0f, 0.1f));
    }

    SECTION("get_current_waypoint with empty waypoints") {
        Vec3 wp = patrol.get_current_waypoint();
        REQUIRE_THAT(wp.x, WithinAbs(0.0f, 0.01f));
    }

    SECTION("get_current_waypoint returns correct waypoint") {
        patrol.waypoints = {Vec3{1, 0, 0}, Vec3{2, 0, 0}, Vec3{3, 0, 0}};
        patrol.current_waypoint = 1;

        Vec3 wp = patrol.get_current_waypoint();
        REQUIRE_THAT(wp.x, WithinAbs(2.0f, 0.01f));
    }

    SECTION("advance_waypoint Loop mode") {
        patrol.type = AIPatrolComponent::PatrolType::Loop;
        patrol.waypoints = {Vec3{1, 0, 0}, Vec3{2, 0, 0}, Vec3{3, 0, 0}};
        patrol.current_waypoint = 0;

        patrol.advance_waypoint();
        REQUIRE(patrol.current_waypoint == 1);

        patrol.advance_waypoint();
        REQUIRE(patrol.current_waypoint == 2);

        patrol.advance_waypoint();
        REQUIRE(patrol.current_waypoint == 0); // Loops back
    }

    SECTION("advance_waypoint PingPong mode") {
        patrol.type = AIPatrolComponent::PatrolType::PingPong;
        patrol.waypoints = {Vec3{1, 0, 0}, Vec3{2, 0, 0}, Vec3{3, 0, 0}};
        patrol.current_waypoint = 0;
        patrol.reverse_direction = false;

        patrol.advance_waypoint();
        REQUIRE(patrol.current_waypoint == 1);

        patrol.advance_waypoint();
        REQUIRE(patrol.current_waypoint == 2);
        REQUIRE(patrol.reverse_direction == true);

        patrol.advance_waypoint();
        REQUIRE(patrol.current_waypoint == 1);

        patrol.advance_waypoint();
        REQUIRE(patrol.current_waypoint == 0);
        REQUIRE(patrol.reverse_direction == false);
    }
}

TEST_CASE("AIControllerComponent", "[ai][components]") {
    AIControllerComponent controller;

    SECTION("Default values") {
        REQUIRE(controller.enabled == true);
        REQUIRE(controller.behavior_tree == nullptr);
        REQUIRE(controller.blackboard == nullptr);
        REQUIRE_THAT(controller.update_interval, WithinAbs(0.1f, 0.01f));
    }

    SECTION("ensure_blackboard creates blackboard") {
        REQUIRE(controller.blackboard == nullptr);
        controller.ensure_blackboard();
        REQUIRE(controller.blackboard != nullptr);

        // Calling again doesn't create new one
        auto* first = controller.blackboard.get();
        controller.ensure_blackboard();
        REQUIRE(controller.blackboard.get() == first);
    }

    SECTION("should_update respects interval") {
        controller.update_interval = 0.1f;
        controller.time_since_update = 0.0f;

        REQUIRE(controller.should_update(0.05f) == false);
        REQUIRE(controller.should_update(0.05f) == true);
        // After update, time resets
        REQUIRE(controller.should_update(0.05f) == false);
    }
}

TEST_CASE("AIInvestigateComponent", "[ai][components]") {
    AIInvestigateComponent investigate;

    SECTION("Default values") {
        REQUIRE(investigate.is_investigating == false);
        REQUIRE_THAT(investigate.max_investigation_time, WithinAbs(10.0f, 0.1f));
        REQUIRE_THAT(investigate.search_radius, WithinAbs(5.0f, 0.1f));
        REQUIRE(investigate.search_points_checked == 0);
    }
}

TEST_CASE("AINoiseEmitterComponent", "[ai][components]") {
    AINoiseEmitterComponent emitter;

    SECTION("Default values") {
        REQUIRE(emitter.enabled == true);
        REQUIRE_THAT(emitter.noise_radius, WithinAbs(5.0f, 0.1f));
        REQUIRE_THAT(emitter.loudness, WithinAbs(1.0f, 0.01f));
        REQUIRE(emitter.is_continuous == false);
        REQUIRE(emitter.noise_type == "generic");
        REQUIRE(emitter.trigger_noise == false);
    }
}

TEST_CASE("PerceptionSense enum", "[ai][perception]") {
    REQUIRE(static_cast<uint8_t>(PerceptionSense::Sight) == 0);
    REQUIRE(static_cast<uint8_t>(PerceptionSense::Hearing) == 1);
    REQUIRE(static_cast<uint8_t>(PerceptionSense::Damage) == 2);
}
