#include <catch2/catch_test_macros.hpp>
#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>

using namespace engine::scene;

TEST_CASE("Entity type alias", "[scene][entity]") {
    SECTION("Entity is entt::entity") {
        static_assert(std::is_same_v<Entity, entt::entity>, "Entity should be entt::entity");
    }

    SECTION("NullEntity is entt::null") {
        REQUIRE((uint32_t)NullEntity == (uint32_t)entt::null);
    }
}

TEST_CASE("EntityInfo component", "[scene][entity]") {
    EntityInfo info;

    SECTION("Default values") {
        REQUIRE(info.name.empty());
        REQUIRE(info.uuid == 0);
        REQUIRE(info.enabled == true);
    }

    SECTION("Custom values") {
        info.name = "TestEntity";
        info.uuid = 12345;
        info.enabled = false;

        REQUIRE(info.name == "TestEntity");
        REQUIRE(info.uuid == 12345);
        REQUIRE(info.enabled == false);
    }
}

TEST_CASE("Entity with EntityInfo in World", "[scene][entity]") {
    World world;

    SECTION("Create entity with info") {
        Entity e = world.create();
        auto& info = world.get<EntityInfo>(e);
        info.name = "Player";
        info.uuid = 1;

        auto& retrieved = world.get<EntityInfo>(e);
        REQUIRE(retrieved.name == "Player");
        REQUIRE(retrieved.uuid == 1);
    }

    SECTION("Named entity has EntityInfo") {
        Entity e = world.create("MyEntity");

        REQUIRE(world.has<EntityInfo>(e));
        auto& info = world.get<EntityInfo>(e);
        REQUIRE(info.name == "MyEntity");
    }

    SECTION("Disable entity via EntityInfo") {
        Entity e = world.create("DisabledEntity");
        auto& info = world.get<EntityInfo>(e);
        info.enabled = false;

        REQUIRE(world.get<EntityInfo>(e).enabled == false);
    }
}

TEST_CASE("Entity validity", "[scene][entity]") {
    World world;

    SECTION("NullEntity is invalid") {
        REQUIRE_FALSE(world.valid(NullEntity));
    }

    SECTION("Created entity is valid") {
        Entity e = world.create();
        REQUIRE(world.valid(e));
    }

    SECTION("Destroyed entity is invalid") {
        Entity e = world.create();
        world.destroy(e);
        REQUIRE_FALSE(world.valid(e));
    }
}

TEST_CASE("Entity comparison", "[scene][entity]") {
    World world;

    Entity e1 = world.create();
    Entity e2 = world.create();
    Entity e1_copy = e1;

    SECTION("Different entities are not equal") {
        REQUIRE(e1 != e2);
    }

    SECTION("Same entity is equal") {
        REQUIRE(e1 == e1_copy);
    }

    SECTION("NullEntity equals NullEntity") {
        Entity null1 = NullEntity;
        Entity null2 = NullEntity;
        REQUIRE((uint32_t)null1 == (uint32_t)null2);
    }
}
