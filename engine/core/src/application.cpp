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
#include <engine/render/render_pipeline.hpp>
#include <engine/render/render_systems.hpp>
#include <engine/plugin/plugin.hpp>
#include <engine/audio/audio_system.hpp>
#include <engine/streaming/streaming_volume.hpp>
#include <engine/cinematic/cinematic.hpp>
#include <engine/navigation/navigation_systems.hpp>
#include <engine/terrain/terrain.hpp>
#include <engine/vegetation/vegetation_systems.hpp>
#include <engine/ui/ui_context.hpp>
#include <engine/ui/ui_system.hpp>
#include <engine/script/script_context.hpp>

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif


#include <engine/physics/physics_world.hpp>
#include <engine/physics/physics_system.hpp>

// Helper to access physics world from terrain module
namespace engine::terrain {
    extern engine::physics::PhysicsWorld& get_physics_world();
}

namespace engine::core {
    // Global pointer for the terrain accessor
    static engine::physics::PhysicsWorld* s_physics_world_instance = nullptr;
}

namespace engine::terrain {
    engine::physics::PhysicsWorld& get_physics_world() {
        if (!engine::core::s_physics_world_instance) {
            // Should not happen if application invalid
            static std::unique_ptr<engine::physics::PhysicsWorld> fallback = std::make_unique<engine::physics::PhysicsWorld>();
            return *fallback;
        }
        return *engine::core::s_physics_world_instance;
    }
}

namespace engine::core {

Application::Application()
    : m_clock(ProjectSettings::get().physics.fixed_timestep)
{
}

Application::~Application() {
    // Ensure plugin is unloaded before engine systems are destroyed
    unload_game_plugin();
    
    // Clear global pointer
    if (s_physics_world_instance == m_physics_world.get()) {
        s_physics_world_instance = nullptr;
    }
}

int Application::run(int argc, char** argv) {
    // Parse command line arguments
    parse_args(argc, argv);

    // Set working directory to the executable's directory so relative paths
    // (shaders/, project.json, etc.) resolve correctly from any launch location
    {
        std::filesystem::path exe_dir;
#ifdef _WIN32
        char exe_path[MAX_PATH];
        if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
            exe_dir = std::filesystem::path(exe_path).parent_path();
        }
#else
        if (argc > 0 && argv && argv[0]) {
            exe_dir = std::filesystem::path(argv[0]).parent_path();
        }
#endif
        if (!exe_dir.empty() && std::filesystem::exists(exe_dir)) {
            std::filesystem::current_path(exe_dir);
        }
    }

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
    
    // Initialize Physics
    m_physics_world = std::make_unique<physics::PhysicsWorld>();
    m_physics_world->init(settings().physics);
    s_physics_world_instance = m_physics_world.get();
    
    m_physics_system = std::make_unique<physics::PhysicsSystem>(*m_physics_world);

    // Initialize script context with all subsystems
    script::init_script_context(m_world.get(), m_physics_world.get());

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
        } else {
            // Create and initialize render pipeline
            m_render_pipeline = std::make_unique<render::RenderPipeline>();
            render::RenderPipelineConfig pipeline_config;
            m_render_pipeline->init(m_renderer.get(), pipeline_config);

            // Initialize and register render systems
            render::init_render_systems(m_render_pipeline.get(), m_renderer.get());
            render::register_render_systems(*m_engine_scheduler);

            // Initialize and register terrain systems
            terrain::init_terrain_systems();
            terrain::register_terrain_systems(*m_engine_scheduler);

            // Initialize and register vegetation systems
            vegetation::init_vegetation_systems();
            vegetation::register_vegetation_systems(*m_engine_scheduler);

            // Initialize UI system
            m_ui_context = std::make_unique<ui::UIContext>();
            m_ui_input_state = std::make_unique<ui::UIInputState>();
            if (m_ui_context->init(m_renderer.get())) {
                m_ui_context->set_screen_size(m_window_width, m_window_height);
                ui::set_ui_context(m_ui_context.get());
                log(LogLevel::Info, "UI system initialized");
            } else {
                log(LogLevel::Error, "Failed to initialize UI system");
                m_ui_context.reset();
                m_ui_input_state.reset();
            }
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
        // Begin UI input frame before processing events
        if (m_ui_input_state) {
            ui::ui_input_begin_frame(*m_ui_input_state);
        }

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

        // Update UI system
        if (m_ui_context && m_ui_input_state) {
            m_ui_context->update(static_cast<float>(dt), *m_ui_input_state);
            ui::ui_input_end_frame(*m_ui_input_state);
        }

        // Begin renderer frame
        if (m_renderer) {
            m_renderer->begin_frame();
        }

        // Run PreRender phase
        if (m_system_registry) {
            m_system_registry->run(*m_world, dt, scene::Phase::PreRender);
        }

        // Rendering (custom hook for subclassed apps)
        on_render(m_clock.get_alpha());

        // Run Render phase (render_submit_system fires here)
        if (m_system_registry) {
            m_system_registry->run(*m_world, dt, scene::Phase::Render);
        }

        // Render UI (after 3D scene, before PostRender)
        if (m_ui_context && m_renderer) {
            // Use specific view ID for UI rendering to ensure it draws on top
            m_ui_context->render(static_cast<render::RenderView>(200));
        }

        // Run PostRender phase
        if (m_system_registry) {
            m_system_registry->run(*m_world, dt, scene::Phase::PostRender);
        }

        // End renderer frame
        if (m_renderer) {
            m_renderer->end_frame();
        }

        // Screenshot automation: capture at target frame, quit a few frames later
        ++m_frame_counter;
        if (!m_screenshot_path.empty()) {
            if (m_frame_counter == static_cast<uint32_t>(m_screenshot_frame)) {
                save_screenshot(m_screenshot_path);
            }
            // Quit a few frames after the screenshot to let the async read complete
            if (m_frame_counter >= static_cast<uint32_t>(m_screenshot_frame) + 5) {
                m_quit_requested = true;
            }
        }
    }

    // Call user shutdown
    on_shutdown();

    // Unload game plugin
    unload_game_plugin();

    // Destroy engine systems
    m_system_registry.reset();
    m_engine_scheduler.reset();

    // Shutdown script context
    script::shutdown_script_context();

    // Shutdown navigation system
    navigation::navigation_shutdown();

    // Shutdown terrain systems
    terrain::shutdown_terrain_systems();

    // Shutdown render systems
    render::shutdown_render_systems();

    // Shutdown UI system before renderer
    if (m_ui_context) {
        ui::set_ui_context(nullptr);
        m_ui_context->shutdown();
        m_ui_context.reset();
    }
    m_ui_input_state.reset();

    // Shutdown render pipeline before renderer
    if (m_render_pipeline) {
        m_render_pipeline->shutdown();
        m_render_pipeline.reset();
    }

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
    m_game_context->ui_context = m_ui_context.get();
    m_game_context->app = this;

    // Create and initialize hot reload manager
    m_hot_reload_manager = std::make_unique<plugin::HotReloadManager>();

    plugin::HotReloadConfig config;
    config.enabled = m_hot_reload_enabled;
    config.preserve_state = settings().hot_reload.preserve_state;
    config.poll_interval_ms = static_cast<float>(settings().hot_reload.poll_interval_ms);

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

