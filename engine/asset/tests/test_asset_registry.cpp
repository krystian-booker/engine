#include <catch2/catch_test_macros.hpp>
#include <engine/asset/asset_registry.hpp>

using namespace engine::asset;
using namespace engine::core;

// Test helper - creates a fresh registry for each test
class TestRegistry {
public:
    AssetRegistry registry;
};

TEST_CASE("AssetMetadata defaults", "[asset][registry]") {
    AssetMetadata meta;

    REQUIRE(meta.id.is_null());
    REQUIRE(meta.type == AssetType::Unknown);
    REQUIRE(meta.path.empty());
    REQUIRE(meta.last_modified == 0);
    REQUIRE(meta.is_loaded == false);
}

TEST_CASE("AssetMetadata with values", "[asset][registry]") {
    AssetMetadata meta;
    meta.id = UUID::from_u64(0x1234, 0x5678);
    meta.type = AssetType::Mesh;
    meta.path = "assets/model.gltf";
    meta.last_modified = 1234567890;
    meta.is_loaded = true;

    REQUIRE_FALSE(meta.id.is_null());
    REQUIRE(meta.type == AssetType::Mesh);
    REQUIRE(meta.path == "assets/model.gltf");
    REQUIRE(meta.last_modified == 1234567890);
    REQUIRE(meta.is_loaded == true);
}

TEST_CASE("AssetRegistry register_asset generates UUID", "[asset][registry]") {
    TestRegistry t;

    UUID id = t.registry.register_asset("assets/test.gltf", AssetType::Mesh);

    REQUIRE_FALSE(id.is_null());
    REQUIRE(t.registry.count() == 1);
}

TEST_CASE("AssetRegistry register_asset returns same UUID for same path", "[asset][registry]") {
    TestRegistry t;

    UUID id1 = t.registry.register_asset("assets/test.gltf", AssetType::Mesh);
    UUID id2 = t.registry.register_asset("assets/test.gltf", AssetType::Mesh);

    REQUIRE(id1 == id2);
    REQUIRE(t.registry.count() == 1);
}

TEST_CASE("AssetRegistry register_asset with explicit UUID", "[asset][registry]") {
    TestRegistry t;

    UUID explicit_id = UUID::from_u64(0xDEAD, 0xBEEF);
    t.registry.register_asset(explicit_id, "assets/explicit.gltf", AssetType::Mesh);

    auto found = t.registry.find_by_path("assets/explicit.gltf");
    REQUIRE(found.has_value());
    REQUIRE(*found == explicit_id);
}

TEST_CASE("AssetRegistry find_by_path", "[asset][registry]") {
    TestRegistry t;

    UUID id = t.registry.register_asset("assets/player.gltf", AssetType::Mesh);

    SECTION("Found") {
        auto found = t.registry.find_by_path("assets/player.gltf");
        REQUIRE(found.has_value());
        REQUIRE(*found == id);
    }

    SECTION("Not found") {
        auto found = t.registry.find_by_path("assets/nonexistent.gltf");
        REQUIRE_FALSE(found.has_value());
    }
}

TEST_CASE("AssetRegistry find_by_id", "[asset][registry]") {
    TestRegistry t;

    UUID id = t.registry.register_asset("assets/texture.png", AssetType::Texture);

    SECTION("Found") {
        auto found = t.registry.find_by_id(id);
        REQUIRE(found.has_value());
        REQUIRE(found->id == id);
        REQUIRE(found->type == AssetType::Texture);
        REQUIRE(found->path == "assets/texture.png");
    }

    SECTION("Not found") {
        auto found = t.registry.find_by_id(UUID::from_u64(0x1111, 0x2222));
        REQUIRE_FALSE(found.has_value());
    }
}

TEST_CASE("AssetRegistry get_path", "[asset][registry]") {
    TestRegistry t;

    UUID id = t.registry.register_asset("assets/shader.glsl", AssetType::Shader);

    SECTION("Found") {
        auto path = t.registry.get_path(id);
        REQUIRE(path.has_value());
        REQUIRE(*path == "assets/shader.glsl");
    }

    SECTION("Not found") {
        auto path = t.registry.get_path(UUID::from_u64(0x9999, 0x8888));
        REQUIRE_FALSE(path.has_value());
    }
}

TEST_CASE("AssetRegistry update_path", "[asset][registry]") {
    TestRegistry t;

    UUID id = t.registry.register_asset("assets/old_path.gltf", AssetType::Mesh);

    SECTION("Successful update") {
        bool success = t.registry.update_path(id, "assets/new_path.gltf");
        REQUIRE(success);

        auto path = t.registry.get_path(id);
        REQUIRE(path.has_value());
        REQUIRE(*path == "assets/new_path.gltf");

        // Old path should no longer resolve
        auto old_lookup = t.registry.find_by_path("assets/old_path.gltf");
        REQUIRE_FALSE(old_lookup.has_value());

        // New path should resolve
        auto new_lookup = t.registry.find_by_path("assets/new_path.gltf");
        REQUIRE(new_lookup.has_value());
        REQUIRE(*new_lookup == id);
    }

    SECTION("Update nonexistent UUID") {
        bool success = t.registry.update_path(UUID::from_u64(0x1111, 0x2222), "assets/new.gltf");
        REQUIRE_FALSE(success);
    }
}

