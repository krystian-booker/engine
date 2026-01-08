#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/effects/effect_instance.hpp>

using namespace engine::effects;
using namespace engine::scene;
using Catch::Matchers::WithinAbs;

TEST_CASE("EffectState enum", "[effects][instance]") {
    REQUIRE(static_cast<uint8_t>(EffectState::Pending) == 0);
    REQUIRE(static_cast<uint8_t>(EffectState::Active) == 1);
    REQUIRE(static_cast<uint8_t>(EffectState::Paused) == 2);
    REQUIRE(static_cast<uint8_t>(EffectState::Expiring) == 3);
    REQUIRE(static_cast<uint8_t>(EffectState::Expired) == 4);
    REQUIRE(static_cast<uint8_t>(EffectState::Removed) == 5);
    REQUIRE(static_cast<uint8_t>(EffectState::Blocked) == 6);
}

TEST_CASE("EffectInstance defaults", "[effects][instance]") {
    EffectInstance instance;

    REQUIRE(instance.instance_id.is_null());
    REQUIRE(instance.definition_id.empty());
    REQUIRE(instance.target == NullEntity);
    REQUIRE(instance.source == NullEntity);
    REQUIRE(instance.state == EffectState::Pending);
    REQUIRE_THAT(instance.duration, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(instance.remaining, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(instance.elapsed, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(instance.tick_timer, WithinAbs(0.0f, 0.001f));
    REQUIRE(instance.stacks == 1);
    REQUIRE_THAT(instance.intensity, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(instance.damage_multiplier, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(instance.heal_multiplier, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(instance.duration_multiplier, WithinAbs(1.0f, 0.001f));
    REQUIRE(instance.applied_modifier_ids.empty());
    REQUIRE(instance.apply_timestamp == 0);
}

TEST_CASE("EffectInstance state queries", "[effects][instance]") {
    EffectInstance instance;

    SECTION("is_active") {
        instance.state = EffectState::Active;
        REQUIRE(instance.is_active());
        REQUIRE_FALSE(instance.is_expired());
    }

    SECTION("is_expired - Expired state") {
        instance.state = EffectState::Expired;
        REQUIRE(instance.is_expired());
        REQUIRE_FALSE(instance.is_active());
    }

    SECTION("is_expired - Removed state") {
        instance.state = EffectState::Removed;
        REQUIRE(instance.is_expired());
    }

    SECTION("is_permanent") {
        instance.duration = 10.0f;
        REQUIRE_FALSE(instance.is_permanent());

        instance.duration = 0.0f;
        REQUIRE(instance.is_permanent());
    }
}

TEST_CASE("EffectInstance time queries", "[effects][instance]") {
    EffectInstance instance;
    instance.duration = 10.0f;
    instance.remaining = 7.5f;
    instance.elapsed = 2.5f;

    SECTION("get_remaining_percent") {
        float percent = instance.get_remaining_percent();
        REQUIRE_THAT(percent, WithinAbs(0.75f, 0.01f));
    }

    SECTION("get_elapsed_percent") {
        float percent = instance.get_elapsed_percent();
        REQUIRE_THAT(percent, WithinAbs(0.25f, 0.01f));
    }
}

TEST_CASE("EffectInstance stack helpers", "[effects][instance]") {
    EffectInstance instance;
    instance.stacks = 1;

    SECTION("add_stack") {
        instance.add_stack();
        REQUIRE(instance.stacks == 2);

        instance.add_stack(3);
        REQUIRE(instance.stacks == 5);
    }

    SECTION("remove_stack") {
        instance.stacks = 5;
        instance.remove_stack();
        REQUIRE(instance.stacks == 4);

        instance.remove_stack(2);
        REQUIRE(instance.stacks == 2);
    }

    SECTION("set_stacks") {
        instance.set_stacks(10);
        REQUIRE(instance.stacks == 10);
    }
}

TEST_CASE("EffectInstance duration helpers", "[effects][instance]") {
    EffectInstance instance;
    instance.duration = 10.0f;
    instance.remaining = 5.0f;

    SECTION("refresh_duration") {
        instance.refresh_duration();
        REQUIRE_THAT(instance.remaining, WithinAbs(10.0f, 0.001f));
    }

    SECTION("extend_duration") {
        instance.extend_duration(3.0f);
        REQUIRE_THAT(instance.remaining, WithinAbs(8.0f, 0.001f));
    }
}

TEST_CASE("EffectInstance custom data", "[effects][instance]") {
    EffectInstance instance;

    instance.custom_float_data["damage_bonus"] = 1.5f;
    instance.custom_string_data["source_name"] = "Poison Trap";

    REQUIRE_THAT(instance.custom_float_data["damage_bonus"], WithinAbs(1.5f, 0.001f));
    REQUIRE(instance.custom_string_data["source_name"] == "Poison Trap");
}

TEST_CASE("ApplyResult enum", "[effects][instance]") {
    REQUIRE(static_cast<uint8_t>(ApplyResult::Applied) == 0);
    REQUIRE(static_cast<uint8_t>(ApplyResult::Refreshed) == 1);
    REQUIRE(static_cast<uint8_t>(ApplyResult::Extended) == 2);
    REQUIRE(static_cast<uint8_t>(ApplyResult::Stacked) == 3);
    REQUIRE(static_cast<uint8_t>(ApplyResult::StackedAndRefreshed) == 4);
    REQUIRE(static_cast<uint8_t>(ApplyResult::AlreadyAtMax) == 5);
    REQUIRE(static_cast<uint8_t>(ApplyResult::Blocked) == 6);
    REQUIRE(static_cast<uint8_t>(ApplyResult::TargetInvalid) == 7);
    REQUIRE(static_cast<uint8_t>(ApplyResult::DefinitionNotFound) == 8);
    REQUIRE(static_cast<uint8_t>(ApplyResult::Failed) == 9);
}

TEST_CASE("ApplyResultInfo defaults", "[effects][instance]") {
    ApplyResultInfo info;

    REQUIRE(info.instance == nullptr);
    REQUIRE(info.new_stack_count == 0);
    REQUIRE_THAT(info.new_duration, WithinAbs(0.0f, 0.001f));
    REQUIRE(info.blocked_by.empty());
}

TEST_CASE("RemovalReason enum", "[effects][instance]") {
    REQUIRE(static_cast<uint8_t>(RemovalReason::Expired) == 0);
    REQUIRE(static_cast<uint8_t>(RemovalReason::Dispelled) == 1);
    REQUIRE(static_cast<uint8_t>(RemovalReason::Purged) == 2);
    REQUIRE(static_cast<uint8_t>(RemovalReason::Replaced) == 3);
    REQUIRE(static_cast<uint8_t>(RemovalReason::Cancelled) == 4);
    REQUIRE(static_cast<uint8_t>(RemovalReason::Death) == 5);
    REQUIRE(static_cast<uint8_t>(RemovalReason::SourceDeath) == 6);
    REQUIRE(static_cast<uint8_t>(RemovalReason::StacksDepleted) == 7);
    REQUIRE(static_cast<uint8_t>(RemovalReason::GameLogic) == 8);
}
