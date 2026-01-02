#include <engine/plugin/plugin_loader.hpp>
#include <engine/core/log.hpp>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

#include <fstream>

namespace engine::plugin {

const char* load_result_to_string(LoadResult result) {
    switch (result) {
        case LoadResult::Success: return "Success";
        case LoadResult::FileNotFound: return "File not found";
        case LoadResult::LoadFailed: return "Failed to load DLL";
        case LoadResult::MissingExports: return "Missing required exports";
        case LoadResult::VersionMismatch: return "Engine version mismatch";
        case LoadResult::InitFailed: return "Initialization failed";
        default: return "Unknown error";
    }
}

PluginLoader::PluginLoader() = default;

PluginLoader::~PluginLoader() {
    unload();
}

PluginLoader::PluginLoader(PluginLoader&& other) noexcept
    : m_handle(other.m_handle)
    , m_dll_path(std::move(other.m_dll_path))
    , m_loaded_path(std::move(other.m_loaded_path))
    , m_get_info(other.m_get_info)
    , m_init(other.m_init)
    , m_register_systems(other.m_register_systems)
    , m_register_components(other.m_register_components)
    , m_pre_reload(other.m_pre_reload)
    , m_post_reload(other.m_post_reload)
    , m_shutdown(other.m_shutdown)
{
    other.m_handle = nullptr;
    other.reset();
}

PluginLoader& PluginLoader::operator=(PluginLoader&& other) noexcept {
    if (this != &other) {
        unload();
        m_handle = other.m_handle;
        m_dll_path = std::move(other.m_dll_path);
        m_loaded_path = std::move(other.m_loaded_path);
        m_get_info = other.m_get_info;
        m_init = other.m_init;
        m_register_systems = other.m_register_systems;
        m_register_components = other.m_register_components;
        m_pre_reload = other.m_pre_reload;
        m_post_reload = other.m_post_reload;
        m_shutdown = other.m_shutdown;

        other.m_handle = nullptr;
        other.reset();
    }
    return *this;
}

LoadResult PluginLoader::load(const std::filesystem::path& dll_path, bool copy_before_load) {
    // Unload any existing plugin
    unload();

    m_dll_path = dll_path;

    // Check if file exists
    if (!std::filesystem::exists(dll_path)) {
        core::log(core::LogLevel::Error, "Plugin DLL not found: {}", dll_path.string());
        return LoadResult::FileNotFound;
    }

    std::filesystem::path load_path = dll_path;

    // Copy to temp file if requested (for hot reload)
    if (copy_before_load) {
        m_loaded_path = dll_path;
        m_loaded_path.replace_extension(".loaded.dll");

        try {
            std::filesystem::copy_file(dll_path, m_loaded_path,
                std::filesystem::copy_options::overwrite_existing);
            load_path = m_loaded_path;
        } catch (const std::exception& e) {
            core::log(core::LogLevel::Error, "Failed to copy DLL for hot reload: {}", e.what());
            return LoadResult::LoadFailed;
        }
    } else {
        m_loaded_path = dll_path;
    }

    // Load the DLL
#ifdef _WIN32
    m_handle = LoadLibraryW(load_path.wstring().c_str());
    if (!m_handle) {
        DWORD error = GetLastError();
        core::log(core::LogLevel::Error, "Failed to load DLL: {} (error {})",
                  load_path.string(), error);
        return LoadResult::LoadFailed;
    }
#else
    m_handle = dlopen(load_path.c_str(), RTLD_NOW);
    if (!m_handle) {
        core::log(core::LogLevel::Error, "Failed to load DLL: {} ({})",
                  load_path.string(), dlerror());
        return LoadResult::LoadFailed;
    }
#endif

    // Get required exports
    m_get_info = reinterpret_cast<GetInfoFn>(get_proc(EXPORT_GET_INFO));
    m_init = reinterpret_cast<InitFn>(get_proc(EXPORT_INIT));
    m_register_systems = reinterpret_cast<RegisterSystemsFn>(get_proc(EXPORT_REGISTER_SYSTEMS));
    m_shutdown = reinterpret_cast<ShutdownFn>(get_proc(EXPORT_SHUTDOWN));

    // Check required exports
    if (!m_get_info || !m_init || !m_register_systems || !m_shutdown) {
        core::log(core::LogLevel::Error, "Plugin DLL missing required exports");
        unload();
        return LoadResult::MissingExports;
    }

    // Get optional exports
    m_register_components = reinterpret_cast<RegisterComponentsFn>(get_proc(EXPORT_REGISTER_COMPONENTS));
    m_pre_reload = reinterpret_cast<PreReloadFn>(get_proc(EXPORT_PRE_RELOAD));
    m_post_reload = reinterpret_cast<PostReloadFn>(get_proc(EXPORT_POST_RELOAD));

    // Check version compatibility
    PluginInfo info = m_get_info();
    uint32_t major = (info.engine_version >> 16) & 0xFF;
    uint32_t engine_major = (ENGINE_VERSION >> 16) & 0xFF;

    if (major != engine_major) {
        core::log(core::LogLevel::Error, "Plugin requires engine version {}.x.x, but engine is {}.x.x",
                  major, engine_major);
        unload();
        return LoadResult::VersionMismatch;
    }

    core::log(core::LogLevel::Info, "Loaded plugin: {} v{}", info.name, info.version);
    return LoadResult::Success;
}

void PluginLoader::unload() {
    if (m_handle) {
#ifdef _WIN32
        if (!FreeLibrary(static_cast<HMODULE>(m_handle))) {
            core::log(core::LogLevel::Warn, "FreeLibrary failed for plugin (error {})", GetLastError());
        }
#else
        if (dlclose(m_handle) != 0) {
            core::log(core::LogLevel::Warn, "dlclose failed: {}", dlerror());
        }
#endif
        m_handle = nullptr;

        // Clean up temp copy if it exists
        if (m_loaded_path != m_dll_path && std::filesystem::exists(m_loaded_path)) {
            try {
                std::filesystem::remove(m_loaded_path);
            } catch (const std::exception& e) {
                core::log(core::LogLevel::Warn, "Failed to cleanup temp DLL: {}", e.what());
            }
        }
    }
    reset();
}

PluginInfo PluginLoader::get_info() const {
    if (m_get_info) {
        return m_get_info();
    }
    return {"Unknown", "0.0.0", 0};
}

bool PluginLoader::call_init(GameContext* ctx) {
    if (m_init) {
        return m_init(ctx);
    }
    return false;
}

void PluginLoader::call_register_systems(SystemRegistry* reg) {
    if (m_register_systems) {
        m_register_systems(reg);
    }
}

void PluginLoader::call_register_components() {
    if (m_register_components) {
        m_register_components();
    }
}

void PluginLoader::call_pre_reload(scene::World* world, void* state) {
    if (m_pre_reload) {
        m_pre_reload(world, state);
    }
}

void PluginLoader::call_post_reload(scene::World* world, const void* state) {
    if (m_post_reload) {
        m_post_reload(world, state);
    }
}

void PluginLoader::call_shutdown() {
    if (m_shutdown) {
        m_shutdown();
    }
}

void* PluginLoader::get_proc(const char* name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_handle), name));
#else
    return dlsym(m_handle, name);
#endif
}

void PluginLoader::reset() {
    m_get_info = nullptr;
    m_init = nullptr;
    m_register_systems = nullptr;
    m_register_components = nullptr;
    m_pre_reload = nullptr;
    m_post_reload = nullptr;
    m_shutdown = nullptr;
}

} // namespace engine::plugin