TEST_CASE("AssetRegistry set_loaded", "[asset][registry]") {
    TestRegistry t;

    UUID id = t.registry.register_asset("assets/test.gltf", AssetType::Mesh);

    auto meta = t.registry.find_by_id(id);
    REQUIRE(meta.has_value());
    REQUIRE(meta->is_loaded == false);

    t.registry.set_loaded(id, true);

    meta = t.registry.find_by_id(id);
    REQUIRE(meta.has_value());
    REQUIRE(meta->is_loaded == true);

    t.registry.set_loaded(id, false);

    meta = t.registry.find_by_id(id);
    REQUIRE(meta.has_value());
    REQUIRE(meta->is_loaded == false);
}

TEST_CASE("AssetRegistry set_last_modified", "[asset][registry]") {
    TestRegistry t;

    UUID id = t.registry.register_asset("assets/test.gltf", AssetType::Mesh);

    t.registry.set_last_modified(id, 1234567890);

    auto meta = t.registry.find_by_id(id);
    REQUIRE(meta.has_value());
    REQUIRE(meta->last_modified == 1234567890);
}

TEST_CASE("AssetRegistry unregister by UUID", "[asset][registry]") {
    TestRegistry t;

    UUID id = t.registry.register_asset("assets/test.gltf", AssetType::Mesh);
    REQUIRE(t.registry.count() == 1);

    t.registry.unregister(id);

    REQUIRE(t.registry.count() == 0);
    REQUIRE_FALSE(t.registry.find_by_id(id).has_value());
    REQUIRE_FALSE(t.registry.find_by_path("assets/test.gltf").has_value());
}

TEST_CASE("AssetRegistry unregister by path", "[asset][registry]") {
    TestRegistry t;

    UUID id = t.registry.register_asset("assets/test.gltf", AssetType::Mesh);
    REQUIRE(t.registry.count() == 1);

    t.registry.unregister("assets/test.gltf");

    REQUIRE(t.registry.count() == 0);
    REQUIRE_FALSE(t.registry.find_by_id(id).has_value());
}

TEST_CASE("AssetRegistry get_all", "[asset][registry]") {
    TestRegistry t;

    t.registry.register_asset("assets/mesh.gltf", AssetType::Mesh);
    t.registry.register_asset("assets/texture.png", AssetType::Texture);
    t.registry.register_asset("assets/shader.glsl", AssetType::Shader);

    auto all = t.registry.get_all();
    REQUIRE(all.size() == 3);
}

TEST_CASE("AssetRegistry get_by_type", "[asset][registry]") {
    TestRegistry t;

    t.registry.register_asset("assets/mesh1.gltf", AssetType::Mesh);
    t.registry.register_asset("assets/mesh2.gltf", AssetType::Mesh);
    t.registry.register_asset("assets/texture.png", AssetType::Texture);
    t.registry.register_asset("assets/shader.glsl", AssetType::Shader);

    auto meshes = t.registry.get_by_type(AssetType::Mesh);
    REQUIRE(meshes.size() == 2);

    auto textures = t.registry.get_by_type(AssetType::Texture);
    REQUIRE(textures.size() == 1);

    auto materials = t.registry.get_by_type(AssetType::Material);
    REQUIRE(materials.size() == 0);
}

TEST_CASE("AssetRegistry count", "[asset][registry]") {
    TestRegistry t;

    REQUIRE(t.registry.count() == 0);

    t.registry.register_asset("assets/a.gltf", AssetType::Mesh);
    REQUIRE(t.registry.count() == 1);

    t.registry.register_asset("assets/b.gltf", AssetType::Mesh);
    REQUIRE(t.registry.count() == 2);
}

TEST_CASE("AssetRegistry count_by_type", "[asset][registry]") {
    TestRegistry t;

    t.registry.register_asset("assets/mesh1.gltf", AssetType::Mesh);
    t.registry.register_asset("assets/mesh2.gltf", AssetType::Mesh);
    t.registry.register_asset("assets/texture.png", AssetType::Texture);

    REQUIRE(t.registry.count_by_type(AssetType::Mesh) == 2);
    REQUIRE(t.registry.count_by_type(AssetType::Texture) == 1);
    REQUIRE(t.registry.count_by_type(AssetType::Shader) == 0);
}

TEST_CASE("AssetRegistry clear", "[asset][registry]") {
    TestRegistry t;

    t.registry.register_asset("assets/a.gltf", AssetType::Mesh);
    t.registry.register_asset("assets/b.png", AssetType::Texture);
    t.registry.register_asset("assets/c.glsl", AssetType::Shader);

    REQUIRE(t.registry.count() == 3);

    t.registry.clear();

    REQUIRE(t.registry.count() == 0);
    REQUIRE(t.registry.get_all().empty());
}
