#include <catch2/catch_test_macros.hpp>
#include <engine/achievements/achievement_definition.hpp>

using namespace engine::achievements;

TEST_CASE("AchievementBuilder basic building", "[achievements][builder]") {
    SECTION("Build binary achievement") {
        auto def = achievement()
            .id("first_blood")
            .name("First Blood")
            .description("Defeat your first enemy")
            .type(AchievementType::Binary)
            .category(AchievementCategory::Combat)
            .points(10)
            .build();

        REQUIRE(def.achievement_id == "first_blood");
        REQUIRE(def.display_name == "First Blood");
        REQUIRE(def.description == "Defeat your first enemy");
        REQUIRE(def.type == AchievementType::Binary);
        REQUIRE(def.category == AchievementCategory::Combat);
        REQUIRE(def.points == 10);
    }

    SECTION("Build counter achievement") {
        auto def = achievement()
            .id("enemy_slayer")
            .name("Enemy Slayer")
            .description("Defeat 100 enemies")
            .type(AchievementType::Counter)
            .category(AchievementCategory::Combat)
            .target(100)
            .points(50)
            .build();

        REQUIRE(def.achievement_id == "enemy_slayer");
        REQUIRE(def.type == AchievementType::Counter);
        REQUIRE(def.target_count == 100);
        REQUIRE(def.points == 50);
    }

    SECTION("Build progress achievement") {
        auto def = achievement()
            .id("map_explorer")
            .name("Map Explorer")
            .description("Explore 50% of the map")
            .type(AchievementType::Progress)
            .category(AchievementCategory::Exploration)
            .target(50)
            .points(25)
            .build();

        REQUIRE(def.type == AchievementType::Progress);
        REQUIRE(def.target_count == 50);
    }
}

TEST_CASE("AchievementBuilder tiered achievement", "[achievements][builder]") {
    auto def = achievement()
        .id("collector")
        .name("Collector")
        .description("Collect items")
        .type(AchievementType::Tiered)
        .category(AchievementCategory::Collection)
        .tier("bronze", "Bronze Collector", 10, 10)
        .tier("silver", "Silver Collector", 50, 25)
        .tier("gold", "Gold Collector", 100, 50)
        .build();

    REQUIRE(def.type == AchievementType::Tiered);
    REQUIRE(def.tiers.size() == 3);
    REQUIRE(def.tiers[0].tier_id == "bronze");
    REQUIRE(def.tiers[0].target_count == 10);
    REQUIRE(def.tiers[0].points == 10);
    REQUIRE(def.tiers[1].tier_id == "silver");
    REQUIRE(def.tiers[2].tier_id == "gold");
    REQUIRE(def.get_total_points() == 85);
}

TEST_CASE("AchievementBuilder hidden achievement", "[achievements][builder]") {
    SECTION("Simple hidden") {
        auto def = achievement()
            .id("secret_ending")
            .name("???")
            .description("Find the secret ending")
            .hidden_description("A hidden achievement")
            .hidden()
            .build();

        REQUIRE(def.is_hidden == true);
        REQUIRE(def.is_hidden_until_progress == false);
    }

    SECTION("Hidden until progress") {
        auto def = achievement()
            .id("hidden_collector")
            .name("Hidden Collector")
            .hidden(true, 0.25f)
            .build();

        REQUIRE(def.is_hidden == true);
        REQUIRE(def.is_hidden_until_progress == true);
        REQUIRE(def.hidden_progress_threshold == 0.25f);
    }
}

TEST_CASE("AchievementBuilder prerequisites", "[achievements][builder]") {
    auto def = achievement()
        .id("master_warrior")
        .name("Master Warrior")
        .prerequisite("warrior_1")
        .prerequisite("warrior_2")
        .prerequisite("warrior_3")
        .build();

    REQUIRE(def.prerequisites.size() == 3);
    REQUIRE(def.prerequisites[0] == "warrior_1");
    REQUIRE(def.prerequisites[1] == "warrior_2");
    REQUIRE(def.prerequisites[2] == "warrior_3");
}

TEST_CASE("AchievementBuilder rewards", "[achievements][builder]") {
    auto def = achievement()
        .id("completionist")
        .name("Completionist")
        .reward("skin_gold")
        .reward("title_master")
        .reward("badge_complete")
        .build();

    REQUIRE(def.unlock_rewards.size() == 3);
    REQUIRE(def.unlock_rewards[0] == "skin_gold");
    REQUIRE(def.unlock_rewards[1] == "title_master");
    REQUIRE(def.unlock_rewards[2] == "badge_complete");
}

TEST_CASE("AchievementBuilder icons and platform", "[achievements][builder]") {
    auto def = achievement()
        .id("test_achievement")
        .name("Test")
        .icon("icons/achievement.png")
        .locked_icon("icons/achievement_locked.png")
        .platform_id("STEAM_ACH_001")
        .order(5)
        .build();

    REQUIRE(def.icon_path == "icons/achievement.png");
    REQUIRE(def.icon_locked_path == "icons/achievement_locked.png");
    REQUIRE(def.platform_id == "STEAM_ACH_001");
    REQUIRE(def.display_order == 5);
}

TEST_CASE("AchievementBuilder fluent chain", "[achievements][builder]") {
    // Test that all methods return builder reference for chaining
    auto def = achievement()
        .id("full_test")
        .name("Full Test")
        .description("Test all builder methods")
        .hidden_description("Hidden description")
        .icon("icon.png")
        .locked_icon("locked.png")
        .type(AchievementType::Counter)
        .category(AchievementCategory::Challenge)
        .target(10)
        .hidden(false, 0.5f)
        .prerequisite("prereq1")
        .points(100)
        .reward("reward1")
        .platform_id("PLATFORM_001")
        .order(1)
        .build();

    // Just verify it compiled and built without errors
    REQUIRE(def.achievement_id == "full_test");
    REQUIRE(def.display_name == "Full Test");
}
