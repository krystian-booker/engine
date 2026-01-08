#include <catch2/catch_test_macros.hpp>
#include <engine/plugin/plugin.hpp>
#include <engine/plugin/game_interface.hpp>

using namespace engine::plugin;

// ============================================================================
// Version Constants Tests
// ============================================================================

TEST_CASE("Plugin version constants", "[plugin][version]") {
    REQUIRE(PLUGIN_VERSION_MAJOR == 1);
    REQUIRE(PLUGIN_VERSION_MINOR == 0);
}

TEST_CASE("Engine version constants", "[plugin][version]") {
    REQUIRE(ENGINE_VERSION_MAJOR == 1);
    REQUIRE(ENGINE_VERSION_MINOR == 0);
    REQUIRE(ENGINE_VERSION_PATCH == 0);

    // Check version encoding
    uint32_t expected = (1 << 16) | (0 << 8) | 0;
    REQUIRE(ENGINE_VERSION == expected);
}

// ============================================================================
// PluginInfo Tests
// ============================================================================

TEST_CASE("PluginInfo structure", "[plugin][info]") {
    PluginInfo info;
    info.name = "Test Game";
    info.version = "1.0.0";
    info.engine_version = ENGINE_VERSION;

    REQUIRE(std::string(info.name) == "Test Game");
    REQUIRE(std::string(info.version) == "1.0.0");
    REQUIRE(info.engine_version == ENGINE_VERSION);
}

TEST_CASE("PluginInfo version compatibility", "[plugin][info]") {
    PluginInfo info;
    info.name = "Compatible Game";
    info.version = "0.1.0";
    info.engine_version = ENGINE_VERSION;

    // Same engine version is compatible
    REQUIRE(info.engine_version == ENGINE_VERSION);
}

// ============================================================================
// GameContext Tests
// ============================================================================

TEST_CASE("GameContext defaults", "[plugin][context]") {
    GameContext ctx{};

    REQUIRE(ctx.world == nullptr);
    REQUIRE(ctx.scheduler == nullptr);
    REQUIRE(ctx.renderer == nullptr);
    REQUIRE(ctx.ui_context == nullptr);
    REQUIRE(ctx.app == nullptr);
    REQUIRE(ctx.project_path == nullptr);
}

TEST_CASE("GameContext with project path", "[plugin][context]") {
    GameContext ctx{};
    ctx.project_path = "/path/to/project";

    REQUIRE(std::string(ctx.project_path) == "/path/to/project");
}

// ============================================================================
// Export Name Constants Tests
// ============================================================================

TEST_CASE("Export name constants", "[plugin][exports]") {
    REQUIRE(std::string(EXPORT_GET_INFO) == "game_get_info");
    REQUIRE(std::string(EXPORT_INIT) == "game_init");
    REQUIRE(std::string(EXPORT_REGISTER_SYSTEMS) == "game_register_systems");
    REQUIRE(std::string(EXPORT_REGISTER_COMPONENTS) == "game_register_components");
    REQUIRE(std::string(EXPORT_PRE_RELOAD) == "game_pre_reload");
    REQUIRE(std::string(EXPORT_POST_RELOAD) == "game_post_reload");
    REQUIRE(std::string(EXPORT_SHUTDOWN) == "game_shutdown");
}
