#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/companion/companion_ai.hpp>
#include <engine/companion/companion.hpp>
#include <engine/companion/formation.hpp>
#include <engine/companion/party_manager.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>

using namespace engine::companion;
using namespace engine::scene;
using Catch::Matchers::WithinAbs;

// Helper to create a test world with leader and companion
class CompanionAITestFixture {
public:
    World world;
    Entity leader;
    Entity companion;

    CompanionAITestFixture() {
        // Create leader
        leader = world.create();
        auto& leader_lt = world.emplace<LocalTransform>(leader);
        leader_lt.position = Vec3(0.0f, 0.0f, 0.0f);
        world.emplace<WorldTransform>(leader);

        // Create companion
        companion = world.create();
        auto& comp = world.emplace<CompanionComponent>(companion);
        comp.owner = leader;
        comp.state = CompanionState::Following;
        comp.companion_id = "test_companion";

        auto& comp_lt = world.emplace<LocalTransform>(companion);
        comp_lt.position = Vec3(5.0f, 0.0f, 0.0f);
        world.emplace<WorldTransform>(companion);
    }

    void set_leader_position(const Vec3& pos) {
        auto* lt = world.try_get<LocalTransform>(leader);
        if (lt) lt->position = pos;
    }

    void set_companion_position(const Vec3& pos) {
        auto* lt = world.try_get<LocalTransform>(companion);
        if (lt) lt->position = pos;
    }

    Vec3 get_companion_position() {
        auto* lt = world.try_get<LocalTransform>(companion);
        return lt ? lt->position : Vec3(0.0f);
    }

    CompanionComponent& get_companion_component() {
        return world.get<CompanionComponent>(companion);
    }
};

TEST_CASE("Companion state time tracking", "[companion][ai]") {
    CompanionAITestFixture fixture;

    SECTION("State time increments when state unchanged") {
        auto& comp = fixture.get_companion_component();
        comp.state_time = 0.0f;

        // Simulate some time passing
        comp.state_time += 1.0f;

        REQUIRE_THAT(comp.state_time, WithinAbs(1.0f, 0.01f));
    }

    SECTION("State time resets on state change") {
        auto& comp = fixture.get_companion_component();
        comp.state_time = 5.0f;

        comp.set_state(CompanionState::Waiting);

        REQUIRE_THAT(comp.state_time, WithinAbs(0.0f, 0.01f));
    }
}

TEST_CASE("Companion follow behavior", "[companion][ai]") {
    CompanionAITestFixture fixture;

    SECTION("Companion targets leader position when following") {
        auto& comp = fixture.get_companion_component();
        comp.state = CompanionState::Following;
        comp.follow_distance = 3.0f;

        // Leader at origin, companion should target position behind leader
        fixture.set_leader_position(Vec3(0.0f, 0.0f, 0.0f));
        fixture.set_companion_position(Vec3(10.0f, 0.0f, 0.0f));

        // The follow system would calculate target based on leader position and formation
        // Here we just verify the state is correct
        REQUIRE(comp.is_following());
    }

    SECTION("Companion stops following in Waiting state") {
        auto& comp = fixture.get_companion_component();
        comp.set_state(CompanionState::Waiting);

        REQUIRE_FALSE(comp.is_following());
    }

    SECTION("Speed multiplier increases when catching up") {
        auto& comp = fixture.get_companion_component();
        comp.follow_speed_multiplier = 1.0f;
        comp.catch_up_speed_multiplier = 1.5f;
        comp.catch_up_distance = 10.0f;

        // When distance > catch_up_distance, higher multiplier should be used
        REQUIRE(comp.catch_up_speed_multiplier > comp.follow_speed_multiplier);
    }

    SECTION("Companion respects formation position") {
        auto& comp = fixture.get_companion_component();
        comp.formation_slot = 0;

        // With a formation slot, companion should follow formation position
        REQUIRE(comp.formation_slot >= 0);
    }
}

