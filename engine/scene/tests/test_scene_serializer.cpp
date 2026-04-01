#include <catch2/catch_test_macros.hpp>
#include <engine/scene/scene_serializer.hpp>
#include <engine/scene/transform.hpp>
#include <engine/reflect/reflect.hpp>
#include <nlohmann/json.hpp>

using namespace engine::scene;

namespace {

struct TestReferenceComponent {
    entt::entity target = entt::null;
    int weight = 0;

    TestReferenceComponent() = default;
    TestReferenceComponent(entt::entity target_entity, int new_weight)
        : target(target_entity), weight(new_weight) {}
};

struct TestCustomEncodedComponent {
    int hp = 0;

    TestCustomEncodedComponent() = default;
    explicit TestCustomEncodedComponent(int value) : hp(value) {}
};

void register_test_components() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    auto& type_registry = engine::reflect::TypeRegistry::instance();

    type_registry.register_component<TestReferenceComponent>(
        "TestReferenceComponent",
        engine::reflect::TypeMeta().set_display_name("Test Reference"));
    type_registry.register_property<TestReferenceComponent, &TestReferenceComponent::target>(
        "target",
        engine::reflect::PropertyMeta().set_entity_ref(true));
    type_registry.register_property<TestReferenceComponent, &TestReferenceComponent::weight>(
        "weight",
        engine::reflect::PropertyMeta());

    type_registry.register_component<TestCustomEncodedComponent>(
        "TestCustomEncodedComponent",
        engine::reflect::TypeMeta().set_display_name("Test Custom Encoded"));
    type_registry.register_property<TestCustomEncodedComponent, &TestCustomEncodedComponent::hp>(
        "hp",
        engine::reflect::PropertyMeta());
}

} // namespace

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

TEST_CASE("SceneSerializer replaces existing world state on scene deserialize", "[scene][serializer]") {
    World world;
    SceneSerializer serializer;

    Entity stale = world.create("StaleEntity");
    world.emplace<LocalTransform>(stale, engine::core::Vec3{9.0f, 9.0f, 9.0f});
    world.get_scene_metadata()["stale"] = "true";

    const std::string json = R"({
        "name": "FreshScene",
        "entities": [
            {
                "uuid": 12,
                "name": "FreshEntity",
                "enabled": true,
                "parent_uuid": 0,
                "components": []
            }
        ]
    })";

    REQUIRE(serializer.deserialize(world, json));

    REQUIRE(world.find_by_name("StaleEntity") == NullEntity);
    REQUIRE(world.find_by_name("FreshEntity") != NullEntity);
    REQUIRE(world.get_scene_name() == "FreshScene");
    REQUIRE(world.get_scene_metadata().find("stale") == world.get_scene_metadata().end());
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

TEST_CASE("SceneSerializer deserializes components for entities with zero UUID", "[scene][serializer][regression]") {
    World world;
    SceneSerializer serializer;

    const std::string json = R"({
        "name": "ZeroUuidScene",
        "entities": [
            {
                "uuid": 0,
                "name": "ZeroUuidEntity",
                "enabled": true,
                "parent_uuid": 0,
                "components": [
                    {
                        "type": "LocalTransform",
                        "data": {
                            "position": [8.0, 9.0, 10.0],
                            "rotation": [1.0, 0.0, 0.0, 0.0],
                            "scale": [1.0, 1.0, 1.0]
                        }
                    }
                ]
            }
        ]
    })";

    REQUIRE(serializer.deserialize(world, json));

    const Entity loaded = world.find_by_name("ZeroUuidEntity");
    REQUIRE(loaded != NullEntity);
    REQUIRE(world.has<LocalTransform>(loaded));
    REQUIRE(world.get<LocalTransform>(loaded).position == engine::core::Vec3{8.0f, 9.0f, 10.0f});
}

