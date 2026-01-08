#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/stats/stat_calculator.hpp>
#include <engine/scene/world.hpp>

using namespace engine::stats;
using namespace engine::scene;
using Catch::Matchers::WithinAbs;

TEST_CASE("StatCalculator basic calculation", "[stats][calculator]") {
    std::vector<StatModifier> modifiers;

    SECTION("No modifiers returns base") {
        float result = StatCalculator::calculate(100.0f, modifiers);
        REQUIRE_THAT(result, WithinAbs(100.0f, 0.001f));
    }

    SECTION("Single flat modifier") {
        modifiers.push_back(StatModifier::flat(StatType::Strength, 25.0f));
        float result = StatCalculator::calculate(100.0f, modifiers);
        REQUIRE_THAT(result, WithinAbs(125.0f, 0.001f));
    }

    SECTION("Single percent add modifier") {
        modifiers.push_back(StatModifier::percent_add(StatType::Strength, 50.0f));
        float result = StatCalculator::calculate(100.0f, modifiers);
        REQUIRE_THAT(result, WithinAbs(150.0f, 0.001f));
    }
}

TEST_CASE("StatCalculator with ModifierStack", "[stats][calculator]") {
    ModifierStack stack;
    float base = 100.0f;

    SECTION("Complex calculation") {
        stack.add(StatModifier::flat(StatType::Strength, 20.0f));
        stack.add(StatModifier::percent_add(StatType::Strength, 25.0f));
        stack.add(StatModifier::percent_mult(StatType::Strength, 10.0f));

        float result = StatCalculator::calculate(base, stack);
        // (100 + 20) * (1 + 0.25) * (1 + 0.10)
        // = 120 * 1.25 * 1.10
        // = 165
        REQUIRE_THAT(result, WithinAbs(165.0f, 0.1f));
    }
}