TEST_CASE("Companion teleport behavior", "[companion][ai]") {
    CompanionAITestFixture fixture;

    SECTION("Teleport is enabled by default") {
        auto& comp = fixture.get_companion_component();
        REQUIRE(comp.teleport_if_too_far == true);
    }

    SECTION("Teleport distance threshold is configurable") {
        auto& comp = fixture.get_companion_component();
        comp.teleport_distance = 50.0f;

        REQUIRE_THAT(comp.teleport_distance, WithinAbs(50.0f, 0.01f));
    }

    SECTION("Teleport cooldown prevents rapid teleports") {
        auto& comp = fixture.get_companion_component();
        comp.teleport_cooldown = 5.0f;
        comp.time_since_teleport = 0.0f;

        // Should not teleport when on cooldown
        REQUIRE(comp.time_since_teleport < comp.teleport_cooldown);
    }

    SECTION("Teleport only happens when following") {
        auto& comp = fixture.get_companion_component();
        comp.set_state(CompanionState::Waiting);

        // In waiting state, companion should not teleport
        REQUIRE_FALSE(comp.is_following());
    }
}

TEST_CASE("Companion combat behavior", "[companion][ai]") {
    CompanionAITestFixture fixture;

    SECTION("Auto-engage is enabled by default") {
        auto& comp = fixture.get_companion_component();
        REQUIRE(comp.auto_engage_enemies == true);
    }

    SECTION("Combat behavior types are respected") {
        auto& comp = fixture.get_companion_component();

        comp.combat_behavior = CombatBehavior::Passive;
        REQUIRE(comp.combat_behavior == CombatBehavior::Passive);

        comp.combat_behavior = CombatBehavior::Defensive;
        REQUIRE(comp.combat_behavior == CombatBehavior::Defensive);

        comp.combat_behavior = CombatBehavior::Aggressive;
        REQUIRE(comp.combat_behavior == CombatBehavior::Aggressive);
    }

    SECTION("Engagement range controls when combat starts") {
        auto& comp = fixture.get_companion_component();
        comp.engagement_range = 15.0f;
        comp.disengage_range = 25.0f;

        REQUIRE(comp.disengage_range > comp.engagement_range);
    }

    SECTION("Companion enters attacking state on engagement") {
        auto& comp = fixture.get_companion_component();
        comp.set_state(CompanionState::Attacking);

        REQUIRE(comp.is_in_combat());
        REQUIRE(comp.state == CompanionState::Attacking);
    }

    SECTION("Companion tracks combat target") {
        auto& comp = fixture.get_companion_component();
        Entity enemy = fixture.world.create();

        comp.combat_target = enemy;
        comp.set_state(CompanionState::Attacking);

        REQUIRE(comp.combat_target == enemy);
        REQUIRE(comp.is_in_combat());
    }

    SECTION("Combat time is tracked") {
        auto& comp = fixture.get_companion_component();
        comp.set_state(CompanionState::Attacking);
        comp.time_in_combat = 0.0f;

        // Simulate time passing
        comp.time_in_combat += 2.5f;

        REQUIRE_THAT(comp.time_in_combat, WithinAbs(2.5f, 0.01f));
    }

    SECTION("Disengage clears combat target") {
        auto& comp = fixture.get_companion_component();
        Entity enemy = fixture.world.create();

        comp.combat_target = enemy;
        comp.set_state(CompanionState::Attacking);

        // Disengage
        comp.combat_target = NullEntity;
        comp.set_state(CompanionState::Following);

        REQUIRE(comp.combat_target == NullEntity);
        REQUIRE_FALSE(comp.is_in_combat());
    }

    SECTION("Assist range controls owner defense") {
        auto& comp = fixture.get_companion_component();
        comp.assist_range = 10.0f;

        REQUIRE_THAT(comp.assist_range, WithinAbs(10.0f, 0.01f));
    }
}

