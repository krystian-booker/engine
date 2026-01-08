#include <catch2/catch_test_macros.hpp>
#include <engine/achievements/achievement_definition.hpp>

using namespace engine::achievements;

TEST_CASE("AchievementDefinition default values", "[achievements][definition]") {
    AchievementDefinition def;

    REQUIRE(def.achievement_id.empty());
    REQUIRE(def.display_name.empty());
    REQUIRE(def.type == AchievementType::Binary);
    REQUIRE(def.category == AchievementCategory::Misc);
    REQUIRE(def.target_count == 1);
    REQUIRE(def.is_hidden == false);
    REQUIRE(def.points == 0);
    REQUIRE(def.tiers.empty());
}

TEST_CASE("AchievementDefinition tiered helpers", "[achievements][definition]") {
    AchievementDefinition def;

    SECTION("Non-tiered achievement") {
        def.type = AchievementType::Binary;
        REQUIRE_FALSE(def.is_tiered());
        REQUIRE(def.get_tier_count() == 0);
    }

    SECTION("Tiered achievement with tiers") {
        def.type = AchievementType::Tiered;
        def.tiers.push_back({"tier1", "Bronze", 10, 10, {}});
        def.tiers.push_back({"tier2", "Silver", 50, 25, {}});
        def.tiers.push_back({"tier3", "Gold", 100, 50, {}});

        REQUIRE(def.is_tiered());
        REQUIRE(def.get_tier_count() == 3);
    }

    SECTION("Tiered type but empty tiers") {
        def.type = AchievementType::Tiered;
        REQUIRE_FALSE(def.is_tiered());
    }

    SECTION("Get tier by index") {
        def.type = AchievementType::Tiered;
        def.tiers.push_back({"tier1", "Bronze", 10, 10, {}});
        def.tiers.push_back({"tier2", "Silver", 50, 25, {}});

        const auto* tier0 = def.get_tier(0);
        REQUIRE(tier0 != nullptr);
        REQUIRE(tier0->tier_id == "tier1");
        REQUIRE(tier0->target_count == 10);

        const auto* tier1 = def.get_tier(1);
        REQUIRE(tier1 != nullptr);
        REQUIRE(tier1->tier_id == "tier2");

        // Out of bounds
        REQUIRE(def.get_tier(2) == nullptr);
        REQUIRE(def.get_tier(-1) == nullptr);
    }
}

TEST_CASE("AchievementDefinition total points", "[achievements][definition]") {
    AchievementDefinition def;

    SECTION("Binary achievement points") {
        def.type = AchievementType::Binary;
        def.points = 50;
        REQUIRE(def.get_total_points() == 50);
    }

    SECTION("Tiered achievement points") {
        def.type = AchievementType::Tiered;
        def.points = 0; // Base points ignored for tiered
        def.tiers.push_back({"tier1", "Bronze", 10, 10, {}});
        def.tiers.push_back({"tier2", "Silver", 50, 25, {}});
        def.tiers.push_back({"tier3", "Gold", 100, 50, {}});

        // Total should be sum of tier points
        REQUIRE(def.get_total_points() == 85); // 10 + 25 + 50
    }
}

TEST_CASE("AchievementTier structure", "[achievements][tier]") {
    AchievementTier tier;
    tier.tier_id = "bronze";
    tier.display_name = "Bronze";
    tier.target_count = 10;
    tier.points = 10;
    tier.rewards = {"reward_skin_1", "reward_title_1"};

    REQUIRE(tier.tier_id == "bronze");
    REQUIRE(tier.display_name == "Bronze");
    REQUIRE(tier.target_count == 10);
    REQUIRE(tier.points == 10);
    REQUIRE(tier.rewards.size() == 2);
}

TEST_CASE("AchievementType enum", "[achievements][enum]") {
    REQUIRE(static_cast<uint8_t>(AchievementType::Binary) == 0);
    REQUIRE(static_cast<uint8_t>(AchievementType::Counter) == 1);
    REQUIRE(static_cast<uint8_t>(AchievementType::Progress) == 2);
    REQUIRE(static_cast<uint8_t>(AchievementType::Tiered) == 3);
}

TEST_CASE("AchievementCategory enum", "[achievements][enum]") {
    REQUIRE(static_cast<uint8_t>(AchievementCategory::Story) == 0);
    REQUIRE(static_cast<uint8_t>(AchievementCategory::Combat) == 1);
    REQUIRE(static_cast<uint8_t>(AchievementCategory::Exploration) == 2);
    REQUIRE(static_cast<uint8_t>(AchievementCategory::Collection) == 3);
    REQUIRE(static_cast<uint8_t>(AchievementCategory::Challenge) == 4);
    REQUIRE(static_cast<uint8_t>(AchievementCategory::Social) == 5);
    REQUIRE(static_cast<uint8_t>(AchievementCategory::Secret) == 6);
    REQUIRE(static_cast<uint8_t>(AchievementCategory::Misc) == 7);
}