TEST_CASE("StatCalculator modifier breakdown", "[stats][calculator]") {
    StatsComponent stats;
    stats.set_base(StatType::Strength, 100.0f);
    stats.add_modifier(StatModifier::flat(StatType::Strength, 20.0f, "equipment"));
    stats.add_modifier(StatModifier::percent_add(StatType::Strength, 50.0f, "buff"));
    stats.recalculate();

    StatCalculator::ModifierBreakdown breakdown = StatCalculator::get_breakdown(stats, StatType::Strength);

    REQUIRE_THAT(breakdown.base_value, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(breakdown.flat_total, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(breakdown.percent_add_total, WithinAbs(50.0f, 0.001f));
    REQUIRE(breakdown.has_override == false);
}

TEST_CASE("StatCalculator modifier contribution", "[stats][calculator]") {
    StatsComponent stats;
    stats.set_base(StatType::Strength, 100.0f);
    stats.add_modifier(StatModifier::flat(StatType::Strength, 50.0f));
    stats.recalculate();

    float contribution = StatCalculator::get_modifier_contribution(stats, StatType::Strength);
    REQUIRE_THAT(contribution, WithinAbs(50.0f, 0.001f)); // Final 150 - Base 100
}

TEST_CASE("StatCalculator percent change", "[stats][calculator]") {
    StatsComponent stats;
    stats.set_base(StatType::Strength, 100.0f);
    stats.add_modifier(StatModifier::flat(StatType::Strength, 50.0f));
    stats.recalculate();

    float percent_change = StatCalculator::get_modifier_percent_change(stats, StatType::Strength);
    REQUIRE_THAT(percent_change, WithinAbs(50.0f, 0.01f)); // 50% increase
}

TEST_CASE("StatQuery with World", "[stats][query]") {
    World world;
    Entity player = world.create("Player");

    auto& stats = world.emplace<StatsComponent>(player);
    stats.set_base(StatType::MaxHealth, 100.0f);
    stats.set_base(StatType::Strength, 20.0f);
    stats.set_current(StatType::Health, 75.0f);
    stats.recalculate();

    SECTION("Get stat from entity") {
        float strength = StatQuery::get(world, player, StatType::Strength);
        REQUIRE_THAT(strength, WithinAbs(20.0f, 0.001f));
    }

    SECTION("Get current resource") {
        float health = StatQuery::get_current(world, player, StatType::Health);
        REQUIRE_THAT(health, WithinAbs(75.0f, 0.001f));
    }

    SECTION("Get resource percent") {
        float percent = StatQuery::get_percent(world, player, StatType::Health);
        REQUIRE_THAT(percent, WithinAbs(0.75f, 0.01f));
    }

    SECTION("Has stat") {
        REQUIRE(StatQuery::has(world, player, StatType::Strength) == true);
        REQUIRE(StatQuery::has(world, player, StatType::Intelligence) == false);
    }

    SECTION("Get stat from entity without StatsComponent returns 0") {
        Entity empty = world.create("Empty");
        float result = StatQuery::get(world, empty, StatType::Strength);
        REQUIRE_THAT(result, WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("StatQuery comparison", "[stats][query]") {
    World world;

    Entity player = world.create("Player");
    auto& player_stats = world.emplace<StatsComponent>(player);
    player_stats.set_base(StatType::Strength, 50.0f);
    player_stats.recalculate();

    Entity enemy = world.create("Enemy");
    auto& enemy_stats = world.emplace<StatsComponent>(enemy);
    enemy_stats.set_base(StatType::Strength, 30.0f);
    enemy_stats.recalculate();

    SECTION("Compare stats") {
        float diff = StatQuery::compare(world, player, enemy, StatType::Strength);
        REQUIRE_THAT(diff, WithinAbs(20.0f, 0.001f)); // player - enemy = 50 - 30
    }
}

TEST_CASE("StatQuery find highest/lowest", "[stats][query]") {
    World world;

    Entity e1 = world.create("E1");
    auto& s1 = world.emplace<StatsComponent>(e1);
    s1.set_base(StatType::Strength, 30.0f);
    s1.recalculate();

    Entity e2 = world.create("E2");
    auto& s2 = world.emplace<StatsComponent>(e2);
    s2.set_base(StatType::Strength, 50.0f);
    s2.recalculate();

    Entity e3 = world.create("E3");
    auto& s3 = world.emplace<StatsComponent>(e3);
    s3.set_base(StatType::Strength, 10.0f);
    s3.recalculate();

    SECTION("Find highest stat") {
        Entity highest = StatQuery::find_highest(world, StatType::Strength);
        REQUIRE(highest == e2);
    }

    SECTION("Find lowest stat") {
        Entity lowest = StatQuery::find_lowest(world, StatType::Strength);
        REQUIRE(lowest == e3);
    }
}

TEST_CASE("StatQuery filter by stat", "[stats][query]") {
    World world;

    Entity e1 = world.create();
    auto& s1 = world.emplace<StatsComponent>(e1);
    s1.set_base(StatType::Strength, 10.0f);
    s1.recalculate();

    Entity e2 = world.create();
    auto& s2 = world.emplace<StatsComponent>(e2);
    s2.set_base(StatType::Strength, 50.0f);
    s2.recalculate();

    Entity e3 = world.create();
    auto& s3 = world.emplace<StatsComponent>(e3);
    s3.set_base(StatType::Strength, 30.0f);
    s3.recalculate();

    auto results = StatQuery::filter_by_stat(world, StatType::Strength, [](float v) {
        return v >= 30.0f;
    });

    REQUIRE(results.size() == 2);
}

TEST_CASE("StatOperations", "[stats][operations]") {
    World world;
    Entity player = world.create("Player");

    auto& stats = world.emplace<StatsComponent>(player);
    stats.set_base(StatType::MaxHealth, 100.0f);
    stats.set_base(StatType::Strength, 20.0f);
    stats.set_current(StatType::Health, 100.0f);
    stats.recalculate();

    SECTION("Set base stat") {
        StatOperations::set_base(world, player, StatType::Strength, 50.0f);
        stats.recalculate();
        REQUIRE_THAT(stats.get(StatType::Strength), WithinAbs(50.0f, 0.001f));
    }

    SECTION("Add to base stat") {
        StatOperations::add_base(world, player, StatType::Strength, 10.0f);
        stats.recalculate();
        REQUIRE_THAT(stats.get(StatType::Strength), WithinAbs(30.0f, 0.001f));
    }

    SECTION("Add modifier") {
        auto mod = StatModifier::flat(StatType::Strength, 15.0f, "test");
        StatOperations::add_modifier(world, player, mod);
        stats.recalculate();
        REQUIRE_THAT(stats.get(StatType::Strength), WithinAbs(35.0f, 0.001f));
    }

    SECTION("Damage entity") {
        float actual = StatOperations::damage(world, player, 30.0f);
        REQUIRE_THAT(actual, WithinAbs(-30.0f, 0.001f));
        REQUIRE_THAT(stats.get_current(StatType::Health), WithinAbs(70.0f, 0.001f));
    }

    SECTION("Heal entity") {
        stats.set_current(StatType::Health, 50.0f);
        float actual = StatOperations::heal(world, player, 30.0f);
        REQUIRE_THAT(actual, WithinAbs(30.0f, 0.001f));
        REQUIRE_THAT(stats.get_current(StatType::Health), WithinAbs(80.0f, 0.001f));
    }

    SECTION("Can afford resource cost") {
        stats.set_current(StatType::Health, 50.0f);
        REQUIRE(StatOperations::can_afford(world, player, StatType::Health, 30.0f) == true);
        REQUIRE(StatOperations::can_afford(world, player, StatType::Health, 60.0f) == false);
    }
}

TEST_CASE("StatScaling", "[stats][scaling]") {
    SECTION("Linear scaling") {
        float result = StatScaling::scale(100.0f, 10, 5.0f, StatScaling::ScaleType::Linear);
        // 100 + (10 * 5) = 150
        REQUIRE_THAT(result, WithinAbs(150.0f, 0.001f));
    }

    SECTION("Damage reduction calculation") {
        float reduction = StatScaling::calculate_damage_reduction(100.0f);
        // Should return 0.0 to 1.0
        REQUIRE(reduction >= 0.0f);
        REQUIRE(reduction <= 1.0f);
    }
}

TEST_CASE("StatScaling critical hit", "[stats][scaling]") {
    StatsComponent stats;
    stats.set_base(StatType::CritChance, 50.0f); // 50% crit chance
    stats.set_base(StatType::CritDamage, 150.0f); // 150% crit damage
    stats.recalculate();

    SECTION("Apply crit multiplier") {
        float base_damage = 100.0f;
        float crit_damage = StatScaling::apply_crit(base_damage, stats);
        REQUIRE_THAT(crit_damage, WithinAbs(150.0f, 0.1f));
    }
}

TEST_CASE("Regeneration calculation", "[stats][regen]") {
    StatsComponent stats;
    stats.set_base(StatType::HealthRegen, 10.0f); // 10 HP per second
    stats.recalculate();

    RegenRate rate;
    rate.resource = StatType::Health;
    rate.regen_stat = StatType::HealthRegen;
    rate.base_rate = 0.0f;

    SECTION("Calculate regen amount") {
        float dt = 0.5f; // 500ms
        float regen = calculate_regen(stats, rate, dt);
        REQUIRE_THAT(regen, WithinAbs(5.0f, 0.1f)); // 10 * 0.5 = 5
    }
}
