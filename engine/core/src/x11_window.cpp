#if defined(__linux__)

#include <engine/core/platform_window.hpp>
#include <engine/core/log.hpp>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

namespace engine::core {

class X11Window : public PlatformWindow {
public:
    bool create(const WindowSettings& ws, const WindowCallbacks& callbacks) override {
        m_callbacks = callbacks;

        m_display = XOpenDisplay(nullptr);
        if (!m_display) {
            log(LogLevel::Error, "Failed to open X11 display");
            return false;
        }

        int screen = DefaultScreen(m_display);
        ::Window root = RootWindow(m_display, screen);

        m_window = XCreateSimpleWindow(
            m_display, root,
            0, 0, ws.width, ws.height,
            0,
            BlackPixel(m_display, screen),
            BlackPixel(m_display, screen)
        );

        if (!m_window) {
            log(LogLevel::Error, "Failed to create X11 window");
            XCloseDisplay(m_display);
            m_display = nullptr;
            return false;
        }

        XStoreName(m_display, m_window, ws.title.c_str());

        XSelectInput(m_display, m_window,
            KeyPressMask | KeyReleaseMask |
            ButtonPressMask | ButtonReleaseMask |
            PointerMotionMask | StructureNotifyMask |
            FocusChangeMask | ExposureMask);

        m_wm_delete_window = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(m_display, m_window, &m_wm_delete_window, 1);

        if (ws.borderless) {
            struct {
                unsigned long flags;
                unsigned long functions;
                unsigned long decorations;
                long input_mode;
                unsigned long status;
            } hints = {};
            hints.flags = 2; // MWM_HINTS_DECORATIONS
            hints.decorations = 0;
            Atom motif_hints = XInternAtom(m_display, "_MOTIF_WM_HINTS", False);
            XChangeProperty(m_display, m_window, motif_hints, motif_hints, 32,
                PropModeReplace, reinterpret_cast<unsigned char*>(&hints), 5);
        }

        XMapWindow(m_display, m_window);
        XFlush(m_display);

        if (ws.fullscreen) {
            m_net_wm_state = XInternAtom(m_display, "_NET_WM_STATE", False);
            m_net_wm_state_fullscreen = XInternAtom(m_display, "_NET_WM_STATE_FULLSCREEN", False);

            XEvent xev = {};
            xev.type = ClientMessage;
            xev.xclient.window = m_window;
            xev.xclient.message_type = m_net_wm_state;
            xev.xclient.format = 32;
            xev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
            xev.xclient.data.l[1] = static_cast<long>(m_net_wm_state_fullscreen);
            xev.xclient.data.l[2] = 0;

            XSendEvent(m_display, root, False,
                SubstructureRedirectMask | SubstructureNotifyMask, &xev);
            XFlush(m_display);
        }

        m_width = ws.width;
        m_height = ws.height;
        return true;
    }

    void destroy() override {
        if (m_display) {
            if (m_window) {
                XDestroyWindow(m_display, m_window);
                m_window = 0;
            }
            XCloseDisplay(m_display);
            m_display = nullptr;
        }
    }

