#include <catch2/catch_test_macros.hpp>
#include <engine/asset/manager.hpp>

using namespace engine::asset;
using namespace engine::core;

TEST_CASE("AssetStatus enum", "[asset][manager]") {
    REQUIRE(static_cast<int>(AssetStatus::NotLoaded) == 0);
    REQUIRE(static_cast<int>(AssetStatus::Loading) == 1);
    REQUIRE(static_cast<int>(AssetStatus::Loaded) == 2);
    REQUIRE(static_cast<int>(AssetStatus::Failed) == 3);
}

// Note: Full AssetManager tests require renderer initialization.
// These tests cover the data structures and enum values.
// Integration tests would require proper renderer and file system setup.

TEST_CASE("AssetManager construction", "[asset][manager]") {
    // Just verify the manager can be constructed without a renderer
    AssetManager manager;

    // Without initialization, loaded count should be 0
    REQUIRE(manager.get_loaded_count() == 0);
}

TEST_CASE("AssetManager status queries before init", "[asset][manager]") {
    AssetManager manager;

    // Non-existent asset should return NotLoaded status
    REQUIRE(manager.get_status("nonexistent.gltf") == AssetStatus::NotLoaded);
    REQUIRE_FALSE(manager.is_loaded("nonexistent.gltf"));

    // UUID-based query should also work
    auto uuid = UUID::from_u64(0x1234, 0x5678);
    REQUIRE(manager.get_status(uuid) == AssetStatus::NotLoaded);
    REQUIRE_FALSE(manager.is_loaded(uuid));
}

TEST_CASE("AssetManager memory usage before init", "[asset][manager]") {
    AssetManager manager;

    // Without any loaded assets, memory usage should be 0
    REQUIRE(manager.get_memory_usage() == 0);
}

TEST_CASE("AssetManager extension detection", "[asset][manager]") {
    // Test the internal get_extension function by checking load behavior
    // The manager should accept various asset file extensions

    // Note: Since get_extension is private, we can only test this indirectly
    // by verifying the load functions accept valid paths (they'll fail without
    // a renderer but won't crash due to unknown extension)

    AssetManager manager;

    // These should not throw, even without initialization
    // They should return nullptr since manager is not initialized
    auto mesh = manager.load_mesh("test.gltf");
    REQUIRE(mesh == nullptr);

    auto texture = manager.load_texture("test.png");
    REQUIRE(texture == nullptr);
}

TEST_CASE("AssetManager hot reload toggle", "[asset][manager]") {
    AssetManager manager;

    // Should be able to toggle hot reload without crashing
    manager.enable_hot_reload(true);
    manager.enable_hot_reload(false);

    // Poll should be safe to call even without initialization
    manager.poll_hot_reload();
}

TEST_CASE("AssetManager reload callback", "[asset][manager]") {
    AssetManager manager;

    bool callback_set = false;
    manager.set_reload_callback([&callback_set](UUID /*id*/, const std::string& /*path*/) {
        callback_set = true;
    });

    // Callback is set but won't be called until actual reload happens
    REQUIRE_FALSE(callback_set);
}

TEST_CASE("AssetManager unload operations", "[asset][manager]") {
    AssetManager manager;

    // These should be safe to call even with no loaded assets
    manager.unload("nonexistent.gltf");
    manager.unload(UUID::from_u64(0x1234, 0x5678));
    manager.unload_unused();
    manager.unload_all();

    REQUIRE(manager.get_loaded_count() == 0);
}
