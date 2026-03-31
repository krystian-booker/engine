#include <engine/core/application.hpp>
#include <engine/core/platform_window.hpp>
#include <engine/core/time.hpp>
#include <engine/core/job_system.hpp>
#include <engine/core/log.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <engine/core/events.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/systems.hpp>
#include <engine/scene/transform.hpp>
#include <engine/plugin/plugin.hpp>
#include <engine/audio/audio_system.hpp>

#include <cstring>

#ifdef _WIN32
#include <climits>
#elif defined(__linux__)
#include <climits>
#include <unistd.h>
#endif


#include <engine/physics/physics_world.hpp>
#include <engine/physics/physics_system.hpp>

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

    // Set working directory to the executable's directory so relative paths
    // (shaders/, project.json, etc.) resolve correctly from any launch location
    {
        std::filesystem::path exe_dir;
#ifdef _WIN32
        char exe_path[MAX_PATH];
        if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
            exe_dir = std::filesystem::path(exe_path).parent_path();
        }
#elif defined(__linux__)
        char exe_path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len > 0) {
            exe_path[len] = '\0';
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

    // Create window unless running headless.
    if (!m_headless_mode) {
        if (!create_window(settings().window)) {
            log(LogLevel::Error, "Failed to create window");
            return 1;
        }
    } else {
        m_window_width = settings().window.width;
        m_window_height = settings().window.height;
        m_native_window = nullptr;
        m_native_display = nullptr;
        log(LogLevel::Info, "Running in headless mode ({}x{})", m_window_width, m_window_height);
    }

    // Initialize engine systems
    m_world = std::make_unique<scene::World>();

    // Initialize Physics
    m_physics_world = std::make_unique<physics::PhysicsWorld>();
    m_physics_world->init(settings().physics);

    m_physics_system = std::make_unique<physics::PhysicsSystem>(*m_physics_world);

    m_engine_scheduler = std::make_unique<scene::Scheduler>();
    m_system_registry = std::make_unique<plugin::SystemRegistry>();
    m_system_registry->set_engine_scheduler(m_engine_scheduler.get());

    // Register core engine systems
    register_engine_systems();

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
        if (!m_headless_mode) {
            if (!poll_events()) {
                m_quit_requested = true;
                break;
            }
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

        // Rendering (custom hook for subclassed apps)
        on_render(m_clock.get_alpha());

    }

    // Call user shutdown
    on_shutdown();

    // Unload game plugin
    unload_game_plugin();

    // Destroy engine systems
    m_system_registry.reset();
    m_engine_scheduler.reset();

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
        else if (std::strcmp(arg, "--headless") == 0) {
            m_headless_mode = true;
        }
    }
}

// Platform window creation with callback wiring
bool Application::create_window(const WindowSettings& ws) {
    m_platform_window = create_platform_window();
    if (!m_platform_window) {
        log(LogLevel::Error, "No windowing backend available");
        return false;
    }

    WindowCallbacks callbacks;
    callbacks.on_close = []() {
        events().dispatch(WindowCloseEvent{});
    };
    callbacks.on_resize = [this](uint32_t w, uint32_t h) {
        m_window_width = w;
        m_window_height = h;
        events().dispatch(WindowResizeEvent{w, h});
    };
    callbacks.on_focus = [](bool focused) {
        events().dispatch(WindowFocusEvent{focused});
    };

    if (!m_platform_window->create(ws, callbacks))
        return false;

    auto h = m_platform_window->native_handles();
    m_native_window = h.window;
    m_native_display = h.display;
    m_window_backend = h.backend;
    m_window_width = m_platform_window->width();
    m_window_height = m_platform_window->height();
    return true;
}

void Application::destroy_window() {
    if (m_platform_window) {
        m_platform_window->destroy();
        m_platform_window.reset();
    }
    m_native_window = nullptr;
    m_native_display = nullptr;
}

bool Application::poll_events() {
    if (!m_platform_window) return false;
    return m_platform_window->poll_events();
}

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