TEST_CASE("SceneSerializer round-trips reflected entity references", "[scene][serializer][reflection]") {
    register_test_components();

    World source_world;
    SceneSerializer serializer;

    Entity target = source_world.create("Target");
    Entity owner = source_world.create("Owner");
    source_world.emplace<TestReferenceComponent>(owner, target, 7);

    const std::string json = serializer.serialize(source_world);

    World loaded_world;
    REQUIRE(serializer.deserialize(loaded_world, json));

    Entity loaded_owner = loaded_world.find_by_name("Owner");
    Entity loaded_target = loaded_world.find_by_name("Target");

    REQUIRE(loaded_owner != NullEntity);
    REQUIRE(loaded_target != NullEntity);
    REQUIRE(loaded_world.has<TestReferenceComponent>(loaded_owner));
    REQUIRE(loaded_world.get<TestReferenceComponent>(loaded_owner).target == loaded_target);
    REQUIRE(loaded_world.get<TestReferenceComponent>(loaded_owner).weight == 7);
}

TEST_CASE("SceneSerializer deserialize_entity resolves internal entity references", "[scene][serializer][prefab]") {
    register_test_components();

    World source_world;
    SceneSerializer serializer;

    Entity root = source_world.create("Root");
    Entity child = source_world.create("Child");
    source_world.emplace<LocalTransform>(root, engine::core::Vec3{0.0f, 0.0f, 0.0f});
    source_world.emplace<LocalTransform>(child, engine::core::Vec3{1.0f, 0.0f, 0.0f});
    set_parent(source_world, child, root);
    source_world.emplace<TestReferenceComponent>(root, child, 99);

    const std::string json = serializer.serialize_entity(source_world, root, true);

    World instance_world;
    Entity new_root = serializer.deserialize_entity(instance_world, json);

    REQUIRE(new_root != NullEntity);
    REQUIRE(instance_world.has<TestReferenceComponent>(new_root));

    const auto& children = get_children(instance_world, new_root);
    REQUIRE(children.size() == 1);
    REQUIRE(instance_world.get<TestReferenceComponent>(new_root).target == children[0]);
    REQUIRE(instance_world.get<TestReferenceComponent>(new_root).weight == 99);
}

TEST_CASE("SceneSerializer uses registered custom component serializers during round-trip", "[scene][serializer][custom]") {
    SceneSerializer serializer;
    serializer.register_component<TestCustomEncodedComponent>(
        "TestCustomEncodedComponent",
        [](const void* component) {
            const auto& value = *static_cast<const TestCustomEncodedComponent*>(component);
            return std::string{"{\"encoded_hp\": "} + std::to_string(value.hp) + "}";
        },
        [](void* component, const std::string& json) {
            auto& value = *static_cast<TestCustomEncodedComponent*>(component);
            const auto colon = json.find(':');
            const auto end = json.find('}', colon == std::string::npos ? 0 : colon);
            value.hp = (colon != std::string::npos)
                ? std::stoi(json.substr(colon + 1, end == std::string::npos ? std::string::npos : end - colon - 1))
                : 0;
        }
    );

    World source_world;
    Entity entity = source_world.create("Custom");
    source_world.emplace<TestCustomEncodedComponent>(entity, 123);

    const std::string json = serializer.serialize_entity(source_world, entity, false);
    REQUIRE(json.find("\"encoded_hp\"") != std::string::npos);

    World loaded_world;
    Entity loaded = serializer.deserialize_entity(loaded_world, json);

    REQUIRE(loaded != NullEntity);
    REQUIRE(loaded_world.has<TestCustomEncodedComponent>(loaded));
    REQUIRE(loaded_world.get<TestCustomEncodedComponent>(loaded).hp == 123);
}

