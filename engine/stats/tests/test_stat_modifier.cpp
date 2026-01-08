#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/stats/stat_modifier.hpp>

using namespace engine::stats;
using Catch::Matchers::WithinAbs;

TEST_CASE("ModifierType enum", "[stats][modifier]") {
    REQUIRE(static_cast<uint8_t>(ModifierType::Flat) == 0);
    REQUIRE(static_cast<uint8_t>(ModifierType::PercentAdd) == 1);
    REQUIRE(static_cast<uint8_t>(ModifierType::PercentMult) == 2);
    REQUIRE(static_cast<uint8_t>(ModifierType::Override) == 3);
}

TEST_CASE("ModifierSource enum", "[stats][modifier]") {
    REQUIRE(static_cast<uint8_t>(ModifierSource::Base) == 0);
    REQUIRE(static_cast<uint8_t>(ModifierSource::Equipment) == 1);
    REQUIRE(static_cast<uint8_t>(ModifierSource::Effect) == 2);
    REQUIRE(static_cast<uint8_t>(ModifierSource::Skill) == 3);
    REQUIRE(static_cast<uint8_t>(ModifierSource::Aura) == 4);
}

TEST_CASE("StatModifier default values", "[stats][modifier]") {
    StatModifier mod;

    REQUIRE(mod.stat == StatType::Health);
    REQUIRE(mod.type == ModifierType::Flat);
    REQUIRE(mod.source == ModifierSource::Temporary);
    REQUIRE_THAT(mod.value, WithinAbs(0.0f, 0.001f));
    REQUIRE(mod.priority == 0);
    REQUIRE(mod.source_id.empty());
    REQUIRE(mod.source_name.empty());
    REQUIRE_THAT(mod.duration, WithinAbs(-1.0f, 0.001f)); // Permanent by default
    REQUIRE_THAT(mod.elapsed, WithinAbs(0.0f, 0.001f));
    REQUIRE(mod.is_hidden == false);
    REQUIRE(mod.is_stackable == true);
    REQUIRE(mod.condition == nullptr);
}

