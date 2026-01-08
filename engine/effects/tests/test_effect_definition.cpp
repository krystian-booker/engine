#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/effects/effect_definition.hpp>

using namespace engine::effects;
using Catch::Matchers::WithinAbs;

TEST_CASE("EffectCategory enum", "[effects][definition]") {
    REQUIRE(static_cast<uint8_t>(EffectCategory::Buff) == 0);
    REQUIRE(static_cast<uint8_t>(EffectCategory::Debuff) == 1);
    REQUIRE(static_cast<uint8_t>(EffectCategory::Neutral) == 2);
    REQUIRE(static_cast<uint8_t>(EffectCategory::Passive) == 3);
    REQUIRE(static_cast<uint8_t>(EffectCategory::Aura) == 4);
}

TEST_CASE("StackBehavior enum", "[effects][definition]") {
    REQUIRE(static_cast<uint8_t>(StackBehavior::None) == 0);
    REQUIRE(static_cast<uint8_t>(StackBehavior::Refresh) == 1);
    REQUIRE(static_cast<uint8_t>(StackBehavior::RefreshExtend) == 2);
    REQUIRE(static_cast<uint8_t>(StackBehavior::Intensity) == 3);
    REQUIRE(static_cast<uint8_t>(StackBehavior::IntensityRefresh) == 4);
    REQUIRE(static_cast<uint8_t>(StackBehavior::Independent) == 5);
}

TEST_CASE("EffectFlags enum", "[effects][definition]") {
    REQUIRE(static_cast<uint32_t>(EffectFlags::None) == 0);
    REQUIRE(static_cast<uint32_t>(EffectFlags::Dispellable) == 1);
    REQUIRE(static_cast<uint32_t>(EffectFlags::Purgeable) == 2);
    REQUIRE(static_cast<uint32_t>(EffectFlags::Hidden) == 4);
    REQUIRE(static_cast<uint32_t>(EffectFlags::Persistent) == 8);
    REQUIRE(static_cast<uint32_t>(EffectFlags::Unique) == 16);
    REQUIRE(static_cast<uint32_t>(EffectFlags::Stackable) == 32);
    REQUIRE(static_cast<uint32_t>(EffectFlags::Refreshable) == 64);
    REQUIRE(static_cast<uint32_t>(EffectFlags::NoSave) == 128);
    REQUIRE(static_cast<uint32_t>(EffectFlags::Inheritable) == 256);
}

TEST_CASE("EffectFlags operators", "[effects][definition]") {
    SECTION("OR operator") {
        auto combined = EffectFlags::Dispellable | EffectFlags::Stackable;
        REQUIRE(static_cast<uint32_t>(combined) == 33);
    }

    SECTION("AND operator") {
        auto combined = EffectFlags::Dispellable | EffectFlags::Stackable;
        auto result = combined & EffectFlags::Stackable;
        REQUIRE(static_cast<uint32_t>(result) == 32);
    }

    SECTION("has_flag helper") {
        auto flags = EffectFlags::Dispellable | EffectFlags::Hidden;

        REQUIRE(has_flag(flags, EffectFlags::Dispellable));
        REQUIRE(has_flag(flags, EffectFlags::Hidden));
        REQUIRE_FALSE(has_flag(flags, EffectFlags::Stackable));
    }
}

TEST_CASE("EffectTrigger enum", "[effects][definition]") {
    REQUIRE(static_cast<uint8_t>(EffectTrigger::OnApply) == 0);
    REQUIRE(static_cast<uint8_t>(EffectTrigger::OnRefresh) == 1);
    REQUIRE(static_cast<uint8_t>(EffectTrigger::OnTick) == 2);
    REQUIRE(static_cast<uint8_t>(EffectTrigger::OnExpire) == 3);
    REQUIRE(static_cast<uint8_t>(EffectTrigger::OnRemove) == 4);
    REQUIRE(static_cast<uint8_t>(EffectTrigger::OnStack) == 5);
}

TEST_CASE("EffectDefinition defaults", "[effects][definition]") {
    EffectDefinition def;

    REQUIRE(def.effect_id.empty());
    REQUIRE(def.display_name.empty());
    REQUIRE(def.description.empty());
    REQUIRE(def.category == EffectCategory::Buff);
    REQUIRE_THAT(def.base_duration, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(def.max_duration, WithinAbs(30.0f, 0.001f));
    REQUIRE(def.stacking == StackBehavior::RefreshExtend);
    REQUIRE(def.max_stacks == 1);
    REQUIRE_THAT(def.tick_interval, WithinAbs(0.0f, 0.001f));
    REQUIRE(def.tick_on_apply == false);
    REQUIRE_THAT(def.damage_per_tick, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(def.heal_per_tick, WithinAbs(0.0f, 0.001f));
    REQUIRE(def.dispel_priority == 0);
    REQUIRE_THAT(def.intensity_per_stack, WithinAbs(1.0f, 0.001f));
    REQUIRE(def.scale_duration_with_stacks == false);
}

TEST_CASE("EffectDefinition helpers", "[effects][definition]") {
    EffectDefinition def;

    SECTION("is_buff") {
        def.category = EffectCategory::Buff;
        REQUIRE(def.is_buff());
        REQUIRE_FALSE(def.is_debuff());
    }

    SECTION("is_debuff") {
        def.category = EffectCategory::Debuff;
        REQUIRE(def.is_debuff());
        REQUIRE_FALSE(def.is_buff());
    }

    SECTION("is_dispellable") {
        def.flags = EffectFlags::Dispellable;
        REQUIRE(def.is_dispellable());
    }

    SECTION("is_hidden") {
        def.flags = EffectFlags::Hidden;
        REQUIRE(def.is_hidden());
    }

    SECTION("has_ticking") {
        def.tick_interval = 0.0f;
        REQUIRE_FALSE(def.has_ticking());

        def.tick_interval = 1.0f;
        REQUIRE(def.has_ticking());
    }

    SECTION("is_permanent") {
        def.base_duration = 10.0f;
        REQUIRE_FALSE(def.is_permanent());

        def.base_duration = 0.0f;
        REQUIRE(def.is_permanent());

        def.base_duration = -1.0f;
        REQUIRE(def.is_permanent());
    }

    SECTION("can_stack") {
        def.max_stacks = 1;
        REQUIRE_FALSE(def.can_stack());

        def.max_stacks = 5;
        REQUIRE(def.can_stack());
    }
}

TEST_CASE("EffectBuilder fluent API", "[effects][definition]") {
    auto def = effect()
        .id("poison")
        .name("Poison")
        .description("Deals damage over time")
        .debuff()
        .duration(15.0f)
        .tick(2.0f)
        .damage_per_tick(5.0f, "poison")
        .stacking(StackBehavior::Intensity, 5)
        .tag("dot")
        .tag("nature")
        .dispellable()
        .build();

    REQUIRE(def.effect_id == "poison");
    REQUIRE(def.display_name == "Poison");
    REQUIRE(def.category == EffectCategory::Debuff);
    REQUIRE_THAT(def.base_duration, WithinAbs(15.0f, 0.001f));
    REQUIRE_THAT(def.tick_interval, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(def.damage_per_tick, WithinAbs(5.0f, 0.001f));
    REQUIRE(def.damage_type == "poison");
    REQUIRE(def.stacking == StackBehavior::Intensity);
    REQUIRE(def.max_stacks == 5);
    REQUIRE(def.tags.size() == 2);
}
