#include <engine/core/application.hpp>
#include <engine/core/time.hpp>
#include <engine/core/job_system.hpp>
#include <engine/core/log.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <engine/core/events.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/systems.hpp>
#include <engine/scene/transform.hpp>
#include <engine/render/renderer.hpp>
#include <engine/plugin/plugin.hpp>
#include <engine/audio/audio_system.hpp>

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace engine::core {

Application::Application()
    : m_clock(ProjectSettings::get().physics.fixed_timestep)
{
}

Application::~Application() {
    // Ensure plugin is unloaded before engine systems are destroyed
    unload_game_plugin();
}

int Application::run(int argc, char** argv) {
    // Parse command line arguments
    parse_args(argc, argv);

    // Load project settings
    settings().load("project.json");

    // Apply hot reload setting from project settings if not overridden by command line
    if (!m_hot_reload_override) {
        m_hot_reload_enabled = settings().hot_reload.enabled;
    }

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

    // Initialize engine systems
    m_world = std::make_unique<scene::World>();
    m_engine_scheduler = std::make_unique<scene::Scheduler>();
    m_system_registry = std::make_unique<plugin::SystemRegistry>();
    m_system_registry->set_engine_scheduler(m_engine_scheduler.get());

    // Register core engine systems
    register_engine_systems();

    // Initialize renderer
    m_renderer = render::create_bgfx_renderer();
    if (m_renderer) {
        if (!m_renderer->init(m_native_window, m_window_width, m_window_height)) {
            log(LogLevel::Error, "Failed to initialize renderer");
            m_renderer.reset();
        }
    }

    m_initialized = true;

    // Call user init (for subclassed applications)
    on_init();

    // Load game plugin if specified
    if (!m_game_dll_path.empty()) {
        if (!load_game_plugin(m_game_dll_path)) {
            log(LogLevel::Error, "Failed to load game plugin: {}", m_game_dll_path.string());
            // Continue running - allows for reload attempts
        }
    }

    // Main loop
    while (!m_quit_requested) {
        // Poll window events
        if (!poll_events()) {
            m_quit_requested = true;
            break;
        }

        // Poll for hot reload
        if (m_hot_reload_manager) {
            m_hot_reload_manager->poll();
        }

        // Process deferred events from previous frame
        events().flush();

        // Update time
        Time::update();
        double dt = Time::delta_time();

        // Update clock accumulator
        m_clock.update(dt);

        // Fixed update loop
        while (m_clock.consume_tick()) {
            // Run engine and game systems for FixedUpdate phase
            if (m_system_registry) {
                m_system_registry->run(*m_world, m_clock.fixed_dt, scene::Phase::FixedUpdate);
            }
            on_fixed_update(m_clock.fixed_dt);
        }

        // Run PreUpdate phase
        if (m_system_registry) {
            m_system_registry->run(*m_world, dt, scene::Phase::PreUpdate);
        }

        // Run Update phase
        if (m_system_registry) {
            m_system_registry->run(*m_world, dt, scene::Phase::Update);
        }

        // Variable update callback
        on_update(dt);

        // Run PostUpdate phase
        if (m_system_registry) {
            m_system_registry->run(*m_world, dt, scene::Phase::PostUpdate);
        }

        // Run PreRender phase
        if (m_system_registry) {
            m_system_registry->run(*m_world, dt, scene::Phase::PreRender);
        }

        // Rendering
        on_render(m_clock.get_alpha());

        // Run PostRender phase
        if (m_system_registry) {
            m_system_registry->run(*m_world, dt, scene::Phase::PostRender);
        }
    }

    // Call user shutdown
    on_shutdown();

    // Unload game plugin
    unload_game_plugin();

    // Destroy engine systems
    m_system_registry.reset();
    m_engine_scheduler.reset();
    if (m_renderer) {
        m_renderer->shutdown();
    }
    m_renderer.reset();
    m_world.reset();

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

bool Application::load_game_plugin(const std::filesystem::path& dll_path) {
    // Unload existing plugin first
    unload_game_plugin();

    if (!std::filesystem::exists(dll_path)) {
        log(LogLevel::Error, "Game plugin not found: {}", dll_path.string());
        return false;
    }

    // Create game context
    m_game_context = std::make_unique<plugin::GameContext>();
    m_game_context->world = m_world.get();
    m_game_context->scheduler = m_engine_scheduler.get();
    m_game_context->renderer = m_renderer.get();
    m_game_context->app = this;

    // Create and initialize hot reload manager
    m_hot_reload_manager = std::make_unique<plugin::HotReloadManager>();

    plugin::HotReloadConfig config;
    config.enabled = m_hot_reload_enabled;
    config.preserve_state = settings().hot_reload.preserve_state;
    config.poll_interval_ms = settings().hot_reload.poll_interval_ms;

    m_hot_reload_manager->init(dll_path, m_game_context.get(), m_system_registry.get(), config);

    if (!m_hot_reload_manager->get_loader().is_loaded()) {
        log(LogLevel::Error, "Failed to load game plugin");
        m_hot_reload_manager.reset();
        m_game_context.reset();
        return false;
    }

    log(LogLevel::Info, "Game plugin loaded: {}", dll_path.string());
    return true;
}

void Application::unload_game_plugin() {
    if (m_hot_reload_manager) {
        m_hot_reload_manager->shutdown();
        m_hot_reload_manager.reset();
    }
    m_game_context.reset();
}

bool Application::has_game_plugin() const {
    return m_hot_reload_manager && m_hot_reload_manager->get_loader().is_loaded();
}

void Application::parse_args(int argc, char** argv) {
    if (argc <= 0 || argv == nullptr) {
        return;
    }

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (!arg) continue;

        // --game-dll=<path> or --game-dll <path>
        if (std::strncmp(arg, "--game-dll=", 11) == 0) {
            m_game_dll_path = arg + 11;
        } else if (std::strcmp(arg, "--game-dll") == 0 && i + 1 < argc) {
            m_game_dll_path = argv[++i];
        }
        // --hot-reload=on/off or --hot-reload on/off
        else if (std::strncmp(arg, "--hot-reload=", 13) == 0) {
            const char* value = arg + 13;
            m_hot_reload_enabled = (std::strcmp(value, "on") == 0 ||
                                    std::strcmp(value, "true") == 0 ||
                                    std::strcmp(value, "1") == 0);
            m_hot_reload_override = true;
        } else if (std::strcmp(arg, "--hot-reload") == 0 && i + 1 < argc) {
            const char* value = argv[++i];
            m_hot_reload_enabled = (std::strcmp(value, "on") == 0 ||
                                    std::strcmp(value, "true") == 0 ||
                                    std::strcmp(value, "1") == 0);
            m_hot_reload_override = true;
        }
        // --no-hot-reload
        else if (std::strcmp(arg, "--no-hot-reload") == 0) {
            m_hot_reload_enabled = false;
            m_hot_reload_override = true;
        }
    }
}

// Platform-specific window implementation
#ifdef _WIN32

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Application* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CLOSE:
            events().dispatch(WindowCloseEvent{});
            if (app) app->quit();
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            if (app && wParam != SIZE_MINIMIZED) {
                uint32_t width = LOWORD(lParam);
                uint32_t height = HIWORD(lParam);
                if (width > 0 && height > 0) {
                    events().dispatch(WindowResizeEvent{width, height});
                }
            }
            return 0;

        case WM_SETFOCUS:
            events().dispatch(WindowFocusEvent{true});
            return 0;

        case WM_KILLFOCUS:
            events().dispatch(WindowFocusEvent{false});
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

void Application::register_engine_systems() {
    if (!m_engine_scheduler) return;

    // Transform system in FixedUpdate for physics (priority 10 = runs first)
    m_engine_scheduler->add(scene::Phase::FixedUpdate, scene::transform_system, "transform_fixed", 10);

    // Transform system in PostUpdate for audio/render (priority 10 = runs first)
    m_engine_scheduler->add(scene::Phase::PostUpdate, scene::transform_system, "transform", 10);

    // Audio systems in PostUpdate, after transform (lower priority = runs later)
    m_engine_scheduler->add(scene::Phase::PostUpdate, audio::AudioSystem::update_listener, "audio_listener", 5);
    m_engine_scheduler->add(scene::Phase::PostUpdate, audio::AudioSystem::update_sources, "audio_sources", 4);
    m_engine_scheduler->add(scene::Phase::PostUpdate, audio::AudioSystem::process_triggers, "audio_triggers", 3);
    m_engine_scheduler->add(scene::Phase::PostUpdate, audio::AudioSystem::update_reverb_zones, "audio_reverb", 2);
}

} // namespace engine::core