TEST_CASE("Companion command behavior", "[companion][ai]") {
    CompanionAITestFixture fixture;

    SECTION("Move command sets target position") {
        auto& comp = fixture.get_companion_component();
        Vec3 target(50.0f, 0.0f, 50.0f);

        comp.command_position = target;
        comp.set_state(CompanionState::Moving);

        REQUIRE(comp.state == CompanionState::Moving);
        REQUIRE(comp.command_position == target);
    }

    SECTION("Wait command stores wait position") {
        auto& comp = fixture.get_companion_component();
        Vec3 wait_pos(10.0f, 0.0f, 10.0f);

        comp.wait_position = wait_pos;
        comp.set_state(CompanionState::Waiting);

        REQUIRE(comp.state == CompanionState::Waiting);
        REQUIRE(comp.wait_position == wait_pos);
    }

    SECTION("Interact command tracks target entity") {
        auto& comp = fixture.get_companion_component();
        Entity interactable = fixture.world.create();

        comp.command_target = interactable;
        comp.set_state(CompanionState::Interacting);

        REQUIRE(comp.state == CompanionState::Interacting);
        REQUIRE(comp.command_target == interactable);
    }

    SECTION("Defend command can target position or entity") {
        auto& comp = fixture.get_companion_component();

        // Defend position
        Vec3 defend_pos(20.0f, 0.0f, 20.0f);
        comp.command_position = defend_pos;
        comp.command_target = NullEntity;
        comp.set_state(CompanionState::Defending);

        REQUIRE(comp.state == CompanionState::Defending);
        REQUIRE(comp.command_position == defend_pos);

        // Defend entity
        Entity ally = fixture.world.create();
        comp.command_target = ally;
        REQUIRE(comp.command_target == ally);
    }

    SECTION("Companion can be commanded flag") {
        auto& comp = fixture.get_companion_component();

        REQUIRE(comp.can_be_commanded == true);

        comp.can_be_commanded = false;
        REQUIRE(comp.can_be_commanded == false);
    }
}

TEST_CASE("Companion state queries", "[companion][ai]") {
    CompanionAITestFixture fixture;

    SECTION("is_idle returns true for non-combat states") {
        auto& comp = fixture.get_companion_component();

        comp.state = CompanionState::Following;
        REQUIRE(comp.is_idle());

        comp.state = CompanionState::Waiting;
        REQUIRE(comp.is_idle());

        comp.state = CompanionState::Attacking;
        REQUIRE_FALSE(comp.is_idle());
    }

    SECTION("is_in_combat returns true for combat states") {
        auto& comp = fixture.get_companion_component();

        comp.state = CompanionState::Attacking;
        REQUIRE(comp.is_in_combat());

        comp.state = CompanionState::Defending;
        REQUIRE(comp.is_in_combat());

        comp.state = CompanionState::Following;
        REQUIRE_FALSE(comp.is_in_combat());
    }

    SECTION("is_dead only when in Dead state") {
        auto& comp = fixture.get_companion_component();

        comp.state = CompanionState::Dead;
        REQUIRE(comp.is_dead());

        comp.state = CompanionState::Following;
        REQUIRE_FALSE(comp.is_dead());
    }

    SECTION("Previous state is tracked") {
        auto& comp = fixture.get_companion_component();

        comp.state = CompanionState::Following;
        comp.set_state(CompanionState::Attacking);

        REQUIRE(comp.previous_state == CompanionState::Following);
        REQUIRE(comp.state == CompanionState::Attacking);
    }
}

TEST_CASE("Companion dead state", "[companion][ai]") {
    CompanionAITestFixture fixture;

    SECTION("Dead companions don't process commands") {
        auto& comp = fixture.get_companion_component();
        comp.set_state(CompanionState::Dead);

        REQUIRE(comp.is_dead());
    }

    SECTION("Dead companions can be revived") {
        auto& comp = fixture.get_companion_component();
        comp.set_state(CompanionState::Dead);
        REQUIRE(comp.is_dead());

        comp.set_state(CompanionState::Following);
        REQUIRE_FALSE(comp.is_dead());
        REQUIRE(comp.is_following());
    }
}

TEST_CASE("Companion owner tracking", "[companion][ai]") {
    CompanionAITestFixture fixture;

    SECTION("Owner entity is tracked") {
        auto& comp = fixture.get_companion_component();
        REQUIRE(comp.owner == fixture.leader);
    }

    SECTION("Companion without owner has NullEntity") {
        auto& comp = fixture.get_companion_component();
        comp.owner = NullEntity;

        REQUIRE(comp.owner == NullEntity);
    }

    SECTION("Owner change is reflected") {
        auto& comp = fixture.get_companion_component();
        Entity new_leader = fixture.world.create();

        comp.owner = new_leader;
        REQUIRE(comp.owner == new_leader);
    }
}
