// engine/ai/tests/test_blackboard.cpp
// Happy path tests for Blackboard key-value store

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <engine/ai/blackboard.hpp>
#include <engine/scene/entity.hpp>

using namespace engine::ai;
using namespace engine::core;
using namespace engine::scene;
using Catch::Matchers::WithinAbs;

TEST_CASE("Blackboard basic operations", "[ai][blackboard]") {
    Blackboard bb;

    SECTION("Empty blackboard") {
        REQUIRE(bb.empty());
        REQUIRE(bb.size() == 0);
    }

    SECTION("Set and get int") {
        bb.set<int>("health", 100);
        REQUIRE(bb.get<int>("health") == 100);
        REQUIRE(bb.size() == 1);
    }

    SECTION("Set and get float") {
        bb.set<float>("speed", 5.5f);
        REQUIRE_THAT(bb.get<float>("speed"), WithinAbs(5.5f, 0.001f));
    }

    SECTION("Set and get string") {
        bb.set<std::string>("name", "Agent");
        REQUIRE(bb.get<std::string>("name") == "Agent");
    }

    SECTION("Set and get Vec3") {
        Vec3 pos{1.0f, 2.0f, 3.0f};
        bb.set<Vec3>("position", pos);
        Vec3 result = bb.get<Vec3>("position");
        REQUIRE_THAT(result.x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(result.y, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(result.z, WithinAbs(3.0f, 0.001f));
    }

    SECTION("Get with default value for missing key") {
        REQUIRE(bb.get<int>("missing", 42) == 42);
        REQUIRE_THAT(bb.get<float>("missing", 3.14f), WithinAbs(3.14f, 0.001f));
        REQUIRE(bb.get<std::string>("missing", "default") == "default");
    }
}

TEST_CASE("Blackboard type shortcuts", "[ai][blackboard]") {
    Blackboard bb;

    SECTION("Float shortcuts") {
        bb.set_float("damage", 25.5f);
        REQUIRE_THAT(bb.get_float("damage"), WithinAbs(25.5f, 0.001f));
        REQUIRE_THAT(bb.get_float("missing", 10.0f), WithinAbs(10.0f, 0.001f));
    }

    SECTION("Int shortcuts") {
        bb.set_int("score", 1000);
        REQUIRE(bb.get_int("score") == 1000);
        REQUIRE(bb.get_int("missing", -1) == -1);
    }

    SECTION("Bool shortcuts") {
        bb.set_bool("is_active", true);
        REQUIRE(bb.get_bool("is_active") == true);
        bb.set_bool("is_active", false);
        REQUIRE(bb.get_bool("is_active") == false);
        REQUIRE(bb.get_bool("missing", true) == true);
    }

    SECTION("String shortcuts") {
        bb.set_string("state", "patrol");
        REQUIRE(bb.get_string("state") == "patrol");
        REQUIRE(bb.get_string("missing", "idle") == "idle");
    }

    SECTION("Position shortcuts") {
        Vec3 pos{10.0f, 20.0f, 30.0f};
        bb.set_position("target", pos);
        Vec3 result = bb.get_position("target");
        REQUIRE_THAT(result.x, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(result.y, WithinAbs(20.0f, 0.001f));
        REQUIRE_THAT(result.z, WithinAbs(30.0f, 0.001f));
    }

    SECTION("Entity shortcuts") {
        Entity entity{42};
        bb.set_entity("target", entity);
        REQUIRE(bb.get_entity("target") == entity);
        REQUIRE(bb.get_entity("missing") == NullEntity);
    }
}

TEST_CASE("Blackboard has and remove", "[ai][blackboard]") {
    Blackboard bb;
    bb.set<int>("a", 1);
    bb.set<int>("b", 2);

    SECTION("has returns true for existing keys") {
        REQUIRE(bb.has("a"));
        REQUIRE(bb.has("b"));
    }

    SECTION("has returns false for missing keys") {
        REQUIRE_FALSE(bb.has("c"));
    }

    SECTION("remove deletes key") {
        bb.remove("a");
        REQUIRE_FALSE(bb.has("a"));
        REQUIRE(bb.has("b"));
        REQUIRE(bb.size() == 1);
    }

    SECTION("remove non-existent key is safe") {
        bb.remove("nonexistent");
        REQUIRE(bb.size() == 2);
    }
}

TEST_CASE("Blackboard try_get", "[ai][blackboard]") {
    Blackboard bb;
    bb.set<int>("value", 123);

    SECTION("try_get returns pointer for existing key") {
        int* ptr = bb.try_get<int>("value");
        REQUIRE(ptr != nullptr);
        REQUIRE(*ptr == 123);
    }

    SECTION("try_get returns nullptr for missing key") {
        int* ptr = bb.try_get<int>("missing");
        REQUIRE(ptr == nullptr);
    }

    SECTION("const try_get works") {
        const Blackboard& const_bb = bb;
        const int* ptr = const_bb.try_get<int>("value");
        REQUIRE(ptr != nullptr);
        REQUIRE(*ptr == 123);
    }
}

TEST_CASE("Blackboard get_optional", "[ai][blackboard]") {
    Blackboard bb;
    bb.set<int>("value", 456);

    SECTION("get_optional returns value for existing key") {
        auto opt = bb.get_optional<int>("value");
        REQUIRE(opt.has_value());
        REQUIRE(*opt == 456);
    }

    SECTION("get_optional returns nullopt for missing key") {
        auto opt = bb.get_optional<int>("missing");
        REQUIRE_FALSE(opt.has_value());
    }
}

TEST_CASE("Blackboard clear", "[ai][blackboard]") {
    Blackboard bb;
    bb.set<int>("a", 1);
    bb.set<int>("b", 2);
    bb.set<int>("c", 3);

    REQUIRE(bb.size() == 3);

    bb.clear();

    REQUIRE(bb.empty());
    REQUIRE(bb.size() == 0);
    REQUIRE_FALSE(bb.has("a"));
}

TEST_CASE("Blackboard get_keys", "[ai][blackboard]") {
    Blackboard bb;
    bb.set<int>("alpha", 1);
    bb.set<int>("beta", 2);
    bb.set<int>("gamma", 3);

    auto keys = bb.get_keys();
    REQUIRE(keys.size() == 3);

    // Keys are unordered, so check that all are present
    REQUIRE(std::find(keys.begin(), keys.end(), "alpha") != keys.end());
    REQUIRE(std::find(keys.begin(), keys.end(), "beta") != keys.end());
    REQUIRE(std::find(keys.begin(), keys.end(), "gamma") != keys.end());
}

TEST_CASE("Blackboard copy_from", "[ai][blackboard]") {
    Blackboard source;
    source.set<int>("a", 1);
    source.set<int>("b", 2);

    Blackboard dest;
    dest.set<int>("c", 3);

    dest.copy_from(source);

    SECTION("Copies all keys from source") {
        REQUIRE(dest.has("a"));
        REQUIRE(dest.has("b"));
        REQUIRE(dest.get<int>("a") == 1);
        REQUIRE(dest.get<int>("b") == 2);
    }

    SECTION("Preserves existing keys") {
        REQUIRE(dest.has("c"));
        REQUIRE(dest.get<int>("c") == 3);
    }

    SECTION("Overwrites existing keys with same name") {
        Blackboard bb1;
        bb1.set<int>("x", 100);

        Blackboard bb2;
        bb2.set<int>("x", 200);

        bb1.copy_from(bb2);
        REQUIRE(bb1.get<int>("x") == 200);
    }
}

TEST_CASE("Blackboard merge", "[ai][blackboard]") {
    Blackboard source;
    source.set<int>("new_key", 999);
    source.set<int>("existing", 100);

    Blackboard dest;
    dest.set<int>("existing", 50);

    dest.merge(source);

    SECTION("Adds new keys") {
        REQUIRE(dest.has("new_key"));
        REQUIRE(dest.get<int>("new_key") == 999);
    }

    SECTION("Does not overwrite existing keys") {
        REQUIRE(dest.get<int>("existing") == 50);
    }
}

TEST_CASE("Blackboard predefined keys exist", "[ai][blackboard]") {
    // Test that the predefined key constants compile and are reasonable
    REQUIRE(std::string(bb::TARGET_ENTITY) == "target_entity");
    REQUIRE(std::string(bb::TARGET_POSITION) == "target_position");
    REQUIRE(std::string(bb::SELF_POSITION) == "self_position");
    REQUIRE(std::string(bb::MOVE_TARGET) == "move_target");
    REQUIRE(std::string(bb::IN_ATTACK_RANGE) == "in_attack_range");
    REQUIRE(std::string(bb::CAN_SEE_TARGET) == "can_see_target");
    REQUIRE(std::string(bb::IS_ALERTED) == "is_alerted");
}
