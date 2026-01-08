#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/stats/stat_component.hpp>

using namespace engine::stats;
using Catch::Matchers::WithinAbs;

TEST_CASE("StatsComponent default construction", "[stats][component]") {
    StatsComponent stats;

    REQUIRE(stats.base_values.empty());
    REQUIRE(stats.final_values.empty());
    REQUIRE(stats.current_resources.empty());
    REQUIRE(stats.modifiers.empty());
    REQUIRE(stats.needs_recalculation == true);
}

TEST_CASE("StatsComponent base value access", "[stats][component]") {
    StatsComponent stats;

    SECTION("Set and get base value") {
        stats.set_base(StatType::MaxHealth, 100.0f);
        REQUIRE_THAT(stats.get_base(StatType::MaxHealth), WithinAbs(100.0f, 0.001f));
    }

    SECTION("Get non-existent base returns 0") {
        REQUIRE_THAT(stats.get_base(StatType::Strength), WithinAbs(0.0f, 0.001f));
    }

    SECTION("Add to base value") {
        stats.set_base(StatType::Strength, 10.0f);
        stats.add_base(StatType::Strength, 5.0f);
        REQUIRE_THAT(stats.get_base(StatType::Strength), WithinAbs(15.0f, 0.001f));
    }
}

TEST_CASE("StatsComponent final value access", "[stats][component]") {
    StatsComponent stats;
    stats.set_base(StatType::Strength, 50.0f);
    stats.recalculate();

    SECTION("Get final value") {
        float final_val = stats.get(StatType::Strength);
        REQUIRE_THAT(final_val, WithinAbs(50.0f, 0.001f));
    }

    SECTION("Get integer value") {
        int int_val = stats.get_int(StatType::Strength);
        REQUIRE(int_val == 50);
    }

    SECTION("Has stat") {
        REQUIRE(stats.has(StatType::Strength) == true);
        REQUIRE(stats.has(StatType::Intelligence) == false);
    }
}

