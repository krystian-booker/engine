#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <engine/save/save_game.hpp>

using namespace engine::save;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("SaveGameMetadata default values", "[save][metadata]") {
    SaveGameMetadata metadata;

    SECTION("Default name is empty") {
        REQUIRE(metadata.name.empty());
    }

    SECTION("Default description is empty") {
        REQUIRE(metadata.description.empty());
    }

    SECTION("Default version is 1") {
        REQUIRE(metadata.version == 1);
    }

    SECTION("Default timestamp is 0") {
        REQUIRE(metadata.timestamp == 0);
    }

    SECTION("Default play time is 0") {
        REQUIRE(metadata.play_time_seconds == 0);
    }
}

TEST_CASE("SaveGameMetadata custom values", "[save][metadata]") {
    SaveGameMetadata metadata;
    metadata.name = "Chapter 1 Complete";
    metadata.description = "Hard mode run";
    metadata.version = 3;
    metadata.timestamp = 1700000000;
    metadata.play_time_seconds = 3600;
    metadata.level_name = "forest";

    REQUIRE(metadata.name == "Chapter 1 Complete");
    REQUIRE(metadata.description == "Hard mode run");
    REQUIRE(metadata.version == 3);
    REQUIRE(metadata.timestamp == 1700000000);
    REQUIRE(metadata.play_time_seconds == 3600);
    REQUIRE(metadata.level_name == "forest");
}

TEST_CASE("SaveGame construction", "[save][savegame]") {
    SaveGame save;

    SECTION("Default construction") {
        REQUIRE(save.metadata().name.empty());
        REQUIRE(save.metadata().version == 1);
    }
}

TEST_CASE("SaveGame typed value storage", "[save][savegame]") {
    SaveGame save;

    SECTION("Store and retrieve int") {
        save.set_value("score", 12345);
        REQUIRE(save.get_value<int>("score") == 12345);
    }

    SECTION("Store and retrieve float") {
        save.set_value("health", 75.5f);
        REQUIRE(save.get_value<float>("health") == 75.5f);
    }

    SECTION("Store and retrieve bool") {
        save.set_value("completed", true);
        REQUIRE(save.get_value<bool>("completed") == true);

        save.set_value("failed", false);
        REQUIRE(save.get_value<bool>("failed") == false);
    }

    SECTION("Store and retrieve string") {
        save.set_value("player_name", std::string("Hero"));
        REQUIRE(save.get_value<std::string>("player_name") == "Hero");
    }

    SECTION("Overwrite existing value") {
        save.set_value("counter", 10);
        REQUIRE(save.get_value<int>("counter") == 10);

        save.set_value("counter", 20);
        REQUIRE(save.get_value<int>("counter") == 20);
    }
}

TEST_CASE("SaveGame value existence checks", "[save][savegame]") {
    SaveGame save;

    SECTION("Has returns false for non-existent key") {
        REQUIRE_FALSE(save.has_data("nonexistent"));
    }

    SECTION("Has returns true for existing key") {
        save.set_value("exists", 42);
        REQUIRE(save.has_data("exists"));
    }

    SECTION("Remove value") {
        save.set_value("temporary", 100);
        REQUIRE(save.has_data("temporary"));

        save.remove_data("temporary");
        REQUIRE_FALSE(save.has_data("temporary"));
    }
}

TEST_CASE("SaveGame default values", "[save][savegame]") {
    SaveGame save;

    SECTION("Get with default for missing key") {
        int result = save.get_value("missing_int", 999);
        REQUIRE(result == 999);
    }

    SECTION("Get with default when key exists") {
        save.set_value("present", 42);
        int result = save.get_value("present", 999);
        REQUIRE(result == 42);
    }

    SECTION("String default") {
        std::string result = save.get_value<std::string>("missing_string", "default");
        REQUIRE(result == "default");
    }
}

TEST_CASE("SaveGame entity data", "[save][savegame]") {
    SaveGame save;

    SECTION("Entity data storage") {
        uint64_t id = 12345;
        std::string data = R"({"name":"Player","hp":100})";
        
        save.set_entity_data(id, data);

        REQUIRE(save.has_entity_data(id));
        REQUIRE(save.get_entity_data(id) == data);
        REQUIRE(save.get_all_entity_ids().size() == 1);
        REQUIRE(save.get_all_entity_ids()[0] == id);
    }

    SECTION("Multiple entities") {
        save.set_entity_data(1, "Player");
        save.set_entity_data(2, "Enemy");

        REQUIRE(save.get_all_entity_ids().size() == 2);
        REQUIRE(save.get_entity_data(1) == "Player");
        REQUIRE(save.get_entity_data(2) == "Enemy");
    }
}

TEST_CASE("SaveGame clear", "[save][savegame]") {
    SaveGame save;
    save.set_value("key1", 100);
    save.set_entity_data(1, "data");

    SECTION("Clear all") {
        save.clear();
        REQUIRE_FALSE(save.has_data("key1"));
        REQUIRE(save.get_all_entity_ids().empty());
    }
    
    SECTION("Clear entities") {
        save.clear_entity_data();
        REQUIRE(save.has_data("key1")); // Values not cleared
        REQUIRE(save.get_all_entity_ids().empty());
    }
}