bool Application::save_screenshot(const std::string& path) {
    if (!m_renderer || !m_render_pipeline) return false;
    auto tex = m_render_pipeline->get_final_texture();
    if (tex.id == 0) return false;
    return m_renderer->save_screenshot(path, tex);
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
        // --screenshot=<path>
        else if (std::strncmp(arg, "--screenshot=", 13) == 0) {
            m_screenshot_path = arg + 13;
        } else if (std::strcmp(arg, "--screenshot") == 0 && i + 1 < argc) {
            m_screenshot_path = argv[++i];
        }
        // --screenshot-frame=<N>
        else if (std::strncmp(arg, "--screenshot-frame=", 19) == 0) {
            m_screenshot_frame = std::atoi(arg + 19);
            if (m_screenshot_frame < 1) m_screenshot_frame = 1;
        } else if (std::strcmp(arg, "--screenshot-frame") == 0 && i + 1 < argc) {
            m_screenshot_frame = std::atoi(argv[++i]);
            if (m_screenshot_frame < 1) m_screenshot_frame = 1;
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
                    // Update UI screen size
                    if (app->get_ui_context()) {
                        app->get_ui_context()->set_screen_size(width, height);
                    }
                }
            }
            return 0;

        case WM_SETFOCUS:
            events().dispatch(WindowFocusEvent{true});
            return 0;

        case WM_KILLFOCUS:
            events().dispatch(WindowFocusEvent{false});
            return 0;

        // UI input handling
        case WM_MOUSEMOVE:
            if (app && app->get_ui_context()) {
                auto* state = app->get_ui_input_state();
                if (state) {
                    float x = static_cast<float>(LOWORD(lParam));
                    float y = static_cast<float>(HIWORD(lParam));
                    state->mouse_delta.x = x - state->mouse_position.x;
                    state->mouse_delta.y = y - state->mouse_position.y;
                    state->mouse_position.x = x;
                    state->mouse_position.y = y;
                }
            }
            return 0;

        case WM_LBUTTONDOWN:
            if (app && app->get_ui_input_state()) {
                app->get_ui_input_state()->mouse_buttons[0] = true;
            }
            return 0;

        case WM_LBUTTONUP:
            if (app && app->get_ui_input_state()) {
                app->get_ui_input_state()->mouse_buttons[0] = false;
            }
            return 0;

        case WM_RBUTTONDOWN:
            if (app && app->get_ui_input_state()) {
                app->get_ui_input_state()->mouse_buttons[1] = true;
            }
            return 0;

        case WM_RBUTTONUP:
            if (app && app->get_ui_input_state()) {
                app->get_ui_input_state()->mouse_buttons[1] = false;
            }
            return 0;

        case WM_MBUTTONDOWN:
            if (app && app->get_ui_input_state()) {
                app->get_ui_input_state()->mouse_buttons[2] = true;
            }
            return 0;

        case WM_MBUTTONUP:
            if (app && app->get_ui_input_state()) {
                app->get_ui_input_state()->mouse_buttons[2] = false;
            }
            return 0;

        case WM_MOUSEWHEEL:
            if (app && app->get_ui_input_state()) {
                float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
                app->get_ui_input_state()->scroll_delta.y = delta;
            }
            return 0;

        case WM_CHAR:
            if (app && app->get_ui_input_state() && wParam >= 32) {
                // Append character to text input (UTF-8)
                char utf8[5] = {};
                if (wParam < 0x80) {
                    utf8[0] = static_cast<char>(wParam);
                } else if (wParam < 0x800) {
                    utf8[0] = static_cast<char>(0xC0 | (wParam >> 6));
                    utf8[1] = static_cast<char>(0x80 | (wParam & 0x3F));
                }
                app->get_ui_input_state()->text_input += utf8;
            }
            return 0;

        case WM_KEYDOWN:
            if (app && app->get_ui_input_state()) {
                switch (wParam) {
                    case VK_BACK: app->get_ui_input_state()->key_backspace = true; break;
                    case VK_DELETE: app->get_ui_input_state()->key_delete = true; break;
                    case VK_LEFT: app->get_ui_input_state()->key_left = true; break;
                    case VK_RIGHT: app->get_ui_input_state()->key_right = true; break;
                    case VK_HOME: app->get_ui_input_state()->key_home = true; break;
                    case VK_END: app->get_ui_input_state()->key_end = true; break;
                    case VK_RETURN: app->get_ui_input_state()->key_enter = true; break;
                    case VK_TAB: app->get_ui_input_state()->key_tab = true; break;
                    case VK_ESCAPE: app->get_ui_input_state()->key_escape = true; break;
                    case VK_UP: app->get_ui_input_state()->nav_up = true; break;
                    case VK_DOWN: app->get_ui_input_state()->nav_down = true; break;
                    case VK_SPACE: app->get_ui_input_state()->nav_confirm = true; break;
                }
            }
            return 0;

        case WM_KEYUP:
            if (app && app->get_ui_input_state()) {
                switch (wParam) {
                    case VK_UP: app->get_ui_input_state()->nav_up = false; break;
                    case VK_DOWN: app->get_ui_input_state()->nav_down = false; break;
                    case VK_SPACE: app->get_ui_input_state()->nav_confirm = false; break;
                }
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

void Application::register_engine_systems() {
    if (!m_engine_scheduler) return;

    // Transform system in FixedUpdate for physics (priority 10 = runs first)
    m_engine_scheduler->add(scene::Phase::FixedUpdate, scene::transform_system, "transform_fixed", 10);

    // Navigation behaviors in FixedUpdate (priority 6, before agents)
    m_engine_scheduler->add(scene::Phase::FixedUpdate, navigation::navigation_behavior_system, "nav_behaviors", 6);

    // Navigation agents in FixedUpdate (priority 5, after behaviors)
    m_engine_scheduler->add(scene::Phase::FixedUpdate, navigation::navigation_agent_system, "nav_agents", 5);

    // Navigation obstacles in FixedUpdate (priority 4, after agents)
    m_engine_scheduler->add(scene::Phase::FixedUpdate, navigation::navigation_obstacle_system, "nav_obstacles", 4);

    // Transform system in PostUpdate for audio/render (priority 10 = runs first)
    m_engine_scheduler->add(scene::Phase::PostUpdate, scene::transform_system, "transform", 10);

    // Streaming systems in PostUpdate, after transform (priority 9-6)
    // Entity system runs first to handle migration before unloads
    m_engine_scheduler->add(scene::Phase::PostUpdate, streaming::streaming_entity_system, "streaming_entities", 9);
    m_engine_scheduler->add(scene::Phase::PostUpdate, streaming::streaming_update_system, "streaming_update", 8);
    m_engine_scheduler->add(scene::Phase::PostUpdate, streaming::streaming_zone_system, "streaming_zones", 7);
    m_engine_scheduler->add(scene::Phase::PostUpdate, streaming::streaming_volume_system, "streaming_volumes", 7);
    m_engine_scheduler->add(scene::Phase::PostUpdate, streaming::streaming_portal_system, "streaming_portals", 6);

    // Audio systems in PostUpdate, after transform (lower priority = runs later)
    m_engine_scheduler->add(scene::Phase::PostUpdate, audio::AudioSystem::update_listener, "audio_listener", 5);
    m_engine_scheduler->add(scene::Phase::PostUpdate, audio::AudioSystem::update_sources, "audio_sources", 4);
    m_engine_scheduler->add(scene::Phase::PostUpdate, audio::AudioSystem::process_triggers, "audio_triggers", 3);
    m_engine_scheduler->add(scene::Phase::PostUpdate, audio::AudioSystem::update_reverb_zones, "audio_reverb", 2);

    // Cinematic system in Update phase (priority 0 = runs after game logic)
    m_engine_scheduler->add(scene::Phase::Update, cinematic::cinematic_update_system, "cinematic", 0);
}

} // namespace engine::core
