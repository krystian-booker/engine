#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/achievements/achievement_manager.hpp>
#include <engine/achievements/achievement_definition.hpp>

using namespace engine::achievements;
using Catch::Matchers::WithinAbs;

class ManagerFixture {
protected:
    ManagerFixture() {
        // Clear registry and reset progress
        achievement_registry().clear();
        achievements().reset_progress();
        setup_test_achievements();
    }

    ~ManagerFixture() {
        achievement_registry().clear();
        achievements().reset_progress();
    }

    void setup_test_achievements() {
        achievement()
            .id("binary_test")
            .name("Binary Achievement")
            .type(AchievementType::Binary)
            .points(10)
            .register_achievement();

        achievement()
            .id("counter_test")
            .name("Counter Achievement")
            .type(AchievementType::Counter)
            .target(10)
            .points(25)
            .register_achievement();

        achievement()
            .id("tiered_test")
            .name("Tiered Achievement")
            .type(AchievementType::Tiered)
            .tier("bronze", "Bronze", 5, 10)
            .tier("silver", "Silver", 25, 25)
            .tier("gold", "Gold", 50, 50)
            .register_achievement();

        achievement()
            .id("with_prereq")
            .name("With Prerequisite")
            .prerequisite("binary_test")
            .points(30)
            .register_achievement();
    }
};

TEST_CASE_METHOD(ManagerFixture, "AchievementManager singleton", "[achievements][manager]") {
    auto& manager = achievements();
    auto& manager2 = AchievementManager::instance();

    REQUIRE(&manager == &manager2);
}

TEST_CASE_METHOD(ManagerFixture, "AchievementManager binary achievement unlock", "[achievements][manager]") {
    auto& manager = achievements();

    SECTION("Initial state is locked") {
        REQUIRE_FALSE(manager.is_unlocked("binary_test"));
        REQUIRE(manager.get_progress("binary_test") == 0);
    }

    SECTION("Unlock achievement") {
        manager.unlock("binary_test");
        REQUIRE(manager.is_unlocked("binary_test"));
    }

    SECTION("Unlock non-existent achievement is safe") {
        REQUIRE_NOTHROW(manager.unlock("nonexistent"));
    }
}

TEST_CASE_METHOD(ManagerFixture, "AchievementManager counter achievement", "[achievements][manager]") {
    auto& manager = achievements();

    SECTION("Increment progress") {
        manager.increment("counter_test", 3);
        REQUIRE(manager.get_progress("counter_test") == 3);

        manager.increment("counter_test", 2);
        REQUIRE(manager.get_progress("counter_test") == 5);
    }

    SECTION("Increment unlocks at target") {
        manager.increment("counter_test", 10);
        REQUIRE(manager.is_unlocked("counter_test"));
    }

    SECTION("Increment beyond target") {
        manager.increment("counter_test", 15);
        REQUIRE(manager.get_progress("counter_test") >= 10);
        REQUIRE(manager.is_unlocked("counter_test"));
    }

    SECTION("Set progress directly") {
        manager.set_progress("counter_test", 7);
        REQUIRE(manager.get_progress("counter_test") == 7);
    }

    SECTION("Progress percent") {
        manager.set_progress("counter_test", 5);
        REQUIRE_THAT(manager.get_progress_percent("counter_test"), WithinAbs(0.5f, 0.01f));

        manager.set_progress("counter_test", 10);
        REQUIRE_THAT(manager.get_progress_percent("counter_test"), WithinAbs(1.0f, 0.01f));
    }
}

TEST_CASE_METHOD(ManagerFixture, "AchievementManager tiered achievement", "[achievements][manager]") {
    auto& manager = achievements();

    SECTION("Initial tier state") {
        REQUIRE(manager.get_current_tier("tiered_test") == 0);
        REQUIRE_FALSE(manager.is_tier_unlocked("tiered_test", 0));
        REQUIRE_FALSE(manager.is_tier_unlocked("tiered_test", 1));
        REQUIRE_FALSE(manager.is_tier_unlocked("tiered_test", 2));
    }

    SECTION("Unlock first tier") {
        manager.increment("tiered_test", 5);
        REQUIRE(manager.is_tier_unlocked("tiered_test", 0));
        REQUIRE_FALSE(manager.is_tier_unlocked("tiered_test", 1));
    }

    SECTION("Unlock multiple tiers") {
        manager.increment("tiered_test", 25);
        REQUIRE(manager.is_tier_unlocked("tiered_test", 0));
        REQUIRE(manager.is_tier_unlocked("tiered_test", 1));
        REQUIRE_FALSE(manager.is_tier_unlocked("tiered_test", 2));
    }

    SECTION("Unlock all tiers") {
        manager.increment("tiered_test", 50);
        REQUIRE(manager.is_tier_unlocked("tiered_test", 0));
        REQUIRE(manager.is_tier_unlocked("tiered_test", 1));
        REQUIRE(manager.is_tier_unlocked("tiered_test", 2));
        REQUIRE(manager.is_unlocked("tiered_test"));
    }
}

