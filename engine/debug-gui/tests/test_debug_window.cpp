#include <catch2/catch_test_macros.hpp>
#include <engine/debug-gui/debug_window.hpp>

using namespace engine::debug_gui;

// ============================================================================
// Test implementation of IDebugWindow
// ============================================================================

class TestDebugWindow : public IDebugWindow {
public:
    const char* get_name() const override { return "test_window"; }
    const char* get_title() const override { return "Test Window"; }
    void draw() override { m_draw_count++; }
    uint32_t get_shortcut_key() const override { return 0x54; }  // 'T'

    int get_draw_count() const { return m_draw_count; }

    bool open_called = false;
    bool close_called = false;

protected:
    void on_open() override { open_called = true; }
    void on_close() override { close_called = true; }

private:
    int m_draw_count = 0;
};

// ============================================================================
// IDebugWindow Tests
// ============================================================================

TEST_CASE("IDebugWindow default state", "[debug-gui][window]") {
    TestDebugWindow window;

    REQUIRE_FALSE(window.is_open());
    REQUIRE(std::string(window.get_name()) == "test_window");
    REQUIRE(std::string(window.get_title()) == "Test Window");
}

TEST_CASE("IDebugWindow set_open", "[debug-gui][window]") {
    TestDebugWindow window;

    REQUIRE_FALSE(window.is_open());

    window.set_open(true);
    REQUIRE(window.is_open());
    REQUIRE(window.open_called);
    REQUIRE_FALSE(window.close_called);

    window.set_open(false);
    REQUIRE_FALSE(window.is_open());
    REQUIRE(window.close_called);
}

TEST_CASE("IDebugWindow set_open same value", "[debug-gui][window]") {
    TestDebugWindow window;

    window.set_open(true);
    window.open_called = false;  // Reset

    // Setting to same value should not trigger callback
    window.set_open(true);
    REQUIRE_FALSE(window.open_called);
}

TEST_CASE("IDebugWindow toggle", "[debug-gui][window]") {
    TestDebugWindow window;

    REQUIRE_FALSE(window.is_open());

    window.toggle();
    REQUIRE(window.is_open());

    window.toggle();
    REQUIRE_FALSE(window.is_open());

    window.toggle();
    REQUIRE(window.is_open());
}

TEST_CASE("IDebugWindow get_shortcut_key", "[debug-gui][window]") {
    TestDebugWindow window;

    REQUIRE(window.get_shortcut_key() == 0x54);  // 'T'
}

TEST_CASE("IDebugWindow draw called", "[debug-gui][window]") {
    TestDebugWindow window;

    REQUIRE(window.get_draw_count() == 0);

    window.draw();
    REQUIRE(window.get_draw_count() == 1);

    window.draw();
    window.draw();
    REQUIRE(window.get_draw_count() == 3);
}

// ============================================================================
// IDebugWindow default shortcut key
// ============================================================================

class NoShortcutWindow : public IDebugWindow {
public:
    const char* get_name() const override { return "no_shortcut"; }
    const char* get_title() const override { return "No Shortcut"; }
    void draw() override {}
    // Don't override get_shortcut_key() - should return 0
};

TEST_CASE("IDebugWindow default shortcut is 0", "[debug-gui][window]") {
    NoShortcutWindow window;

    REQUIRE(window.get_shortcut_key() == 0);
}

// ============================================================================
// IDebugWindow lifecycle callbacks default
// ============================================================================

class MinimalWindow : public IDebugWindow {
public:
    const char* get_name() const override { return "minimal"; }
    const char* get_title() const override { return "Minimal"; }
    void draw() override {}
    // Don't override on_open/on_close - should work with defaults
};

TEST_CASE("IDebugWindow default lifecycle callbacks", "[debug-gui][window]") {
    MinimalWindow window;

    // These should not crash - just use default empty implementations
    window.set_open(true);
    REQUIRE(window.is_open());

    window.set_open(false);
    REQUIRE_FALSE(window.is_open());
}
