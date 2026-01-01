#pragma once

#include <sol/sol.hpp>
#include <string>
#include <memory>
#include <functional>

namespace engine::script {

// Wrapper around sol::state that provides a safe Lua environment
class LuaState {
public:
    LuaState();
    ~LuaState();

    // Non-copyable
    LuaState(const LuaState&) = delete;
    LuaState& operator=(const LuaState&) = delete;

    // Movable
    LuaState(LuaState&&) noexcept;
    LuaState& operator=(LuaState&&) noexcept;

    // Script execution
    bool execute_file(const std::string& path);
    bool execute_string(const std::string& code, const std::string& chunk_name = "chunk");

    // Global variable access
    template<typename T>
    void set_global(const std::string& name, T&& value) {
        m_state[name] = std::forward<T>(value);
    }

    template<typename T>
    T get_global(const std::string& name) {
        return m_state[name].get<T>();
    }

    template<typename T>
    std::optional<T> try_get_global(const std::string& name) {
        sol::object obj = m_state[name];
        if (obj.valid() && obj.is<T>()) {
            return obj.as<T>();
        }
        return std::nullopt;
    }

    // Call Lua functions
    template<typename Ret, typename... Args>
    Ret call(const std::string& func_name, Args&&... args) {
        sol::function fn = m_state[func_name];
        if (fn.valid()) {
            sol::protected_function_result result = fn(std::forward<Args>(args)...);
            if (result.valid()) {
                return result.get<Ret>();
            }
        }
        return Ret{};
    }

    template<typename... Args>
    void call_void(const std::string& func_name, Args&&... args) {
        sol::function fn = m_state[func_name];
        if (fn.valid()) {
            fn(std::forward<Args>(args)...);
        }
    }

    // Register C++ types/functions
    template<typename T>
    sol::usertype<T> register_type(const std::string& name) {
        return m_state.new_usertype<T>(name);
    }

    template<auto Func>
    void register_function(const std::string& name) {
        m_state.set_function(name, Func);
    }

    void register_function(const std::string& name, sol::function fn) {
        m_state[name] = fn;
    }

    // Create a new table
    sol::table create_table() {
        return m_state.create_table();
    }

    sol::table create_named_table(const std::string& name) {
        return m_state.create_named_table(name);
    }

    // Load a script and return the result (for module-style scripts)
    sol::object load_script(const std::string& path);

    // Access sol state for advanced use
    sol::state& state() { return m_state; }
    const sol::state& state() const { return m_state; }

    // Error callback type
    using ErrorCallback = std::function<void(const std::string&)>;
    void set_error_callback(ErrorCallback callback) { m_error_callback = std::move(callback); }

    // Get last error message
    const std::string& last_error() const { return m_last_error; }

    // Garbage collection
    void collect_garbage() { m_state.collect_garbage(); }
    size_t memory_used() const { return m_state.memory_used(); }

private:
    void setup_sandbox();
    void report_error(const std::string& error);

    sol::state m_state;
    ErrorCallback m_error_callback;
    std::string m_last_error;
};

// Global Lua VM instance (lazy initialized)
LuaState& get_lua();

// Initialize the global Lua VM with engine bindings
void init_lua();

// Shutdown the global Lua VM
void shutdown_lua();

} // namespace engine::script
