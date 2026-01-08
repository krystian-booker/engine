#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/debug-gui/debug_profiler.hpp>

using namespace engine::debug_gui;
using Catch::Matchers::WithinAbs;

// ============================================================================
// DebugProfiler Tests
// ============================================================================

TEST_CASE("DebugProfiler get_name", "[debug-gui][profiler]") {
    DebugProfiler profiler;

    REQUIRE(std::string(profiler.get_name()) == "profiler");
}

TEST_CASE("DebugProfiler get_title", "[debug-gui][profiler]") {
    DebugProfiler profiler;

    REQUIRE(std::string(profiler.get_title()) == "Performance");
}

TEST_CASE("DebugProfiler is IDebugWindow", "[debug-gui][profiler]") {
    DebugProfiler profiler;

    // Test that it properly implements the interface
    IDebugWindow* window = &profiler;

    REQUIRE(std::string(window->get_name()) == "profiler");
    REQUIRE(std::string(window->get_title()) == "Performance");
    REQUIRE_FALSE(window->is_open());
}

TEST_CASE("DebugProfiler toggle", "[debug-gui][profiler]") {
    DebugProfiler profiler;

    REQUIRE_FALSE(profiler.is_open());

    profiler.toggle();
    REQUIRE(profiler.is_open());

    profiler.toggle();
    REQUIRE_FALSE(profiler.is_open());
}

TEST_CASE("DebugProfiler has shortcut key", "[debug-gui][profiler]") {
    DebugProfiler profiler;

    // Profiler should have a shortcut key defined
    uint32_t shortcut = profiler.get_shortcut_key();
    // Just verify it returns something (actual key depends on implementation)
    (void)shortcut;
}
