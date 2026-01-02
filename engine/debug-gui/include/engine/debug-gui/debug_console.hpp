#pragma once

#include <engine/debug-gui/debug_window.hpp>
#include <engine/core/log.hpp>
#include <deque>
#include <vector>
#include <string>
#include <functional>
#include <mutex>

namespace engine::debug_gui {

// Debug console window with log output and command input
class DebugConsole : public IDebugWindow, public core::ILogSink {
public:
    DebugConsole();
    ~DebugConsole();

    // IDebugWindow implementation
    const char* get_name() const override { return "console"; }
    const char* get_title() const override { return "Console"; }
    uint32_t get_shortcut_key() const override;

    void on_open() override;
    void on_close() override;
    void draw() override;

    // ILogSink implementation
    void log(core::LogLevel level, const std::string& category, const std::string& message) override;

    // Console command registration
    using CommandCallback = std::function<void(const std::vector<std::string>&)>;
    void register_command(const std::string& name, const std::string& help, CommandCallback callback);

    // Add a log message directly
    void add_log(core::LogLevel level, const std::string& text);

    // Clear all logs
    void clear();

    // Set quit callback (called when "quit" command is executed)
    using QuitCallback = std::function<void()>;
    void set_quit_callback(QuitCallback callback) { m_quit_callback = std::move(callback); }

private:
    void execute_command(const char* command);

    struct LogEntry {
        core::LogLevel level;
        std::string text;
        uint32_t count = 1; // For collapsing duplicates
    };

    struct Command {
        std::string name;
        std::string help;
        CommandCallback callback;
    };

    static constexpr size_t MAX_LOG_ENTRIES = 1000;
    static constexpr size_t MAX_HISTORY = 50;

    std::deque<LogEntry> m_log_entries;
    std::vector<std::string> m_command_history;
    int m_history_pos = -1;
    char m_input_buffer[256] = {};
    std::vector<Command> m_commands;
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

    // Quit callback
    QuitCallback m_quit_callback;
};

} // namespace engine::debug_gui
