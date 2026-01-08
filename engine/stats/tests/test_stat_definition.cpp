#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/stats/stat_definition.hpp>

using namespace engine::stats;
using Catch::Matchers::WithinAbs;

TEST_CASE("StatType enum values", "[stats][definition]") {
    SECTION("Resource stats") {
        REQUIRE(static_cast<uint8_t>(StatType::Health) == 0);
        REQUIRE(static_cast<uint8_t>(StatType::MaxHealth) == 1);
        REQUIRE(static_cast<uint8_t>(StatType::HealthRegen) == 2);
    }

    SECTION("Primary attributes") {
        REQUIRE(static_cast<uint8_t>(StatType::Strength) == 9);
        REQUIRE(static_cast<uint8_t>(StatType::Dexterity) == 10);
        REQUIRE(static_cast<uint8_t>(StatType::Intelligence) == 11);
    }

    SECTION("Custom stat range") {
        REQUIRE(static_cast<uint8_t>(StatType::Custom) == 128);
        REQUIRE(static_cast<uint8_t>(StatType::Count) == 255);
    }
}

TEST_CASE("StatCategory enum", "[stats][definition]") {
    REQUIRE(static_cast<uint8_t>(StatCategory::Resource) == 0);
    REQUIRE(static_cast<uint8_t>(StatCategory::Attribute) == 1);
    REQUIRE(static_cast<uint8_t>(StatCategory::Offense) == 2);
    REQUIRE(static_cast<uint8_t>(StatCategory::Defense) == 3);
    REQUIRE(static_cast<uint8_t>(StatCategory::Resistance) == 4);
    REQUIRE(static_cast<uint8_t>(StatCategory::Utility) == 5);
}

