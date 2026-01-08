#include <catch2/catch_test_macros.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/components.hpp>

using namespace engine::scene;

// Test components
struct TestPosition {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct TestVelocity {
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
};

struct TestHealth {
    int current = 100;
    int max = 100;
};

TEST_CASE("World construction", "[scene][world]") {
    World world;

    SECTION("Initially empty") {
        REQUIRE(world.empty());
        REQUIRE(world.size() == 0);
    }

    SECTION("Default scene name") {
        REQUIRE(world.get_scene_name() == "Untitled");
    }
}

TEST_CASE("World entity creation", "[scene][world]") {
    World world;

    SECTION("Create entity") {
        Entity e = world.create();
        REQUIRE(world.valid(e));
        REQUIRE(world.size() == 1);
    }

    SECTION("Create named entity") {
        Entity e = world.create("Player");
        REQUIRE(world.valid(e));

        // Find by name
        Entity found = world.find_by_name("Player");
        REQUIRE(found == e);
    }

    SECTION("Create multiple entities") {
        Entity e1 = world.create();
        Entity e2 = world.create();
        Entity e3 = world.create();

        REQUIRE(world.size() == 3);
        REQUIRE(e1 != e2);
        REQUIRE(e2 != e3);
    }
}

TEST_CASE("World entity destruction", "[scene][world]") {
    World world;

    SECTION("Destroy entity") {
        Entity e = world.create();
        REQUIRE(world.valid(e));

        world.destroy(e);
        REQUIRE_FALSE(world.valid(e));
        REQUIRE(world.size() == 0);
    }

    SECTION("Destroy with components") {
        Entity e = world.create();
        world.emplace<TestPosition>(e, 1.0f, 2.0f, 3.0f);
        world.emplace<TestHealth>(e);

        world.destroy(e);
        REQUIRE_FALSE(world.valid(e));
    }
}

TEST_CASE("World component management", "[scene][world]") {
    World world;
    Entity e = world.create();

    SECTION("Emplace component") {
        auto& pos = world.emplace<TestPosition>(e, 1.0f, 2.0f, 3.0f);
        REQUIRE(pos.x == 1.0f);
        REQUIRE(pos.y == 2.0f);
        REQUIRE(pos.z == 3.0f);
    }

    SECTION("Get component") {
        world.emplace<TestPosition>(e, 5.0f, 10.0f, 15.0f);

        auto& pos = world.get<TestPosition>(e);
        REQUIRE(pos.x == 5.0f);
        REQUIRE(pos.y == 10.0f);
        REQUIRE(pos.z == 15.0f);
    }

    SECTION("Const get") {
        world.emplace<TestPosition>(e, 1.0f, 2.0f, 3.0f);

        const World& const_world = world;
        const auto& pos = const_world.get<TestPosition>(e);
        REQUIRE(pos.x == 1.0f);
    }

    SECTION("Try get existing") {
        world.emplace<TestPosition>(e);

        auto* pos = world.try_get<TestPosition>(e);
        REQUIRE(pos != nullptr);
    }

    SECTION("Try get non-existing") {
        auto* pos = world.try_get<TestPosition>(e);
        REQUIRE(pos == nullptr);
    }

    SECTION("Has component") {
        REQUIRE_FALSE(world.has<TestPosition>(e));

        world.emplace<TestPosition>(e);
        REQUIRE(world.has<TestPosition>(e));
    }

    SECTION("Has all components") {
        world.emplace<TestPosition>(e);
        world.emplace<TestVelocity>(e);

        REQUIRE(world.has_all<TestPosition, TestVelocity>(e));
        REQUIRE_FALSE(world.has_all<TestPosition, TestHealth>(e));
    }

    SECTION("Has any component") {
        world.emplace<TestPosition>(e);

        REQUIRE(world.has_any<TestPosition, TestHealth>(e));
        REQUIRE_FALSE(world.has_any<TestVelocity, TestHealth>(e));
    }

    SECTION("Remove component") {
        world.emplace<TestPosition>(e);
        REQUIRE(world.has<TestPosition>(e));

        world.remove<TestPosition>(e);
        REQUIRE_FALSE(world.has<TestPosition>(e));
    }

    SECTION("Emplace or replace") {
        world.emplace<TestPosition>(e, 1.0f, 2.0f, 3.0f);
        world.emplace_or_replace<TestPosition>(e, 10.0f, 20.0f, 30.0f);

        auto& pos = world.get<TestPosition>(e);
        REQUIRE(pos.x == 10.0f);
        REQUIRE(pos.y == 20.0f);
    }
}

TEST_CASE("World view creation", "[scene][world]") {
    World world;

    // Create entities with different component combinations
    Entity e1 = world.create();
    world.emplace<TestPosition>(e1, 1.0f, 0.0f, 0.0f);
    world.emplace<TestVelocity>(e1);

    Entity e2 = world.create();
    world.emplace<TestPosition>(e2, 2.0f, 0.0f, 0.0f);

    Entity e3 = world.create();
    world.emplace<TestPosition>(e3, 3.0f, 0.0f, 0.0f);
    world.emplace<TestVelocity>(e3);
    world.emplace<TestHealth>(e3);

    SECTION("View single component") {
        auto view = world.view<TestPosition>();
        int count = 0;
        for (auto entity : view) {
            (void)entity;
            count++;
        }
        REQUIRE(count == 3);
    }

    SECTION("View multiple components") {
        auto view = world.view<TestPosition, TestVelocity>();
        int count = 0;
        for (auto entity : view) {
            (void)entity;
            count++;
        }
        REQUIRE(count == 2); // e1 and e3
    }

    SECTION("View iteration with get") {
        auto view = world.view<TestPosition>();
        float sum = 0.0f;
        for (auto entity : view) {
            auto& pos = view.get<TestPosition>(entity);
            sum += pos.x;
        }
        REQUIRE(sum == 6.0f); // 1 + 2 + 3
    }

    SECTION("Const view") {
        const World& const_world = world;
        auto view = const_world.view<TestPosition>();
        int count = 0;
        for (auto entity : view) {
            (void)entity;
            count++;
        }
        REQUIRE(count == 3);
    }
}

TEST_CASE("World clear", "[scene][world]") {
    World world;

    world.create("Entity1");
    world.create("Entity2");
    world.create("Entity3");

    REQUIRE(world.size() == 3);

    world.clear();

    REQUIRE(world.empty());
    REQUIRE(world.size() == 0);
}

TEST_CASE("World find by name", "[scene][world]") {
    World world;

    Entity player = world.create("Player");
    Entity enemy = world.create("Enemy");
    world.create(); // Unnamed entity

    SECTION("Find existing") {
        REQUIRE(world.find_by_name("Player") == player);
        REQUIRE(world.find_by_name("Enemy") == enemy);
    }

    SECTION("Find non-existing") {
        Entity result = world.find_by_name("NonExistent");
        REQUIRE_FALSE(world.valid(result));
    }
}

TEST_CASE("World scene metadata", "[scene][world]") {
    World world;

    SECTION("Set scene name") {
        world.set_scene_name("TestLevel");
        REQUIRE(world.get_scene_name() == "TestLevel");
    }

    SECTION("Scene metadata") {
        auto& metadata = world.get_scene_metadata();
        metadata["author"] = "Test";
        metadata["version"] = "1.0";

        const auto& const_metadata = world.get_scene_metadata();
        REQUIRE(const_metadata.at("author") == "Test");
        REQUIRE(const_metadata.at("version") == "1.0");
    }
}

TEST_CASE("World registry access", "[scene][world]") {
    World world;

    SECTION("Non-const registry") {
        auto& registry = world.registry();
        auto entity = registry.create();
        REQUIRE(registry.valid(entity));
    }

    SECTION("Const registry") {
        world.create();
        const World& const_world = world;
        const auto& registry = const_world.registry();
        REQUIRE(registry.storage<entt::entity>()->size() == 1);
    }
}
