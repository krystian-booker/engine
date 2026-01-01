#pragma once

#include <cstdint>

namespace engine::debug_gui {

// Base interface for all debug windows
class IDebugWindow {
public:
    virtual ~IDebugWindow() = default;

    // Window identification
    virtual const char* get_name() const = 0;
    virtual const char* get_title() const = 0;

    // Lifecycle callbacks
    virtual void on_open() {}
    virtual void on_close() {}

    // Called every frame when visible
    virtual void draw() = 0;

    // Window state
    bool is_open() const { return m_open; }

    void set_open(bool open) {
        if (m_open != open) {
            m_open = open;
            if (m_open) {
                on_open();
            } else {
                on_close();
            }
        }
    }

    void toggle() { set_open(!m_open); }

    // Optional: custom shortcut key (0 = none)
    virtual uint32_t get_shortcut_key() const { return 0; }

protected:
    bool m_open = false;
};

} // namespace engine::debug_gui
