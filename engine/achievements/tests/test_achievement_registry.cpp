#include <catch2/catch_test_macros.hpp>
#include <engine/achievements/achievement_definition.hpp>

using namespace engine::achievements;

class RegistryFixture {
protected:
    RegistryFixture() {
        // Clear registry before each test
        achievement_registry().clear();
    }

    ~RegistryFixture() {
        achievement_registry().clear();
    }

    void register_test_achievements() {
        achievement()
            .id("test_combat_1")
            .name("Combat 1")
            .category(AchievementCategory::Combat)
            .points(10)
            .register_achievement();

        achievement()
            .id("test_combat_2")
            .name("Combat 2")
            .category(AchievementCategory::Combat)
            .points(20)
            .register_achievement();

        achievement()
            .id("test_exploration_1")
            .name("Exploration 1")
            .category(AchievementCategory::Exploration)
            .points(15)
            .register_achievement();

        achievement()
            .id("test_secret_1")
            .name("???")
            .category(AchievementCategory::Secret)
            .hidden()
            .points(50)
            .register_achievement();
    }
};

TEST_CASE_METHOD(RegistryFixture, "AchievementRegistry singleton", "[achievements][registry]") {
    auto& registry = achievement_registry();
    auto& registry2 = AchievementRegistry::instance();

    REQUIRE(&registry == &registry2);
}

TEST_CASE_METHOD(RegistryFixture, "AchievementRegistry empty state", "[achievements][registry]") {
    auto& registry = achievement_registry();

    REQUIRE(registry.get_total_achievements() == 0);
    REQUIRE(registry.get_total_points() == 0);
    REQUIRE(registry.get_all_achievement_ids().empty());
}

TEST_CASE_METHOD(RegistryFixture, "AchievementRegistry registration", "[achievements][registry]") {
    auto& registry = achievement_registry();

    SECTION("Register single achievement") {
        AchievementDefinition def;
        def.achievement_id = "test_1";
        def.display_name = "Test Achievement";
        def.points = 10;

        registry.register_achievement(def);

        REQUIRE(registry.exists("test_1"));
        REQUIRE(registry.get_total_achievements() == 1);
    }

    SECTION("Register via builder") {
        achievement()
            .id("builder_test")
            .name("Builder Test")
            .points(20)
            .register_achievement();

        REQUIRE(registry.exists("builder_test"));
    }

    SECTION("Register multiple achievements") {
        register_test_achievements();
        REQUIRE(registry.get_total_achievements() == 4);
    }
}

TEST_CASE_METHOD(RegistryFixture, "AchievementRegistry lookup", "[achievements][registry]") {
    auto& registry = achievement_registry();
    register_test_achievements();

    SECTION("Get existing achievement") {
        const auto* def = registry.get("test_combat_1");
        REQUIRE(def != nullptr);
        REQUIRE(def->achievement_id == "test_combat_1");
        REQUIRE(def->display_name == "Combat 1");
    }

    SECTION("Get non-existent achievement") {
        const auto* def = registry.get("nonexistent");
        REQUIRE(def == nullptr);
    }

    SECTION("Exists check") {
        REQUIRE(registry.exists("test_combat_1"));
        REQUIRE(registry.exists("test_exploration_1"));
        REQUIRE_FALSE(registry.exists("nonexistent"));
    }
}

TEST_CASE_METHOD(RegistryFixture, "AchievementRegistry queries", "[achievements][registry]") {
    auto& registry = achievement_registry();
    register_test_achievements();

    SECTION("Get all achievement IDs") {
        auto ids = registry.get_all_achievement_ids();
        REQUIRE(ids.size() == 4);
    }

    SECTION("Get by category") {
        auto combat = registry.get_by_category(AchievementCategory::Combat);
        REQUIRE(combat.size() == 2);

        auto exploration = registry.get_by_category(AchievementCategory::Exploration);
        REQUIRE(exploration.size() == 1);

        auto secret = registry.get_by_category(AchievementCategory::Secret);
        REQUIRE(secret.size() == 1);

        auto story = registry.get_by_category(AchievementCategory::Story);
        REQUIRE(story.empty());
    }

    SECTION("Get visible achievements") {
        auto visible = registry.get_visible_achievements();
        REQUIRE(visible.size() == 3); // Excludes the hidden one
    }

    SECTION("Get hidden achievements") {
        auto hidden = registry.get_hidden_achievements();
        REQUIRE(hidden.size() == 1);
        REQUIRE(hidden[0] == "test_secret_1");
    }
}

TEST_CASE_METHOD(RegistryFixture, "AchievementRegistry statistics", "[achievements][registry]") {
    auto& registry = achievement_registry();
    register_test_achievements();

    SECTION("Total achievements count") {
        REQUIRE(registry.get_total_achievements() == 4);
    }

    SECTION("Total points") {
        // 10 + 20 + 15 + 50 = 95
        REQUIRE(registry.get_total_points() == 95);
    }
}

TEST_CASE_METHOD(RegistryFixture, "AchievementRegistry clear", "[achievements][registry]") {
    auto& registry = achievement_registry();
    register_test_achievements();

    REQUIRE(registry.get_total_achievements() == 4);

    registry.clear();

    REQUIRE(registry.get_total_achievements() == 0);
    REQUIRE_FALSE(registry.exists("test_combat_1"));
}
