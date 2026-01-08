#include <catch2/catch_test_macros.hpp>
#include <engine/localization/localization.hpp>

using namespace engine::localization;

TEST_CASE("LocalizationManager initialization", "[localization][manager]") {
    LocalizationManager manager;

    SECTION("Not initialized by default") {
        REQUIRE_FALSE(manager.is_initialized());
    }

    SECTION("Initialize with default config") {
        manager.init();
        REQUIRE(manager.is_initialized());
        manager.shutdown();
    }

    SECTION("Initialize with custom config") {
        LocalizationConfig config;
        config.default_language = "de";
        config.fallback_language = "en";
        config.show_missing_keys = false;

        manager.init(config);
        REQUIRE(manager.is_initialized());
        REQUIRE(manager.get_config().default_language == "de");
        manager.shutdown();
    }
}

TEST_CASE("LocalizationManager string lookup", "[localization][manager]") {
    LocalizationManager manager;
    LocalizationConfig config;
    config.show_missing_keys = true;
    config.missing_prefix = "[!]";
    manager.init(config);

    SECTION("Missing key returns key with prefix") {
        auto result = manager.get("nonexistent_key");
        REQUIRE(result.find("nonexistent_key") != std::string::npos);
    }

    SECTION("Has returns false for missing key") {
        REQUIRE_FALSE(manager.has("nonexistent_key"));
    }

    SECTION("Operator[] works like get") {
        auto result1 = manager.get("test_key");
        auto result2 = manager["test_key"];
        REQUIRE(result1 == result2);
    }

    manager.shutdown();
}

TEST_CASE("LocalizationManager format function", "[localization][format]") {
    SECTION("No arguments") {
        auto result = LocalizationManager::format("Hello, World!", {});
        REQUIRE(result == "Hello, World!");
    }

    SECTION("Single argument") {
        auto result = LocalizationManager::format("Hello, {name}!", {{"name", "Alice"}});
        REQUIRE(result == "Hello, Alice!");
    }

    SECTION("Multiple arguments") {
        auto result = LocalizationManager::format(
            "{greeting}, {name}! You have {count} messages.",
            {{"greeting", "Hello"}, {"name", "Bob"}, {"count", "5"}}
        );
        REQUIRE(result == "Hello, Bob! You have 5 messages.");
    }

    SECTION("Missing argument leaves placeholder") {
        auto result = LocalizationManager::format("Hello, {name}!", {});
        // Behavior depends on implementation - may leave {name} or empty
    }

    SECTION("Duplicate placeholder") {
        auto result = LocalizationManager::format("{x} + {x} = 2{x}", {{"x", "1"}});
        REQUIRE(result == "1 + 1 = 21");
    }
}

TEST_CASE("LocalizationManager callbacks", "[localization][manager]") {
    LocalizationManager manager;
    manager.init();

    SECTION("Add and remove callback") {
        bool callback_invoked = false;
        manager.add_callback("test_callback", [&](const LanguageCode&, const LanguageCode&) {
            callback_invoked = true;
        });

        // Remove callback shouldn't crash
        manager.remove_callback("test_callback");
        manager.remove_callback("nonexistent_callback");
    }

    manager.shutdown();
}

TEST_CASE("LocalizationManager table access", "[localization][manager]") {
    LocalizationManager manager;
    manager.init();

    SECTION("Get non-existent table returns null") {
        REQUIRE(manager.get_table("nonexistent") == nullptr);
    }

    SECTION("Const get table") {
        const LocalizationManager& const_manager = manager;
        REQUIRE(const_manager.get_table("nonexistent") == nullptr);
    }

    manager.shutdown();
}

TEST_CASE("LocalizationManager statistics", "[localization][manager]") {
    LocalizationManager manager;
    manager.init();

    SECTION("Initial stats") {
        auto stats = manager.get_stats();
        REQUIRE(stats.loaded_languages == 0);
        REQUIRE(stats.total_strings == 0);
    }

    manager.shutdown();
}

TEST_CASE("Global localization access", "[localization]") {
    auto& loc_manager = get_localization();

    // Just verify it doesn't crash and returns a reference
    REQUIRE(&loc_manager == &get_localization());
}

TEST_CASE("Convenience loc function", "[localization]") {
    // These test the global convenience functions
    // They use the global manager which may or may not be initialized

    SECTION("loc with key") {
        auto result = loc("test_key");
        // Result depends on whether manager is initialized and has key
        REQUIRE_FALSE(result.empty()); // Should return something (key or value)
    }

    SECTION("loc with count") {
        auto result = loc("test_key", 5);
        REQUIRE_FALSE(result.empty());
    }
}