    bool poll_events() override {
        if (!m_display) return false;

        while (XPending(m_display)) {
            XEvent event;
            XNextEvent(m_display, &event);

            switch (event.type) {
                case ClientMessage:
                    if (static_cast<Atom>(event.xclient.data.l[0]) == m_wm_delete_window) {
                        if (m_callbacks.on_close) m_callbacks.on_close();
                        m_closed = true;
                        return false;
                    }
                    break;

                case ConfigureNotify: {
                    uint32_t w = static_cast<uint32_t>(event.xconfigure.width);
                    uint32_t h = static_cast<uint32_t>(event.xconfigure.height);
                    if (w != m_width || h != m_height) {
                        m_width = w;
                        m_height = h;
                        if (m_callbacks.on_resize) m_callbacks.on_resize(w, h);
                    }
                    break;
                }

                case FocusIn:
                    if (m_callbacks.on_focus) m_callbacks.on_focus(true);
                    break;

                case FocusOut:
                    if (m_callbacks.on_focus) m_callbacks.on_focus(false);
                    break;

                case MotionNotify:
                    if (m_callbacks.on_mouse_move)
                        m_callbacks.on_mouse_move(
                            static_cast<float>(event.xmotion.x),
                            static_cast<float>(event.xmotion.y));
                    break;

                case ButtonPress:
                    switch (event.xbutton.button) {
                        case Button1:
                            if (m_callbacks.on_mouse_button) m_callbacks.on_mouse_button(0, true);
                            break;
                        case Button2:
                            if (m_callbacks.on_mouse_button) m_callbacks.on_mouse_button(2, true);
                            break;
                        case Button3:
                            if (m_callbacks.on_mouse_button) m_callbacks.on_mouse_button(1, true);
                            break;
                        case Button4:
                            if (m_callbacks.on_scroll) m_callbacks.on_scroll(0.0f, 1.0f);
                            break;
                        case Button5:
                            if (m_callbacks.on_scroll) m_callbacks.on_scroll(0.0f, -1.0f);
                            break;
                    }
                    break;

                case ButtonRelease:
                    switch (event.xbutton.button) {
                        case Button1:
                            if (m_callbacks.on_mouse_button) m_callbacks.on_mouse_button(0, false);
                            break;
                        case Button2:
                            if (m_callbacks.on_mouse_button) m_callbacks.on_mouse_button(2, false);
                            break;
                        case Button3:
                            if (m_callbacks.on_mouse_button) m_callbacks.on_mouse_button(1, false);
                            break;
                    }
                    break;

                case KeyPress: {
                    KeySym keysym = XLookupKeysym(&event.xkey, 0);
                    KeyAction action;
                    if (keysym_to_key_action(static_cast<uint32_t>(keysym), action)) {
                        if (m_callbacks.on_key) m_callbacks.on_key(action, true);
                    }
                    // Text input
                    char buf[8] = {};
                    int len = XLookupString(&event.xkey, buf, sizeof(buf) - 1, nullptr, nullptr);
                    if (len > 0 && static_cast<unsigned char>(buf[0]) >= 32) {
                        if (m_callbacks.on_text_input) m_callbacks.on_text_input(buf);
                    }
                    break;
                }

                case KeyRelease: {
                    KeySym keysym = XLookupKeysym(&event.xkey, 0);
                    KeyAction action;
                    if (keysym_to_key_action(static_cast<uint32_t>(keysym), action)) {
                        if (m_callbacks.on_key) m_callbacks.on_key(action, false);
                    }
                    break;
                }
            }
        }
        return !m_closed;
    }

    NativeHandles native_handles() const override {
        NativeHandles h;
        h.display = m_display;
        h.window = reinterpret_cast<void*>(m_window);
        h.backend = WindowBackend::X11;
        return h;
    }

    uint32_t width() const override { return m_width; }
    uint32_t height() const override { return m_height; }

    void set_fullscreen(bool fs) override {
        if (!m_display || !m_window) return;

        if (m_net_wm_state == None) {
            m_net_wm_state = XInternAtom(m_display, "_NET_WM_STATE", False);
            m_net_wm_state_fullscreen = XInternAtom(m_display, "_NET_WM_STATE_FULLSCREEN", False);
        }

        XEvent xev = {};
        xev.type = ClientMessage;
        xev.xclient.window = m_window;
        xev.xclient.message_type = m_net_wm_state;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = fs ? 1 : 0; // ADD or REMOVE
        xev.xclient.data.l[1] = static_cast<long>(m_net_wm_state_fullscreen);
        xev.xclient.data.l[2] = 0;

        int screen = DefaultScreen(m_display);
        XSendEvent(m_display, RootWindow(m_display, screen), False,
            SubstructureRedirectMask | SubstructureNotifyMask, &xev);
        XFlush(m_display);
    }

private:
    Display* m_display = nullptr;
    ::Window m_window = 0;
    WindowCallbacks m_callbacks;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_closed = false;

    Atom m_wm_delete_window = None;
    Atom m_net_wm_state = None;
    Atom m_net_wm_state_fullscreen = None;
};

std::unique_ptr<PlatformWindow> create_x11_window() {
    return std::make_unique<X11Window>();
}

} // namespace engine::core

#endif // __linux__
