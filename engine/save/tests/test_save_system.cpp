#include <catch2/catch_test_macros.hpp>
#include <engine/save/save_system.hpp>
#include <engine/scene/world.hpp>

using namespace engine::save;
using namespace engine::scene;

TEST_CASE("SaveSystemConfig defaults", "[save][system]") {
    SaveSystemConfig config;

    REQUIRE(config.save_directory == "saves");
    REQUIRE(config.save_extension == ".sav");
    REQUIRE(config.quick_save_slot == "quicksave");
    REQUIRE(config.autosave_slot == "autosave");
    REQUIRE(config.autosave_interval == 300.0f);
    REQUIRE(config.max_autosaves == 3);
}

TEST_CASE("SaveResult defaults", "[save][system]") {
    SaveResult result;

    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.empty());
    REQUIRE(result.slot_name.empty());
    REQUIRE(result.save_time_ms == 0.0f);
}

TEST_CASE("LoadResult defaults", "[save][system]") {
    LoadResult result;

    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.empty());
    REQUIRE(result.slot_name.empty());
    REQUIRE(result.load_time_ms == 0.0f);
    REQUIRE(result.entities_loaded == 0);
}

TEST_CASE("Saveable component defaults", "[save][saveable]") {
    Saveable saveable;

    REQUIRE(saveable.persistent_id == 0);
    REQUIRE(saveable.save_transform == true);
    REQUIRE(saveable.save_components == true);
    REQUIRE(saveable.destroy_on_load == true);
    REQUIRE(saveable.excluded_components.empty());
}

TEST_CASE("Saveable component custom values", "[save][saveable]") {
    Saveable saveable;
    saveable.persistent_id = 12345;
    saveable.save_transform = false;
    saveable.save_components = true;
    saveable.destroy_on_load = false;
    saveable.excluded_components.push_back("RuntimeDebug");
    saveable.excluded_components.push_back("CachedData");

    REQUIRE(saveable.persistent_id == 12345);
    REQUIRE_FALSE(saveable.save_transform);
    REQUIRE(saveable.save_components);
    REQUIRE_FALSE(saveable.destroy_on_load);
    REQUIRE(saveable.excluded_components.size() == 2);
}

TEST_CASE("Saveable ID generation", "[save][saveable]") {
    SECTION("Generated IDs are non-zero") {
        uint64_t id = Saveable::generate_id();
        REQUIRE(id != 0);
    }

    SECTION("Generated IDs are unique") {
        uint64_t id1 = Saveable::generate_id();
        uint64_t id2 = Saveable::generate_id();
        uint64_t id3 = Saveable::generate_id();

        REQUIRE(id1 != id2);
        REQUIRE(id2 != id3);
        REQUIRE(id1 != id3);
    }
}

TEST_CASE("SaveSystem initialization", "[save][system]") {
    SaveSystem system;
    SaveSystemConfig config;
    config.save_directory = "test_saves";

    SECTION("Initialize with config") {
        system.init(config);
        REQUIRE(system.get_config().save_directory == "test_saves");
        system.shutdown();
    }
}

TEST_CASE("SaveSystem autosave state", "[save][system]") {
    SaveSystem system;
    SaveSystemConfig config;
    system.init(config);

    SECTION("Autosave disabled by default") {
        REQUIRE_FALSE(system.is_autosave_enabled());
    }

    SECTION("Enable autosave") {
        system.enable_autosave(true);
        REQUIRE(system.is_autosave_enabled());
    }

    SECTION("Disable autosave") {
        system.enable_autosave(true);
        system.enable_autosave(false);
        REQUIRE_FALSE(system.is_autosave_enabled());
    }

    system.shutdown();
}

TEST_CASE("SaveSystem progress tracking", "[save][system]") {
    SaveSystem system;
    SaveSystemConfig config;
    system.init(config);

    SECTION("Initial state") {
        REQUIRE(system.get_save_progress() == 0.0f);
        REQUIRE(system.get_load_progress() == 0.0f);
        REQUIRE_FALSE(system.is_saving());
        REQUIRE_FALSE(system.is_loading());
    }

    system.shutdown();
}

TEST_CASE("SaveSystem save path generation", "[save][system]") {
    SaveSystem system;
    SaveSystemConfig config;
    config.save_directory = "saves";
    config.save_extension = ".sav";
    system.init(config);

    SECTION("Get save path") {
        std::string path = system.get_save_path("slot1");
        REQUIRE(path.find("slot1") != std::string::npos);
        REQUIRE(path.find(".sav") != std::string::npos);
    }

    system.shutdown();
}

// Custom test handler for testing handler system
class TestSaveHandler : public ISaveHandler {
public:
    std::string get_type_name() const override { return "TestHandler"; }

    void on_save(SaveGame& save, World& /*world*/) override {
        save.set_value("handler_saved", true);
        save_called = true;
    }

    void on_pre_load(const SaveGame& /*save*/, World& /*world*/) override {
        pre_load_called = true;
    }

    void on_post_load(const SaveGame& /*save*/, World& /*world*/) override {
        post_load_called = true;
    }

    bool save_called = false;
    bool pre_load_called = false;
    bool post_load_called = false;
};

TEST_CASE("SaveSystem handler registration", "[save][system][handler]") {
    SaveSystem system;
    SaveSystemConfig config;
    system.init(config);

    SECTION("Register handler") {
        auto handler = std::make_unique<TestSaveHandler>();
        system.register_handler(std::move(handler));
        // Handler is registered (no public way to verify, but no crash)
    }

    SECTION("Unregister handler") {
        auto handler = std::make_unique<TestSaveHandler>();
        system.register_handler(std::move(handler));
        system.unregister_handler("TestHandler");
        // Handler is unregistered (no crash)
    }

    system.shutdown();
}

TEST_CASE("SaveSystem migration registration", "[save][system][migration]") {
    SaveSystem system;
    SaveSystemConfig config;
    system.init(config);

    SECTION("Register migration") {
        int migration_calls = 0;
        system.register_migration(1, [&](SaveGame& /*save*/, uint32_t from) {
            migration_calls++;
            REQUIRE(from == 1);
            return true;
        });

        system.register_migration(2, [&](SaveGame& /*save*/, uint32_t from) {
            migration_calls++;
            REQUIRE(from == 2);
            return true;
        });

        // Migrations registered (no public way to verify, but no crash)
    }

    SECTION("Clear migrations") {
        system.register_migration(1, [](SaveGame&, uint32_t) { return true; });
        system.clear_migrations();
        // Migrations cleared (no crash)
    }

    system.shutdown();
}
