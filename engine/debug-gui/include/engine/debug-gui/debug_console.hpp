#pragma once

#include <engine/debug-gui/debug_window.hpp>
#include <engine/core/log.hpp>
#include <deque>
#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <variant>
#include <any>

namespace engine::debug_gui {

// ============================================================================
// Console Command Definition
// ============================================================================

using CommandCallback = std::function<std::string(const std::vector<std::string>&)>;

struct ConsoleCommand {
    std::string name;
    std::string description;
    std::string usage;
    CommandCallback callback;
    int min_args = 0;
    int max_args = -1;  // -1 = unlimited
};

// ============================================================================
// Watched Variable Types
// ============================================================================

using WatchedValue = std::variant<
    bool*,
    int*,
    float*,
    double*,
    std::string*
>;

struct WatchedVariable {
    std::string name;
    WatchedValue value_ptr;
    bool read_only = false;
};

// ============================================================================
// Debug Console
// ============================================================================

// Debug console window with log output, command input, and variable watching
class DebugConsole : public IDebugWindow, public core::ILogSink {
public:
    DebugConsole();
    ~DebugConsole();

    // Singleton access
    static DebugConsole& instance();

    // IDebugWindow implementation
    const char* get_name() const override { return "console"; }
    const char* get_title() const override { return "Console"; }
    uint32_t get_shortcut_key() const override;

    void on_open() override;
    void on_close() override;
    void draw() override;

    // ILogSink implementation
    void log(core::LogLevel level, const std::string& category, const std::string& message) override;

    // ========================================================================
    // Command Registration
    // ========================================================================

    // Simple registration
    using SimpleCallback = std::function<void(const std::vector<std::string>&)>;
    void register_command(const std::string& name, const std::string& help, SimpleCallback callback);

    // Full registration
    void register_command(const ConsoleCommand& cmd);

    // Unregister
    void unregister_command(const std::string& name);

    // ========================================================================
    // Aliases
    // ========================================================================

    void add_alias(const std::string& alias, const std::string& command);
    void remove_alias(const std::string& alias);

    // ========================================================================
    // Variable Watching
    // ========================================================================

    void watch(const std::string& name, bool* ptr, bool read_only = false);
    void watch(const std::string& name, int* ptr, bool read_only = false);
    void watch(const std::string& name, float* ptr, bool read_only = false);
    void watch(const std::string& name, double* ptr, bool read_only = false);
    void watch(const std::string& name, std::string* ptr, bool read_only = false);
    void unwatch(const std::string& name);

    std::vector<std::pair<std::string, std::string>> get_watched_values() const;

    // ========================================================================
    // Output
    // ========================================================================

    void add_log(core::LogLevel level, const std::string& text);
    void print(const std::string& message);  // Info level
    void print_warning(const std::string& message);
    void print_error(const std::string& message);

    // Clear all logs
    void clear();
    const std::deque<std::string>& get_history() const { return m_output_history; }

    // ========================================================================
    // Execution
    // ========================================================================

    std::string execute(const std::string& input);
    void execute_file(const std::string& path);

    // ========================================================================
    // Auto-complete
    // ========================================================================

    std::vector<std::string> get_completions(const std::string& partial) const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    // Set quit callback (called when "quit" command is executed)
    using QuitCallback = std::function<void()>;
    void set_quit_callback(QuitCallback callback) { m_quit_callback = std::move(callback); }

    // ========================================================================
    // State
    // ========================================================================

    void toggle() { m_open = !m_open; }
    bool is_open() const { return m_open; }

private:
    void execute_command(const char* command);
    void register_builtin_commands();
    std::string get_watched_value_string(const WatchedVariable& var) const;
    void set_watched_value(WatchedVariable& var, const std::string& value);

    struct LogEntry {
        core::LogLevel level;
        std::string text;
        uint32_t count = 1; // For collapsing duplicates
    };

    static constexpr size_t MAX_LOG_ENTRIES = 1000;
    static constexpr size_t MAX_HISTORY = 50;

    std::deque<LogEntry> m_log_entries;
    std::deque<std::string> m_output_history;
    std::vector<std::string> m_command_history;
    int m_history_pos = -1;
    char m_input_buffer[512] = {};
    std::vector<ConsoleCommand> m_commands;
    std::unordered_map<std::string, std::string> m_aliases;
    std::unordered_map<std::string, WatchedVariable> m_watched_vars;
    std::mutex m_log_mutex;

    // Filters
    bool m_auto_scroll = true;
    bool m_show_trace = true;
    bool m_show_debug = true;
    bool m_show_info = true;
    bool m_show_warn = true;
    bool m_show_error = true;
    bool m_collapse_duplicates = true;
    char m_filter[128] = {};

    // UI state
    bool m_show_watches = false;
    bool m_focus_input = false;

    // Quit callback
    QuitCallback m_quit_callback;
};

// ============================================================================
// Global Access
// ============================================================================

inline DebugConsole& console() { return DebugConsole::instance(); }

} // namespace engine::debug_gui
