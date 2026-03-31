#include <catch2/catch_test_macros.hpp>
#include <engine/scene/scene_serializer.hpp>
#include <engine/scene/transform.hpp>

using namespace engine::scene;

TEST_CASE("SceneSerializer deserialize_entity reuses World EntityInfo", "[scene][serializer]") {
    World world;
    SceneSerializer serializer;

    const std::string json = R"({
        "uuid": 77,
        "name": "PrefabRoot",
        "enabled": true,
        "components": [
            {
                "type": "LocalTransform",
                "data": {
                    "position": [1.0, 2.0, 3.0]
                }
            }
        ]
    })";

    Entity entity = serializer.deserialize_entity(world, json);

    REQUIRE(entity != NullEntity);
    REQUIRE(world.valid(entity));
    REQUIRE(world.get<EntityInfo>(entity).name == "PrefabRoot");
    REQUIRE(world.get<EntityInfo>(entity).uuid != 0);
    REQUIRE(world.get<EntityInfo>(entity).uuid != 77);
    REQUIRE(world.has<LocalTransform>(entity));
    REQUIRE(world.has<WorldTransform>(entity));
}

TEST_CASE("SceneSerializer advances World UUID counter after deserialize", "[scene][serializer]") {
    World world;
    SceneSerializer serializer;

    const std::string json = R"({
        "name": "LoadedScene",
        "entities": [
            {
                "uuid": 41,
                "name": "Existing",
                "enabled": true,
                "components": []
            }
        ]
    })";

    REQUIRE(serializer.deserialize(world, json));

    Entity existing = world.find_by_name("Existing");
    REQUIRE(existing != NullEntity);
    REQUIRE(world.get<EntityInfo>(existing).uuid == 41);

    Entity created = world.create("FreshEntity");
    REQUIRE(world.get<EntityInfo>(created).uuid > 41);
}

TEST_CASE("SceneSerializer deserializes entity components from scenes", "[scene][serializer]") {
    World world;
    SceneSerializer serializer;

    const std::string json = R"({
        "name": "LoadedScene",
        "version": "1.0",
        "entities": [
            {
                "uuid": 12,
                "name": "Mover",
                "enabled": true,
                "parent_uuid": 0,
                "components": [
                    {
                        "type": "LocalTransform",
                        "data": {
                            "position": [4.0, 5.0, 6.0],
                            "rotation": [1.0, 0.0, 0.0, 0.0],
                            "scale": [1.0, 1.0, 1.0]
                        }
                    }
                ]
            }
        ]
    })";

    REQUIRE(serializer.deserialize(world, json));

    Entity mover = world.find_by_name("Mover");
    REQUIRE(mover != NullEntity);
    REQUIRE(world.has<LocalTransform>(mover));
    REQUIRE(world.has<WorldTransform>(mover));
    REQUIRE(world.get<LocalTransform>(mover).position == engine::core::Vec3{4.0f, 5.0f, 6.0f});
}