TEST_CASE("SceneSerializer preserves child order across scene round-trip", "[scene][serializer][hierarchy]") {
    World source_world;
    SceneSerializer serializer;

    const Entity parent = source_world.create("Parent");
    const Entity child_a = source_world.create("ChildA");
    const Entity child_b = source_world.create("ChildB");

    source_world.emplace<LocalTransform>(parent);
    source_world.emplace<LocalTransform>(child_a);
    source_world.emplace<LocalTransform>(child_b);

    set_parent(source_world, child_a, parent, NullEntity); // append
    set_parent(source_world, child_b, parent, NullEntity); // append

    const std::string json = serializer.serialize(source_world);

    World loaded_world;
    REQUIRE(serializer.deserialize(loaded_world, json));

    const Entity loaded_parent = loaded_world.find_by_name("Parent");
    REQUIRE(loaded_parent != NullEntity);

    const auto& loaded_children = get_children(loaded_world, loaded_parent);
    REQUIRE(loaded_children.size() == 2);
    REQUIRE(loaded_world.get<EntityInfo>(loaded_children[0]).name == "ChildA");
    REQUIRE(loaded_world.get<EntityInfo>(loaded_children[1]).name == "ChildB");
}

TEST_CASE("SceneSerializer round-trips escaped scene and entity names", "[scene][serializer][strings]") {
    World source_world;
    SceneSerializer serializer;

    source_world.set_scene_name("Scene \"Main\" \\ Build");
    Entity entity = source_world.create("Hero \"Alpha\" \\ Path");

    const std::string json = serializer.serialize(source_world);
    REQUIRE_NOTHROW([&json] {
        const auto parsed = nlohmann::json::parse(json);
        (void)parsed;
    }());

    World loaded_world;
    REQUIRE(serializer.deserialize(loaded_world, json));
    REQUIRE(loaded_world.get_scene_name() == "Scene \"Main\" \\ Build");

    const Entity loaded = loaded_world.find_by_name("Hero \"Alpha\" \\ Path");
    REQUIRE(loaded != NullEntity);
    REQUIRE(loaded_world.valid(loaded));
    REQUIRE(loaded_world.get<EntityInfo>(loaded).name == "Hero \"Alpha\" \\ Path");
}

TEST_CASE("SceneSerializer round-trips scene metadata", "[scene][serializer][metadata]") {
    World source_world;
    SceneSerializer serializer;

    source_world.set_scene_name("MetaScene");
    source_world.get_scene_metadata()["author"] = "engine-team";
    source_world.get_scene_metadata()["build"] = "debug";
    source_world.get_scene_metadata()["path"] = "assets\\scene \"quoted\".json";
    source_world.create("AnyEntity");

    const std::string json = serializer.serialize(source_world);

    World loaded_world;
    REQUIRE(serializer.deserialize(loaded_world, json));

    const auto& metadata = loaded_world.get_scene_metadata();
    REQUIRE(metadata.size() == 3);
    REQUIRE(metadata.at("author") == "engine-team");
    REQUIRE(metadata.at("build") == "debug");
    REQUIRE(metadata.at("path") == "assets\\scene \"quoted\".json");
}

TEST_CASE("SceneSerializer round-trips escaped asset paths", "[scene][serializer][assets]") {
    SceneSerializer serializer;
    serializer.set_asset_resolver([](uint32_t id) -> AssetReference {
        return AssetReference{"assets\\hero \"quoted\".mesh", id == 7 ? "mesh" : "material"};
    });
    serializer.set_asset_loader([](const AssetReference& ref) -> uint32_t {
        if (ref.path == "assets\\hero \"quoted\".mesh") {
            return 77;
        }
        return UINT32_MAX;
    });

    World source_world;
    Entity entity = source_world.create("MeshEntity");
    auto& renderer = source_world.emplace<MeshRenderer>(entity);
    renderer.mesh.id = 7;

    const std::string json = serializer.serialize_entity(source_world, entity, false);
    REQUIRE_NOTHROW([&json] {
        const auto parsed = nlohmann::json::parse(json);
        (void)parsed;
    }());

    World loaded_world;
    const Entity loaded = serializer.deserialize_entity(loaded_world, json);
    REQUIRE(loaded != NullEntity);
    REQUIRE(loaded_world.has<MeshRenderer>(loaded));
    REQUIRE(loaded_world.get<MeshRenderer>(loaded).mesh.id == 77);
}
