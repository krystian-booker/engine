#include <engine/core/application.hpp>
#include <engine/core/time.hpp>
#include <engine/core/job_system.hpp>
#include <engine/core/log.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace engine::core {

Application::Application()
    : m_clock(ProjectSettings::get().physics.fixed_timestep)
{
}

Application::~Application() = default;

int Application::run(int /*argc*/, char** /*argv*/) {
    // Load project settings
    settings().load("project.json");

    // Update clock timestep from settings
    m_clock.fixed_dt = settings().physics.fixed_timestep;

    // Initialize core systems
    JobSystem::init();
    Time::init();

    // Create window
    if (!create_window(settings().window)) {
        log(LogLevel::Error, "Failed to create window");
        return 1;
    }

    m_initialized = true;

    // Call user init
    on_init();

    // Main loop
    while (!m_quit_requested) {
        // Poll window events
        if (!poll_events()) {
            m_quit_requested = true;
            break;
        }

        // Update time
        Time::update();
        double dt = Time::delta_time();

        // Update clock accumulator
        m_clock.update(dt);

        // Fixed update loop
        while (m_clock.consume_tick()) {
            on_fixed_update(m_clock.fixed_dt);
        }

        // Variable update
        on_update(dt);

        // Rendering
        on_render(m_clock.get_alpha());
    }

    // Call user shutdown
    on_shutdown();

    // Destroy window
    destroy_window();

    // Shutdown job system
    JobSystem::shutdown();

    m_initialized = false;

    return 0;
}

void Application::quit() {
    m_quit_requested = true;
}

// Platform-specific window implementation
#ifdef _WIN32

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Application* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CLOSE:
            if (app) app->quit();
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE && app) {
                app->quit();
            }
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Application::create_window(const WindowSettings& ws) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "EngineWindowClass";

    if (!RegisterClassEx(&wc)) {
        log(LogLevel::Error, "Failed to register window class");
        return false;
    }

    // Calculate window size for desired client area
    RECT rect = {0, 0, static_cast<LONG>(ws.width), static_cast<LONG>(ws.height)};
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (ws.borderless) {
        style = WS_POPUP;
    }
    AdjustWindowRect(&rect, style, FALSE);

    // Create window
    HWND hwnd = CreateWindowEx(
        0,
        "EngineWindowClass",
        ws.title.c_str(),
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        log(LogLevel::Error, "Failed to create window");
        return false;
    }

    // Store this pointer for window proc
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Show window
    ShowWindow(hwnd, ws.fullscreen ? SW_MAXIMIZE : SW_SHOW);
    UpdateWindow(hwnd);

    m_native_window = hwnd;
    m_window_width = ws.width;
    m_window_height = ws.height;

    return true;
}

void Application::destroy_window() {
    if (m_native_window) {
        DestroyWindow(static_cast<HWND>(m_native_window));
        m_native_window = nullptr;
    }
    UnregisterClass("EngineWindowClass", GetModuleHandle(nullptr));
}

bool Application::poll_events() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

#else
// Stub implementations for non-Windows platforms
bool Application::create_window(const WindowSettings&) {
    log(LogLevel::Error, "Window creation not implemented for this platform");
    return false;
}

void Application::destroy_window() {}

bool Application::poll_events() {
    return false;
}
#endif

} // namespace engine::core
