#pragma once

#include <engine/core/project_settings.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace engine::core {

enum class WindowBackend { X11, Wayland, Win32 };

struct NativeHandles {
    void* display = nullptr;   // X11 Display* or wl_display*
    void* window  = nullptr;   // X11 Window (cast to void*) or wl_surface*
    WindowBackend backend = WindowBackend::X11;
};

// Key actions understood by the engine's UI input system.
// Platform backends map native keysyms/VK codes to these.
enum class KeyAction {
    Backspace, Delete, Left, Right, Home, End,
    Enter, Tab, Escape,
    Up, Down, Space
};

// Callbacks from platform window into the application layer.
// Set before create() and invoked from poll_events().
struct WindowCallbacks {
    std::function<void()>                   on_close;
    std::function<void(uint32_t, uint32_t)> on_resize;      // width, height
    std::function<void(bool)>               on_focus;        // focused
    std::function<void(float, float)>       on_mouse_move;   // x, y (absolute)
    std::function<void(int, bool)>          on_mouse_button; // button (0=L,1=R,2=M), pressed
    std::function<void(float, float)>       on_scroll;       // dx, dy
    std::function<void(KeyAction, bool)>    on_key;          // action, pressed
    std::function<void(const char*)>        on_text_input;   // UTF-8 text
};

class PlatformWindow {
public:
    virtual ~PlatformWindow() = default;
    virtual bool create(const WindowSettings& settings, const WindowCallbacks& callbacks) = 0;
    virtual void destroy() = 0;
    virtual bool poll_events() = 0;  // returns false if window closed
    virtual NativeHandles native_handles() const = 0;
    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;
    virtual void set_fullscreen(bool fs) = 0;
};

// Factory: creates the best available backend for the current platform.
// On Linux, tries Wayland first (if compiled in), falls back to X11.
// Respects ENGINE_WINDOWING_BACKEND=x11 environment variable.
std::unique_ptr<PlatformWindow> create_platform_window();

// Shared helper: maps X11/XKB keysyms to KeyAction (used by X11 and Wayland backends).
#if defined(__linux__)
inline bool keysym_to_key_action(uint32_t keysym, KeyAction& out) {
    switch (keysym) {
        case 0xFF08: out = KeyAction::Backspace; return true; // XK_BackSpace
        case 0xFFFF: out = KeyAction::Delete;    return true; // XK_Delete
        case 0xFF51: out = KeyAction::Left;      return true; // XK_Left
        case 0xFF53: out = KeyAction::Right;     return true; // XK_Right
        case 0xFF50: out = KeyAction::Home;      return true; // XK_Home
        case 0xFF57: out = KeyAction::End;       return true; // XK_End
        case 0xFF0D: out = KeyAction::Enter;     return true; // XK_Return
        case 0xFF09: out = KeyAction::Tab;       return true; // XK_Tab
        case 0xFF1B: out = KeyAction::Escape;    return true; // XK_Escape
        case 0xFF52: out = KeyAction::Up;        return true; // XK_Up
        case 0xFF54: out = KeyAction::Down;      return true; // XK_Down
        case 0x0020: out = KeyAction::Space;     return true; // XK_space
        default: return false;
    }
}
#endif

} // namespace engine::core