TEST_CASE_METHOD(ManagerFixture, "AchievementManager bulk queries", "[achievements][manager]") {
    auto& manager = achievements();

    SECTION("Get all unlocked (initially empty)") {
        auto unlocked = manager.get_all_unlocked();
        REQUIRE(unlocked.empty());
    }

    SECTION("Get all locked") {
        auto locked = manager.get_all_locked();
        REQUIRE(locked.size() == 4);
    }

    SECTION("After unlocking some") {
        manager.unlock("binary_test");
        manager.increment("counter_test", 10);

        auto unlocked = manager.get_all_unlocked();
        REQUIRE(unlocked.size() == 2);

        auto locked = manager.get_all_locked();
        REQUIRE(locked.size() == 2);
    }

    SECTION("Get in progress") {
        manager.increment("counter_test", 5); // Partial progress
        auto in_progress = manager.get_in_progress();
        REQUIRE(in_progress.size() >= 1);
    }

    SECTION("Get by category") {
        auto misc = manager.get_by_category(AchievementCategory::Misc);
        REQUIRE(misc.size() == 4); // All test achievements are Misc
    }
}

TEST_CASE_METHOD(ManagerFixture, "AchievementManager statistics", "[achievements][manager]") {
    auto& manager = achievements();

    SECTION("Initial stats") {
        REQUIRE(manager.get_unlocked_count() == 0);
        REQUIRE(manager.get_total_count() == 4);
        REQUIRE_THAT(manager.get_completion_percent(), WithinAbs(0.0f, 0.01f));
    }

    SECTION("After unlocking") {
        manager.unlock("binary_test");
        REQUIRE(manager.get_unlocked_count() == 1);
        REQUIRE_THAT(manager.get_completion_percent(), WithinAbs(0.25f, 0.01f));
    }

    SECTION("Earned points") {
        REQUIRE(manager.get_earned_points() == 0);

        manager.unlock("binary_test"); // 10 points
        REQUIRE(manager.get_earned_points() == 10);
    }

    SECTION("Total points") {
        // Binary: 10, Counter: 25, Tiered: 10+25+50=85, WithPrereq: 30
        REQUIRE(manager.get_total_points() == 150);
    }
}

TEST_CASE_METHOD(ManagerFixture, "AchievementManager progress reset", "[achievements][manager]") {
    auto& manager = achievements();

    manager.unlock("binary_test");
    manager.increment("counter_test", 5);

    SECTION("Reset all progress") {
        manager.reset_progress();

        REQUIRE_FALSE(manager.is_unlocked("binary_test"));
        REQUIRE(manager.get_progress("counter_test") == 0);
        REQUIRE(manager.get_unlocked_count() == 0);
    }

    SECTION("Reset single achievement") {
        manager.reset_achievement("counter_test");

        REQUIRE(manager.is_unlocked("binary_test")); // Still unlocked
        REQUIRE(manager.get_progress("counter_test") == 0);
    }
}

TEST_CASE_METHOD(ManagerFixture, "AchievementManager callbacks", "[achievements][manager]") {
    auto& manager = achievements();

    SECTION("On unlock callback") {
        bool callback_invoked = false;
        std::string unlocked_id;

        manager.set_on_unlock([&](const AchievementDefinition& def) {
            callback_invoked = true;
            unlocked_id = def.achievement_id;
        });

        manager.unlock("binary_test");

        REQUIRE(callback_invoked);
        REQUIRE(unlocked_id == "binary_test");

        // Clear callback
        manager.set_on_unlock(nullptr);
    }

    SECTION("On progress callback") {
        int last_current = 0;
        int last_target = 0;

        manager.set_on_progress([&](const std::string&, int current, int target) {
            last_current = current;
            last_target = target;
        });

        manager.increment("counter_test", 3);

        REQUIRE(last_current == 3);
        REQUIRE(last_target == 10);

        manager.set_on_progress(nullptr);
    }
}

TEST_CASE_METHOD(ManagerFixture, "AchievementManager notifications", "[achievements][manager]") {
    auto& manager = achievements();

    SECTION("No notifications initially") {
        REQUIRE_FALSE(manager.has_pending_notifications());
        REQUIRE(manager.get_pending_notifications().empty());
    }

    SECTION("Notification on unlock") {
        manager.unlock("binary_test");

        REQUIRE(manager.has_pending_notifications());
        auto notifications = manager.get_pending_notifications();
        REQUIRE(notifications.size() >= 1);
        REQUIRE(notifications[0].achievement_id == "binary_test");
    }

    SECTION("Clear notifications") {
        manager.unlock("binary_test");
        REQUIRE(manager.has_pending_notifications());

        manager.clear_notifications();
        REQUIRE_FALSE(manager.has_pending_notifications());
    }
}

TEST_CASE_METHOD(ManagerFixture, "AchievementManager debug functions", "[achievements][manager]") {
    auto& manager = achievements();

    SECTION("Unlock all") {
        manager.unlock_all();
        REQUIRE(manager.get_unlocked_count() == manager.get_total_count());
    }

    SECTION("Lock all") {
        manager.unlock_all();
        manager.lock_all();
        REQUIRE(manager.get_unlocked_count() == 0);
    }
}

TEST_CASE_METHOD(ManagerFixture, "AchievementManager achievement progress data", "[achievements][manager]") {
    auto& manager = achievements();

    SECTION("Get progress for non-started achievement") {
        const auto* progress = manager.get_achievement_progress("binary_test");
        // May return null or empty progress
        if (progress) {
            REQUIRE_FALSE(progress->unlocked);
        }
    }

    SECTION("Get progress after activity") {
        manager.increment("counter_test", 5);

        const auto* progress = manager.get_achievement_progress("counter_test");
        REQUIRE(progress != nullptr);
        REQUIRE(progress->achievement_id == "counter_test");
        REQUIRE(progress->current_count == 5);
    }
}
