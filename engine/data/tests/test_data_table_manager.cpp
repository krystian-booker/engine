#include <catch2/catch_test_macros.hpp>
#include <engine/data/data_table.hpp>

using namespace engine::data;

TEST_CASE("DataTableManager singleton", "[data][manager]") {
    auto& manager = data_tables();

    // Clear any existing tables
    manager.clear();

    SECTION("Instance access") {
        auto& manager2 = DataTableManager::instance();
        REQUIRE(&manager == &manager2);
    }

    SECTION("Initial state") {
        REQUIRE(manager.table_count() == 0);
        REQUIRE(manager.get_table_names().empty());
    }
}

TEST_CASE("DataTableManager table management", "[data][manager]") {
    auto& manager = data_tables();
    manager.clear();

    SECTION("Has table check") {
        REQUIRE_FALSE(manager.has("items"));
    }

    SECTION("Get non-existent table returns null") {
        REQUIRE(manager.get("nonexistent") == nullptr);
    }

    SECTION("Table count") {
        REQUIRE(manager.table_count() == 0);
    }

    SECTION("Get table names empty") {
        auto names = manager.get_table_names();
        REQUIRE(names.empty());
    }

    SECTION("Unload non-existent table is safe") {
        REQUIRE_NOTHROW(manager.unload("nonexistent"));
    }

    manager.clear();
}

TEST_CASE("DataTableManager hot reload settings", "[data][manager]") {
    auto& manager = data_tables();

    SECTION("Enable hot reload") {
        manager.enable_hot_reload(true);
        REQUIRE(manager.is_hot_reload_enabled());

        manager.enable_hot_reload(false);
        REQUIRE_FALSE(manager.is_hot_reload_enabled());
    }

    SECTION("Poll changes with no tables is safe") {
        REQUIRE_NOTHROW(manager.poll_changes());
    }

    SECTION("Reload all with no tables is safe") {
        REQUIRE_NOTHROW(manager.reload_all());
    }
}
