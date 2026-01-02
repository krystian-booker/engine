#include <engine/script/lua_state.hpp>
#include <engine/script/bindings.hpp>
#include <engine/core/log.hpp>
#include <engine/core/filesystem.hpp>

namespace engine::script {

LuaState::LuaState() {
    // Open standard Lua libraries
    m_state.open_libraries(
        sol::lib::base,
        sol::lib::coroutine,
        sol::lib::string,
        sol::lib::table,
        sol::lib::math,
        sol::lib::utf8
    );

    // Setup sandboxed environment (disable dangerous functions)
    setup_sandbox();

    // Note: Using sol's default exception handler
    // Custom handlers require function pointers without captures
}

LuaState::~LuaState() = default;

LuaState::LuaState(LuaState&&) noexcept = default;
LuaState& LuaState::operator=(LuaState&&) noexcept = default;

void LuaState::setup_sandbox() {
    // Remove dangerous functions for security
    m_state["os"] = sol::nil;           // No OS access
    m_state["io"] = sol::nil;           // No file I/O
    m_state["loadfile"] = sol::nil;     // No loading files directly
    m_state["dofile"] = sol::nil;       // No executing files directly
    m_state["debug"] = sol::nil;        // No debug library

    // Provide safe print that goes through our logging
    m_state.set_function("print", [](sol::variadic_args va) {
        std::string output;
        for (auto v : va) {
            if (!output.empty()) output += "\t";
            sol::object obj = v;
            if (obj.is<std::string>()) {
                output += obj.as<std::string>();
            } else if (obj.is<double>()) {
                output += std::to_string(obj.as<double>());
            } else if (obj.is<bool>()) {
                output += obj.as<bool>() ? "true" : "false";
            } else if (obj.is<sol::nil_t>()) {
                output += "nil";
            } else {
                output += "[object]";
            }
        }
        core::log(core::LogLevel::Info, "[Lua] {}", output);
    });
}

bool LuaState::execute_file(const std::string& path) {
    auto content = core::FileSystem::read_text(path);
    if (content.empty()) {
        report_error("Failed to read script file: " + path);
        return false;
    }

    return execute_string(content, path);
}

bool LuaState::execute_string(const std::string& code, const std::string& chunk_name) {
    sol::protected_function_result result = m_state.safe_script(code,
        [this](lua_State*, sol::protected_function_result pfr) {
            sol::error err = pfr;
            report_error(err.what());
            return pfr;
        },
        chunk_name
    );

    return result.valid();
}

sol::object LuaState::load_script(const std::string& path) {
    auto content = core::FileSystem::read_text(path);
    if (content.empty()) {
        report_error("Failed to read script file: " + path);
        return sol::nil;
    }

    sol::load_result loaded = m_state.load(content, path);
    if (!loaded.valid()) {
        sol::error err = loaded;
        report_error(err.what());
        return sol::nil;
    }

    sol::protected_function script = loaded;
    sol::protected_function_result result = script();

    if (!result.valid()) {
        sol::error err = result;
        report_error(err.what());
        return sol::nil;
    }

    return result.get<sol::object>();
}

void LuaState::report_error(const std::string& error) {
    m_last_error = error;
    core::log(core::LogLevel::Error, "[Lua] {}", error);

    if (m_error_callback) {
        m_error_callback(error);
    }
}

// Global Lua instance
static std::unique_ptr<LuaState> s_global_lua;

LuaState& get_lua() {
    if (!s_global_lua) {
        init_lua();
    }
    return *s_global_lua;
}

void init_lua() {
    if (s_global_lua) {
        return;
    }

    core::log(core::LogLevel::Info, "Initializing Lua scripting system");
    s_global_lua = std::make_unique<LuaState>();

    // Register all engine bindings
    register_all_bindings(*s_global_lua);

    core::log(core::LogLevel::Info, "Lua scripting system initialized");
}

void shutdown_lua() {
    if (s_global_lua) {
        core::log(core::LogLevel::Info, "Shutting down Lua scripting system");
        s_global_lua.reset();
    }
}

} // namespace engine::script
