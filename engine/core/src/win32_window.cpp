#if defined(_WIN32)

#include <engine/core/platform_window.hpp>
#include <engine/core/log.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace engine::core {

class Win32Window : public PlatformWindow {
public:
    bool create(const WindowSettings& ws, const WindowCallbacks& callbacks) override {
        m_callbacks = callbacks;

        HINSTANCE hInstance = GetModuleHandle(nullptr);

        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = "EngineWindowClass";

        if (!RegisterClassEx(&wc)) {
            log(LogLevel::Error, "Failed to register window class");
            return false;
        }

        RECT rect = {0, 0, static_cast<LONG>(ws.width), static_cast<LONG>(ws.height)};
        DWORD style = WS_OVERLAPPEDWINDOW;
        if (ws.borderless) {
            style = WS_POPUP;
        }
        AdjustWindowRect(&rect, style, FALSE);

        m_hwnd = CreateWindowEx(
            0,
            "EngineWindowClass",
            ws.title.c_str(),
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left, rect.bottom - rect.top,
            nullptr, nullptr, hInstance, nullptr
        );

        if (!m_hwnd) {
            log(LogLevel::Error, "Failed to create window");
            return false;
        }

        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        ShowWindow(m_hwnd, ws.fullscreen ? SW_MAXIMIZE : SW_SHOW);
        UpdateWindow(m_hwnd);

        m_width = ws.width;
        m_height = ws.height;
        return true;
    }

    void destroy() override {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
        UnregisterClass("EngineWindowClass", GetModuleHandle(nullptr));
    }

    bool poll_events() override {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_closed = true;
                return false;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return !m_closed;
    }

    NativeHandles native_handles() const override {
        NativeHandles h;
        h.window = m_hwnd;
        h.backend = WindowBackend::Win32;
        return h;
    }

    uint32_t width() const override { return m_width; }
    uint32_t height() const override { return m_height; }

    void set_fullscreen(bool fs) override {
        if (!m_hwnd) return;
        if (fs) {
            ShowWindow(m_hwnd, SW_MAXIMIZE);
        } else {
            ShowWindow(m_hwnd, SW_RESTORE);
        }
    }

private:
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        auto* win = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch (msg) {
            case WM_CLOSE:
                if (win && win->m_callbacks.on_close) win->m_callbacks.on_close();
                if (win) win->m_closed = true;
                return 0;

            case WM_DESTROY:
                return 0;

            case WM_SIZE:
                if (win && wParam != SIZE_MINIMIZED) {
                    uint32_t w = LOWORD(lParam);
                    uint32_t h = HIWORD(lParam);
                    if (w > 0 && h > 0) {
                        win->m_width = w;
                        win->m_height = h;
                        if (win->m_callbacks.on_resize) win->m_callbacks.on_resize(w, h);
                    }
                }
                return 0;

            case WM_SETFOCUS:
                if (win && win->m_callbacks.on_focus) win->m_callbacks.on_focus(true);
                return 0;

            case WM_KILLFOCUS:
                if (win && win->m_callbacks.on_focus) win->m_callbacks.on_focus(false);
                return 0;

            case WM_MOUSEMOVE:
                if (win && win->m_callbacks.on_mouse_move) {
                    float x = static_cast<float>(LOWORD(lParam));
                    float y = static_cast<float>(HIWORD(lParam));
                    win->m_callbacks.on_mouse_move(x, y);
                }
                return 0;

            case WM_LBUTTONDOWN:
                if (win && win->m_callbacks.on_mouse_button) win->m_callbacks.on_mouse_button(0, true);
                return 0;
            case WM_LBUTTONUP:
                if (win && win->m_callbacks.on_mouse_button) win->m_callbacks.on_mouse_button(0, false);
                return 0;
            case WM_RBUTTONDOWN:
                if (win && win->m_callbacks.on_mouse_button) win->m_callbacks.on_mouse_button(1, true);
                return 0;
            case WM_RBUTTONUP:
                if (win && win->m_callbacks.on_mouse_button) win->m_callbacks.on_mouse_button(1, false);
                return 0;
            case WM_MBUTTONDOWN:
                if (win && win->m_callbacks.on_mouse_button) win->m_callbacks.on_mouse_button(2, true);
                return 0;
            case WM_MBUTTONUP:
                if (win && win->m_callbacks.on_mouse_button) win->m_callbacks.on_mouse_button(2, false);
                return 0;

            case WM_MOUSEWHEEL:
                if (win && win->m_callbacks.on_scroll) {
                    float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
                    win->m_callbacks.on_scroll(0.0f, delta);
                }
                return 0;

            case WM_CHAR:
                if (win && wParam >= 32 && win->m_callbacks.on_text_input) {
                    char utf8[5] = {};
                    if (wParam < 0x80) {
                        utf8[0] = static_cast<char>(wParam);
                    } else if (wParam < 0x800) {
                        utf8[0] = static_cast<char>(0xC0 | (wParam >> 6));
                        utf8[1] = static_cast<char>(0x80 | (wParam & 0x3F));
                    }
                    win->m_callbacks.on_text_input(utf8);
                }
                return 0;

            case WM_KEYDOWN:
                if (win && win->m_callbacks.on_key) {
                    KeyAction action;
                    bool mapped = true;
                    switch (wParam) {
                        case VK_BACK:   action = KeyAction::Backspace; break;
                        case VK_DELETE: action = KeyAction::Delete;    break;
                        case VK_LEFT:   action = KeyAction::Left;      break;
                        case VK_RIGHT:  action = KeyAction::Right;     break;
                        case VK_HOME:   action = KeyAction::Home;      break;
                        case VK_END:    action = KeyAction::End;       break;
                        case VK_RETURN: action = KeyAction::Enter;     break;
                        case VK_TAB:    action = KeyAction::Tab;       break;
                        case VK_ESCAPE: action = KeyAction::Escape;    break;
                        case VK_UP:     action = KeyAction::Up;        break;
                        case VK_DOWN:   action = KeyAction::Down;      break;
                        case VK_SPACE:  action = KeyAction::Space;     break;
                        default: mapped = false; break;
                    }
                    if (mapped) win->m_callbacks.on_key(action, true);
                }
                return 0;

            case WM_KEYUP:
                if (win && win->m_callbacks.on_key) {
                    KeyAction action;
                    bool mapped = true;
                    switch (wParam) {
                        case VK_UP:     action = KeyAction::Up;    break;
                        case VK_DOWN:   action = KeyAction::Down;  break;
                        case VK_SPACE:  action = KeyAction::Space; break;
                        default: mapped = false; break;
                    }
                    if (mapped) win->m_callbacks.on_key(action, false);
                }
                return 0;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    HWND m_hwnd = nullptr;
    WindowCallbacks m_callbacks;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_closed = false;
};

std::unique_ptr<PlatformWindow> create_win32_window() {
    return std::make_unique<Win32Window>();
}

} // namespace engine::core

#endif // _WIN32
