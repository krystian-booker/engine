#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/effects/effect_component.hpp>

using namespace engine::effects;
using namespace engine::scene;
using Catch::Matchers::WithinAbs;

TEST_CASE("ActiveEffectsComponent defaults", "[effects][component]") {
    ActiveEffectsComponent comp;

    REQUIRE(comp.effects.empty());
    REQUIRE(comp.immunities.empty());
    REQUIRE(comp.category_immunities.empty());
    REQUIRE(comp.tag_immunities.empty());
    REQUIRE(comp.max_effects == 0);
    REQUIRE(comp.count() == 0);
}

TEST_CASE("ActiveEffectsComponent effect queries", "[effects][component]") {
    ActiveEffectsComponent comp;

    EffectInstance effect1;
    effect1.definition_id = "poison";
    effect1.state = EffectState::Active;
    comp.effects.push_back(effect1);

    EffectInstance effect2;
    effect2.definition_id = "strength";
    effect2.state = EffectState::Active;
    comp.effects.push_back(effect2);

    SECTION("has_effect") {
        REQUIRE(comp.has_effect("poison"));
        REQUIRE(comp.has_effect("strength"));
        REQUIRE_FALSE(comp.has_effect("haste"));
    }

    SECTION("get_effect") {
        auto* effect = comp.get_effect("poison");
        REQUIRE(effect != nullptr);
        REQUIRE(effect->definition_id == "poison");

        auto* not_found = comp.get_effect("haste");
        REQUIRE(not_found == nullptr);
    }

    SECTION("count") {
        REQUIRE(comp.count() == 2);
    }
}

TEST_CASE("ActiveEffectsComponent immunity", "[effects][component]") {
    ActiveEffectsComponent comp;

    SECTION("Effect immunity") {
        comp.add_immunity("poison");
        REQUIRE(comp.is_immune_to("poison"));
        REQUIRE_FALSE(comp.is_immune_to("fire"));

        comp.remove_immunity("poison");
        REQUIRE_FALSE(comp.is_immune_to("poison"));
    }

    SECTION("Category immunity") {
        comp.add_category_immunity(EffectCategory::Debuff);
        REQUIRE(comp.is_immune_to_category(EffectCategory::Debuff));
        REQUIRE_FALSE(comp.is_immune_to_category(EffectCategory::Buff));

        comp.remove_category_immunity(EffectCategory::Debuff);
        REQUIRE_FALSE(comp.is_immune_to_category(EffectCategory::Debuff));
    }

    SECTION("Tag immunity") {
        comp.add_tag_immunity("fire");
        std::vector<std::string> tags_with_fire = {"fire", "magic"};
        std::vector<std::string> tags_without_fire = {"ice", "magic"};

        REQUIRE(comp.is_immune_to_tags(tags_with_fire));
        REQUIRE_FALSE(comp.is_immune_to_tags(tags_without_fire));

        comp.remove_tag_immunity("fire");
        REQUIRE_FALSE(comp.is_immune_to_tags(tags_with_fire));
    }

    SECTION("Clear immunities") {
        comp.add_immunity("poison");
        comp.add_category_immunity(EffectCategory::Debuff);
        comp.add_tag_immunity("fire");

        comp.clear_immunities();

        REQUIRE_FALSE(comp.is_immune_to("poison"));
        REQUIRE_FALSE(comp.is_immune_to_category(EffectCategory::Debuff));
    }
}

TEST_CASE("EffectSourceComponent defaults", "[effects][component]") {
    EffectSourceComponent comp;

    REQUIRE_THAT(comp.duration_multiplier, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(comp.damage_multiplier, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(comp.heal_multiplier, WithinAbs(1.0f, 0.001f));
    REQUIRE(comp.bonus_stacks == 0);
    REQUIRE(comp.passive_effects.empty());
    REQUIRE(comp.apply_chance_modifiers.empty());
}

TEST_CASE("EffectSourceComponent custom values", "[effects][component]") {
    EffectSourceComponent comp;
    comp.duration_multiplier = 1.5f;
    comp.damage_multiplier = 1.2f;
    comp.bonus_stacks = 2;
    comp.passive_effects = {"aura_fire", "aura_strength"};
    comp.apply_chance_modifiers["poison"] = 0.5f;

    REQUIRE_THAT(comp.duration_multiplier, WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(comp.damage_multiplier, WithinAbs(1.2f, 0.001f));
    REQUIRE(comp.bonus_stacks == 2);
    REQUIRE(comp.passive_effects.size() == 2);
    REQUIRE_THAT(comp.apply_chance_modifiers["poison"], WithinAbs(0.5f, 0.001f));
}

TEST_CASE("EffectAuraComponent defaults", "[effects][component]") {
    EffectAuraComponent comp;

    REQUIRE(comp.effect_id.empty());
    REQUIRE_THAT(comp.radius, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(comp.apply_interval, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(comp.time_since_apply, WithinAbs(0.0f, 0.001f));
    REQUIRE(comp.affects_self == false);
    REQUIRE(comp.affects_allies == true);
    REQUIRE(comp.affects_enemies == true);
    REQUIRE(comp.faction.empty());
    REQUIRE(comp.max_targets == 0);
    REQUIRE(comp.affected_entities.empty());
}

TEST_CASE("EffectAuraComponent custom values", "[effects][component]") {
    EffectAuraComponent comp;
    comp.effect_id = "healing_aura";
    comp.radius = 10.0f;
    comp.apply_interval = 0.5f;
    comp.affects_self = true;
    comp.affects_enemies = false;
    comp.faction = "player";
    comp.max_targets = 5;

    REQUIRE(comp.effect_id == "healing_aura");
    REQUIRE_THAT(comp.radius, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(comp.apply_interval, WithinAbs(0.5f, 0.001f));
    REQUIRE(comp.affects_self == true);
    REQUIRE(comp.affects_enemies == false);
    REQUIRE(comp.faction == "player");
    REQUIRE(comp.max_targets == 5);
}
