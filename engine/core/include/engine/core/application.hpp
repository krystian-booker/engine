#pragma once

#include <engine/core/project_settings.hpp>
#include <engine/core/game_clock.hpp>
#include <string>
#include <cstdint>

namespace engine::core {

// Abstract application base class
// Provides window creation and basic game loop infrastructure
// Subclass this in your game to create a full application
class Application {
public:
    Application();
    virtual ~Application();

    // Non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Run the main game loop
    // Returns exit code
    int run(int argc = 0, char** argv = nullptr);

    // Request application to quit
    void quit();

    // Check if quit was requested
    bool should_quit() const { return m_quit_requested; }

protected:
    // Override these in your application
    virtual void on_init() {}
    virtual void on_shutdown() {}
    virtual void on_fixed_update(double dt) { (void)dt; }
    virtual void on_update(double dt) { (void)dt; }
    virtual void on_render(double alpha) { (void)alpha; }

    // Access settings
    ProjectSettings& settings() { return ProjectSettings::get(); }

    // Window management (platform specific)
    bool create_window(const WindowSettings& settings);
    void destroy_window();
    bool poll_events();  // Returns false if quit requested
    void* get_native_window_handle() { return m_native_window; }

    // Get window dimensions
    uint32_t window_width() const { return m_window_width; }
    uint32_t window_height() const { return m_window_height; }

    // Get clock for fixed timestep management
    GameClock& clock() { return m_clock; }

private:
    GameClock m_clock;
    bool m_quit_requested = false;
    bool m_initialized = false;
    uint32_t m_window_width = 1280;
    uint32_t m_window_height = 720;
    void* m_native_window = nullptr;
};

} // namespace engine::core
