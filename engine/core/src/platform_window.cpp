#include <engine/core/platform_window.hpp>
#include <engine/core/log.hpp>
#include <cstdlib>
#include <cstring>

namespace engine::core {

// Forward declarations of platform-specific factory functions
#if defined(_WIN32)
std::unique_ptr<PlatformWindow> create_win32_window();
#elif defined(__linux__)
std::unique_ptr<PlatformWindow> create_x11_window();
#if defined(ENGINE_HAS_WAYLAND)
bool wayland_probe();
std::unique_ptr<PlatformWindow> create_wayland_window();
#endif
#endif

std::unique_ptr<PlatformWindow> create_platform_window() {
#if defined(_WIN32)
    return create_win32_window();
#elif defined(__linux__)

#if defined(ENGINE_HAS_WAYLAND)
    {
        const char* override_env = std::getenv("ENGINE_WINDOWING_BACKEND");
        bool force_x11 = override_env && std::strcmp(override_env, "x11") == 0;
        if (!force_x11) {
            if (wayland_probe()) {
                log(LogLevel::Info, "Using Wayland windowing backend");
                return create_wayland_window();
            }
            log(LogLevel::Info, "Wayland not available, falling back to X11");
        }
    }
#endif

    log(LogLevel::Info, "Using X11 windowing backend");
    return create_x11_window();
#else
    log(LogLevel::Error, "No windowing backend available for this platform");
    return nullptr;
#endif
}

} // namespace engine::core