TEST_CASE("StatModifier static helpers", "[stats][modifier]") {
    SECTION("Flat modifier") {
        auto mod = StatModifier::flat(StatType::Strength, 10.0f, "item:sword");
        REQUIRE(mod.stat == StatType::Strength);
        REQUIRE(mod.type == ModifierType::Flat);
        REQUIRE_THAT(mod.value, WithinAbs(10.0f, 0.001f));
        REQUIRE(mod.source_id == "item:sword");
    }

    SECTION("Percent add modifier") {
        auto mod = StatModifier::percent_add(StatType::MoveSpeed, 0.15f, "buff:haste");
        REQUIRE(mod.stat == StatType::MoveSpeed);
        REQUIRE(mod.type == ModifierType::PercentAdd);
        REQUIRE_THAT(mod.value, WithinAbs(0.15f, 0.001f));
    }

    SECTION("Percent mult modifier") {
        auto mod = StatModifier::percent_mult(StatType::PhysicalDamage, 0.50f, "ability:rage");
        REQUIRE(mod.stat == StatType::PhysicalDamage);
        REQUIRE(mod.type == ModifierType::PercentMult);
        REQUIRE_THAT(mod.value, WithinAbs(0.50f, 0.001f));
    }

    SECTION("Override modifier") {
        auto mod = StatModifier::override_val(StatType::MoveSpeed, 0.0f, "debuff:root");
        REQUIRE(mod.stat == StatType::MoveSpeed);
        REQUIRE(mod.type == ModifierType::Override);
        REQUIRE_THAT(mod.value, WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("StatModifier duration handling", "[stats][modifier]") {
    StatModifier mod;

    SECTION("Permanent modifier") {
        mod.duration = -1.0f;
        REQUIRE(mod.is_permanent() == true);
        REQUIRE(mod.is_expired() == false);
        REQUIRE_THAT(mod.get_remaining(), WithinAbs(-1.0f, 0.001f));
    }

    SECTION("Timed modifier not expired") {
        mod.duration = 10.0f;
        mod.elapsed = 5.0f;
        REQUIRE(mod.is_permanent() == false);
        REQUIRE(mod.is_expired() == false);
        REQUIRE_THAT(mod.get_remaining(), WithinAbs(5.0f, 0.001f));
    }

    SECTION("Timed modifier expired") {
        mod.duration = 10.0f;
        mod.elapsed = 15.0f;
        REQUIRE(mod.is_expired() == true);
        REQUIRE_THAT(mod.get_remaining(), WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("StatModifier update", "[stats][modifier]") {
    StatModifier mod;

    SECTION("Update permanent modifier") {
        mod.duration = -1.0f;
        bool active = mod.update(1.0f);
        REQUIRE(active == true);
    }

    SECTION("Update timed modifier still active") {
        mod.duration = 10.0f;
        mod.elapsed = 0.0f;
        bool active = mod.update(5.0f);
        REQUIRE(active == true);
        REQUIRE_THAT(mod.elapsed, WithinAbs(5.0f, 0.001f));
    }

    SECTION("Update timed modifier expires") {
        mod.duration = 10.0f;
        mod.elapsed = 9.0f;
        bool active = mod.update(5.0f);
        REQUIRE(active == false);
        REQUIRE_THAT(mod.elapsed, WithinAbs(14.0f, 0.001f));
    }
}

TEST_CASE("StatModifier condition", "[stats][modifier]") {
    StatModifier mod;

    SECTION("No condition is always active") {
        mod.condition = nullptr;
        REQUIRE(mod.is_active() == true);
    }

    SECTION("Condition returns true") {
        mod.condition = []() { return true; };
        REQUIRE(mod.is_active() == true);
    }

    SECTION("Condition returns false") {
        mod.condition = []() { return false; };
        REQUIRE(mod.is_active() == false);
    }
}

TEST_CASE("ModifierStack operations", "[stats][modifier]") {
    ModifierStack stack;

    SECTION("Initially empty") {
        REQUIRE(stack.empty() == true);
        REQUIRE(stack.total_count() == 0);
    }

    SECTION("Add flat modifier") {
        auto mod = StatModifier::flat(StatType::Strength, 10.0f);
        stack.add(mod);

        REQUIRE(stack.flat.size() == 1);
        REQUIRE(stack.total_count() == 1);
        REQUIRE(stack.empty() == false);
    }

    SECTION("Add percent_add modifier") {
        auto mod = StatModifier::percent_add(StatType::Strength, 0.10f);
        stack.add(mod);

        REQUIRE(stack.percent_add.size() == 1);
    }

    SECTION("Add percent_mult modifier") {
        auto mod = StatModifier::percent_mult(StatType::Strength, 0.50f);
        stack.add(mod);

        REQUIRE(stack.percent_mult.size() == 1);
    }

    SECTION("Clear stack") {
        stack.add(StatModifier::flat(StatType::Strength, 10.0f));
        stack.add(StatModifier::percent_add(StatType::Strength, 0.10f));
        stack.clear();

        REQUIRE(stack.empty() == true);
        REQUIRE(stack.total_count() == 0);
    }
}

TEST_CASE("calculate_stat_value formula", "[stats][calculation]") {
    ModifierStack stack;
    float base = 100.0f;

    SECTION("No modifiers returns base") {
        float result = calculate_stat_value(base, stack);
        REQUIRE_THAT(result, WithinAbs(100.0f, 0.001f));
    }

    SECTION("Flat modifier only") {
        stack.add(StatModifier::flat(StatType::Strength, 20.0f));
        float result = calculate_stat_value(base, stack);
        REQUIRE_THAT(result, WithinAbs(120.0f, 0.001f)); // 100 + 20
    }

    SECTION("Multiple flat modifiers") {
        stack.add(StatModifier::flat(StatType::Strength, 20.0f));
        stack.add(StatModifier::flat(StatType::Strength, 10.0f));
        float result = calculate_stat_value(base, stack);
        REQUIRE_THAT(result, WithinAbs(130.0f, 0.001f)); // 100 + 20 + 10
    }

    SECTION("Percent add modifier only") {
        stack.add(StatModifier::percent_add(StatType::Strength, 0.50f)); // +50%
        float result = calculate_stat_value(base, stack);
        REQUIRE_THAT(result, WithinAbs(150.0f, 0.001f)); // 100 * 1.50
    }

    SECTION("Multiple percent add modifiers (additive)") {
        stack.add(StatModifier::percent_add(StatType::Strength, 0.20f)); // +20%
        stack.add(StatModifier::percent_add(StatType::Strength, 0.30f)); // +30%
        float result = calculate_stat_value(base, stack);
        REQUIRE_THAT(result, WithinAbs(150.0f, 0.001f)); // 100 * (1 + 0.20 + 0.30) = 100 * 1.50
    }

    SECTION("Percent mult modifier") {
        stack.add(StatModifier::percent_mult(StatType::Strength, 0.50f)); // *1.50
        float result = calculate_stat_value(base, stack);
        REQUIRE_THAT(result, WithinAbs(150.0f, 0.001f)); // 100 * 1.50
    }

    SECTION("Multiple percent mult modifiers (multiplicative)") {
        stack.add(StatModifier::percent_mult(StatType::Strength, 0.50f)); // *1.50
        stack.add(StatModifier::percent_mult(StatType::Strength, 0.20f)); // *1.20
        float result = calculate_stat_value(base, stack);
        REQUIRE_THAT(result, WithinAbs(180.0f, 0.001f)); // 100 * 1.50 * 1.20 = 180
    }

    SECTION("Combined modifiers") {
        // Formula: (base + flat) * (1 + sum_percent_add) * product(1 + percent_mult)
        stack.add(StatModifier::flat(StatType::Strength, 20.0f));          // +20
        stack.add(StatModifier::percent_add(StatType::Strength, 0.50f));   // +50%
        stack.add(StatModifier::percent_mult(StatType::Strength, 0.20f));  // *1.20

        // (100 + 20) * (1 + 0.50) * (1 + 0.20)
        // = 120 * 1.50 * 1.20
        // = 216
        float result = calculate_stat_value(base, stack);
        REQUIRE_THAT(result, WithinAbs(216.0f, 0.1f));
    }
}

TEST_CASE("ModifierBuilder fluent API", "[stats][builder]") {
    SECTION("Build flat modifier") {
        auto mod = modifier()
            .stat(StatType::PhysicalDamage)
            .flat(25.0f)
            .source(ModifierSource::Equipment, "sword:iron")
            .build();

        REQUIRE(mod.stat == StatType::PhysicalDamage);
        REQUIRE(mod.type == ModifierType::Flat);
        REQUIRE_THAT(mod.value, WithinAbs(25.0f, 0.001f));
        REQUIRE(mod.source == ModifierSource::Equipment);
        REQUIRE(mod.source_id == "sword:iron");
    }

    SECTION("Build percent modifier") {
        auto mod = modifier()
            .stat(StatType::CritChance)
            .percent_add(0.10f)
            .source(ModifierSource::Skill, "skill:precision")
            .permanent()
            .build();

        REQUIRE(mod.stat == StatType::CritChance);
        REQUIRE(mod.type == ModifierType::PercentAdd);
        REQUIRE_THAT(mod.value, WithinAbs(0.10f, 0.001f));
        REQUIRE(mod.is_permanent() == true);
    }

    SECTION("Build timed modifier") {
        auto mod = modifier()
            .stat(StatType::MoveSpeed)
            .percent_mult(0.30f)
            .source(ModifierSource::Effect, "buff:sprint")
            .duration(10.0f)
            .build();

        REQUIRE_THAT(mod.duration, WithinAbs(10.0f, 0.001f));
        REQUIRE(mod.is_permanent() == false);
    }

    SECTION("Build hidden modifier") {
        auto mod = modifier()
            .stat(StatType::Strength)
            .flat(5.0f)
            .hidden()
            .build();

        REQUIRE(mod.is_hidden == true);
    }

    SECTION("Build with priority") {
        auto mod = modifier()
            .stat(StatType::Health)
            .flat(100.0f)
            .priority(10)
            .build();

        REQUIRE(mod.priority == 10);
    }

    SECTION("Build with condition") {
        bool flag = true;
        auto mod = modifier()
            .stat(StatType::PhysicalDamage)
            .percent_mult(0.50f)
            .condition([&flag]() { return flag; })
            .build();

        REQUIRE(mod.is_active() == true);
        flag = false;
        REQUIRE(mod.is_active() == false);
    }
}

TEST_CASE("Convenience modifier functions", "[stats][modifier]") {
    SECTION("make_flat_modifier") {
        auto mod = make_flat_modifier(StatType::Strength, 15.0f, "test");
        REQUIRE(mod.type == ModifierType::Flat);
        REQUIRE_THAT(mod.value, WithinAbs(15.0f, 0.001f));
    }

    SECTION("make_percent_modifier") {
        auto mod = make_percent_modifier(StatType::MoveSpeed, 0.25f, "test");
        REQUIRE(mod.type == ModifierType::PercentAdd);
        REQUIRE_THAT(mod.value, WithinAbs(0.25f, 0.001f));
    }

    SECTION("make_timed_modifier") {
        auto mod = make_timed_modifier(StatType::Health, 50.0f, 30.0f, "test");
        REQUIRE_THAT(mod.value, WithinAbs(50.0f, 0.001f));
        REQUIRE_THAT(mod.duration, WithinAbs(30.0f, 0.001f));
    }
}