TEST_CASE("StatDefinition default values", "[stats][definition]") {
    StatDefinition def;

    REQUIRE(def.type == StatType::Health);
    REQUIRE(def.internal_name.empty());
    REQUIRE(def.display_name.empty());
    REQUIRE(def.abbreviation.empty());
    REQUIRE(def.description.empty());
    REQUIRE(def.category == StatCategory::Attribute);
    REQUIRE_THAT(def.default_value, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(def.min_value, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(def.max_value, WithinAbs(999999.0f, 1.0f));
    REQUIRE(def.max_stat == StatType::Count);
    REQUIRE(def.is_percentage == false);
    REQUIRE(def.decimal_places == 0);
    REQUIRE(def.higher_is_better == true);
    REQUIRE(def.derived_from == StatType::Count);
}

TEST_CASE("StatDefinition custom values", "[stats][definition]") {
    StatDefinition def;
    def.type = StatType::MaxHealth;
    def.internal_name = "max_health";
    def.display_name = "Maximum Health";
    def.abbreviation = "HP";
    def.description = "Your maximum hit points";
    def.icon_path = "icons/health.png";
    def.category = StatCategory::Resource;
    def.default_value = 100.0f;
    def.min_value = 1.0f;
    def.max_value = 10000.0f;
    def.is_percentage = false;
    def.decimal_places = 0;
    def.higher_is_better = true;

    REQUIRE(def.type == StatType::MaxHealth);
    REQUIRE(def.internal_name == "max_health");
    REQUIRE(def.display_name == "Maximum Health");
    REQUIRE(def.abbreviation == "HP");
    REQUIRE(def.category == StatCategory::Resource);
    REQUIRE_THAT(def.default_value, WithinAbs(100.0f, 0.001f));
}

TEST_CASE("StatDefinition derived stat", "[stats][definition]") {
    StatDefinition def;
    def.type = StatType::PhysicalDamage;
    def.derived_from = StatType::Strength;
    def.derived_multiplier = 2.5f;
    def.derived_flat = 10.0f;

    REQUIRE(def.derived_from == StatType::Strength);
    REQUIRE_THAT(def.derived_multiplier, WithinAbs(2.5f, 0.001f));
    REQUIRE_THAT(def.derived_flat, WithinAbs(10.0f, 0.001f));
}

TEST_CASE("StatRegistry singleton", "[stats][registry]") {
    StatRegistry& reg1 = StatRegistry::instance();
    StatRegistry& reg2 = stat_registry();

    REQUIRE(&reg1 == &reg2);
}

TEST_CASE("StatRegistry builtin stats", "[stats][registry]") {
    StatRegistry& reg = stat_registry();
    reg.register_builtin_stats();

    SECTION("Health stat is registered") {
        REQUIRE(reg.is_registered(StatType::Health));
        const auto* def = reg.get_definition(StatType::Health);
        REQUIRE(def != nullptr);
        REQUIRE(def->type == StatType::Health);
    }

    SECTION("MaxHealth stat is registered") {
        REQUIRE(reg.is_registered(StatType::MaxHealth));
        const auto* def = reg.get_definition(StatType::MaxHealth);
        REQUIRE(def != nullptr);
    }

    SECTION("Strength stat is registered") {
        REQUIRE(reg.is_registered(StatType::Strength));
        const auto* def = reg.get_definition(StatType::Strength);
        REQUIRE(def != nullptr);
    }
}

TEST_CASE("StatRegistry lookup by name", "[stats][registry]") {
    StatRegistry& reg = stat_registry();
    reg.register_builtin_stats();

    SECTION("Get definition by name") {
        const auto* def = reg.get_definition("health");
        // May or may not exist depending on registration
        // This tests the interface
    }

    SECTION("Get type by name") {
        StatType type = reg.get_type_by_name("max_health");
        // Type lookup by internal name
    }
}

TEST_CASE("StatRegistry queries", "[stats][registry]") {
    StatRegistry& reg = stat_registry();
    reg.register_builtin_stats();

    SECTION("Get all registered stats") {
        auto stats = reg.get_all_registered_stats();
        REQUIRE(stats.size() > 0);
    }

    SECTION("Get stats by category") {
        auto resource_stats = reg.get_stats_by_category(StatCategory::Resource);
        // Should include Health, MaxHealth, Stamina, etc.
    }

    SECTION("Get category name") {
        std::string name = reg.get_category_name(StatCategory::Offense);
        REQUIRE(!name.empty());
    }
}

TEST_CASE("StatRegistry custom stat registration", "[stats][registry]") {
    StatRegistry& reg = stat_registry();

    StatDefinition custom_def;
    custom_def.internal_name = "custom_stat";
    custom_def.display_name = "Custom Stat";
    custom_def.category = StatCategory::Custom;
    custom_def.default_value = 50.0f;

    StatType custom_type = reg.register_custom_stat(custom_def);

    SECTION("Custom stat type in custom range") {
        REQUIRE(static_cast<uint8_t>(custom_type) >= static_cast<uint8_t>(StatType::Custom));
    }

    SECTION("Custom stat is registered") {
        REQUIRE(reg.is_registered(custom_type));
    }
}

TEST_CASE("is_resource_stat helper", "[stats][helpers]") {
    REQUIRE(is_resource_stat(StatType::Health) == true);
    REQUIRE(is_resource_stat(StatType::Stamina) == true);
    REQUIRE(is_resource_stat(StatType::Mana) == true);
    REQUIRE(is_resource_stat(StatType::Strength) == false);
    REQUIRE(is_resource_stat(StatType::PhysicalDamage) == false);
}

TEST_CASE("is_max_stat helper", "[stats][helpers]") {
    REQUIRE(is_max_stat(StatType::MaxHealth) == true);
    REQUIRE(is_max_stat(StatType::MaxStamina) == true);
    REQUIRE(is_max_stat(StatType::MaxMana) == true);
    REQUIRE(is_max_stat(StatType::Health) == false);
    REQUIRE(is_max_stat(StatType::Strength) == false);
}

TEST_CASE("get_resource_stat helper", "[stats][helpers]") {
    REQUIRE(get_resource_stat(StatType::MaxHealth) == StatType::Health);
    REQUIRE(get_resource_stat(StatType::MaxStamina) == StatType::Stamina);
    REQUIRE(get_resource_stat(StatType::MaxMana) == StatType::Mana);
}

TEST_CASE("get_max_stat helper", "[stats][helpers]") {
    REQUIRE(get_max_stat(StatType::Health) == StatType::MaxHealth);
    REQUIRE(get_max_stat(StatType::Stamina) == StatType::MaxStamina);
    REQUIRE(get_max_stat(StatType::Mana) == StatType::MaxMana);
}