TEST_CASE("StatsComponent resource management", "[stats][component]") {
    StatsComponent stats;
    stats.set_base(StatType::MaxHealth, 100.0f);
    stats.set_current(StatType::Health, 100.0f);
    stats.recalculate();

    SECTION("Get current resource") {
        REQUIRE_THAT(stats.get_current(StatType::Health), WithinAbs(100.0f, 0.001f));
    }

    SECTION("Set current resource clamped") {
        stats.set_current(StatType::Health, 150.0f); // Over max
        REQUIRE_THAT(stats.get_current(StatType::Health), WithinAbs(100.0f, 0.001f));

        stats.set_current(StatType::Health, -50.0f); // Under 0
        REQUIRE_THAT(stats.get_current(StatType::Health), WithinAbs(0.0f, 0.001f));
    }

    SECTION("Modify current resource") {
        stats.set_current(StatType::Health, 50.0f);

        float actual = stats.modify_current(StatType::Health, 30.0f);
        REQUIRE_THAT(actual, WithinAbs(30.0f, 0.001f));
        REQUIRE_THAT(stats.get_current(StatType::Health), WithinAbs(80.0f, 0.001f));
    }

    SECTION("Modify current resource clamped at max") {
        stats.set_current(StatType::Health, 80.0f);
        float actual = stats.modify_current(StatType::Health, 50.0f); // Would go to 130
        REQUIRE_THAT(actual, WithinAbs(20.0f, 0.001f)); // Only 20 applied
        REQUIRE_THAT(stats.get_current(StatType::Health), WithinAbs(100.0f, 0.001f));
    }

    SECTION("Modify current resource clamped at zero") {
        stats.set_current(StatType::Health, 30.0f);
        float actual = stats.modify_current(StatType::Health, -50.0f); // Would go to -20
        REQUIRE_THAT(actual, WithinAbs(-30.0f, 0.001f)); // Only -30 applied
        REQUIRE_THAT(stats.get_current(StatType::Health), WithinAbs(0.0f, 0.001f));
    }

    SECTION("Get resource percentage") {
        stats.set_current(StatType::Health, 75.0f);
        REQUIRE_THAT(stats.get_percent(StatType::Health), WithinAbs(0.75f, 0.001f));
    }

    SECTION("Set resource percentage") {
        stats.set_percent(StatType::Health, 0.5f);
        REQUIRE_THAT(stats.get_current(StatType::Health), WithinAbs(50.0f, 0.001f));
    }

    SECTION("Is depleted") {
        stats.set_current(StatType::Health, 0.0f);
        REQUIRE(stats.is_depleted(StatType::Health) == true);

        stats.set_current(StatType::Health, 1.0f);
        REQUIRE(stats.is_depleted(StatType::Health) == false);
    }

    SECTION("Is full") {
        stats.set_current(StatType::Health, 100.0f);
        REQUIRE(stats.is_full(StatType::Health) == true);

        stats.set_current(StatType::Health, 99.0f);
        REQUIRE(stats.is_full(StatType::Health) == false);
    }

    SECTION("Fill resource") {
        stats.set_current(StatType::Health, 50.0f);
        stats.fill(StatType::Health);
        REQUIRE_THAT(stats.get_current(StatType::Health), WithinAbs(100.0f, 0.001f));
    }

    SECTION("Deplete resource") {
        stats.set_current(StatType::Health, 50.0f);
        stats.deplete(StatType::Health);
        REQUIRE_THAT(stats.get_current(StatType::Health), WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("StatsComponent modifier management", "[stats][component]") {
    StatsComponent stats;
    stats.set_base(StatType::Strength, 100.0f);

    SECTION("Add modifier") {
        auto mod = StatModifier::flat(StatType::Strength, 20.0f, "test");
        stats.add_modifier(mod);

        const auto& mods = stats.get_modifiers(StatType::Strength);
        REQUIRE(mods.size() == 1);
    }

    SECTION("Remove modifier by ID") {
        auto mod = StatModifier::flat(StatType::Strength, 20.0f, "test");
        stats.add_modifier(mod);

        bool removed = stats.remove_modifier(mod.id);
        REQUIRE(removed == true);

        const auto& mods = stats.get_modifiers(StatType::Strength);
        REQUIRE(mods.empty());
    }

    SECTION("Remove modifiers by source") {
        stats.add_modifier(StatModifier::flat(StatType::Strength, 10.0f, "equipment:sword"));
        stats.add_modifier(StatModifier::flat(StatType::Strength, 5.0f, "equipment:sword"));
        stats.add_modifier(StatModifier::flat(StatType::Dexterity, 5.0f, "equipment:bow"));

        int removed = stats.remove_modifiers_by_source("equipment:sword");
        REQUIRE(removed == 2);
    }

    SECTION("Clear modifiers for stat") {
        stats.add_modifier(StatModifier::flat(StatType::Strength, 10.0f));
        stats.add_modifier(StatModifier::flat(StatType::Strength, 20.0f));
        stats.clear_modifiers(StatType::Strength);

        const auto& mods = stats.get_modifiers(StatType::Strength);
        REQUIRE(mods.empty());
    }

    SECTION("Clear all modifiers") {
        stats.add_modifier(StatModifier::flat(StatType::Strength, 10.0f));
        stats.add_modifier(StatModifier::flat(StatType::Dexterity, 10.0f));
        stats.clear_all_modifiers();

        REQUIRE(stats.get_modifiers(StatType::Strength).empty());
        REQUIRE(stats.get_modifiers(StatType::Dexterity).empty());
    }

    SECTION("Has modifier from source") {
        stats.add_modifier(StatModifier::flat(StatType::Strength, 10.0f, "buff:might"));

        REQUIRE(stats.has_modifier_from("buff:might") == true);
        REQUIRE(stats.has_modifier_from("buff:other") == false);
    }
}

TEST_CASE("StatsComponent recalculation", "[stats][component]") {
    StatsComponent stats;
    stats.set_base(StatType::Strength, 100.0f);

    SECTION("Recalculate applies modifiers") {
        stats.add_modifier(StatModifier::flat(StatType::Strength, 50.0f));
        stats.recalculate();

        REQUIRE_THAT(stats.get(StatType::Strength), WithinAbs(150.0f, 0.001f));
    }

    SECTION("Recalculate single stat") {
        stats.add_modifier(StatModifier::flat(StatType::Strength, 25.0f));
        stats.recalculate_stat(StatType::Strength);

        REQUIRE_THAT(stats.get(StatType::Strength), WithinAbs(125.0f, 0.001f));
    }

    SECTION("Mark dirty") {
        stats.recalculate();
        stats.needs_recalculation = false;

        stats.mark_dirty();
        REQUIRE(stats.needs_recalculation == true);
    }
}

TEST_CASE("StatsComponent initialization", "[stats][component]") {
    StatsComponent stats;

    SECTION("Initialize defaults from registry") {
        stats.initialize_defaults();
        // Should populate with default values from stat registry
    }

    SECTION("Copy base from another component") {
        StatsComponent other;
        other.set_base(StatType::Strength, 50.0f);
        other.set_base(StatType::Dexterity, 30.0f);

        stats.copy_base_from(other);

        REQUIRE_THAT(stats.get_base(StatType::Strength), WithinAbs(50.0f, 0.001f));
        REQUIRE_THAT(stats.get_base(StatType::Dexterity), WithinAbs(30.0f, 0.001f));
    }
}

TEST_CASE("StatPreset structure", "[stats][preset]") {
    StatPreset preset;
    preset.preset_id = "warrior";
    preset.display_name = "Warrior";
    preset.base_values[StatType::Strength] = 20.0f;
    preset.base_values[StatType::Vitality] = 15.0f;
    preset.base_values[StatType::MaxHealth] = 150.0f;

    REQUIRE(preset.preset_id == "warrior");
    REQUIRE(preset.display_name == "Warrior");
    REQUIRE(preset.base_values.size() == 3);
    REQUIRE_THAT(preset.base_values[StatType::Strength], WithinAbs(20.0f, 0.001f));
}

TEST_CASE("StatPresetRegistry", "[stats][preset]") {
    StatPresetRegistry& reg = stat_presets();

    SECTION("Register preset") {
        StatPreset preset;
        preset.preset_id = "test_preset";
        preset.display_name = "Test Preset";
        reg.register_preset(preset);

        const auto* found = reg.get_preset("test_preset");
        REQUIRE(found != nullptr);
        REQUIRE(found->preset_id == "test_preset");
    }

    SECTION("Get non-existent preset returns nullptr") {
        const auto* found = reg.get_preset("nonexistent");
        REQUIRE(found == nullptr);
    }

    SECTION("Get all preset IDs") {
        StatPreset preset;
        preset.preset_id = "another_preset";
        reg.register_preset(preset);

        auto ids = reg.get_all_preset_ids();
        REQUIRE(ids.size() > 0);
    }
}
