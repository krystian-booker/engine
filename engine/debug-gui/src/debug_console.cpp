#include <engine/debug-gui/debug_console.hpp>
#include <engine/core/input.hpp>
#include <engine/core/time_manager.hpp>

#include <imgui.h>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>

namespace engine::debug_gui {

// ============================================================================
// Singleton
// ============================================================================

DebugConsole& DebugConsole::instance() {
    static DebugConsole s_instance;
    return s_instance;
}

// ============================================================================
// Constructor/Destructor
// ============================================================================

DebugConsole::DebugConsole() {
    register_builtin_commands();
}

DebugConsole::~DebugConsole() {
    on_close();
}

void DebugConsole::register_builtin_commands() {
    // Help command
    register_command(ConsoleCommand{
        "help", "Show available commands or help for a specific command",
        "help [command]",
        [this](const auto& args) -> std::string {
            if (args.empty()) {
                std::string result = "Available commands:\n";
                for (const auto& cmd : m_commands) {
                    result += "  " + cmd.name + " - " + cmd.description + "\n";
                }
                return result;
            } else {
                for (const auto& cmd : m_commands) {
                    if (cmd.name == args[0]) {
                        return cmd.name + ": " + cmd.description + "\nUsage: " + cmd.usage;
                    }
                }
                return "Unknown command: " + args[0];
            }
        },
        0, 1
    });

    // Clear command
    register_command(ConsoleCommand{
        "clear", "Clear console output", "clear",
        [this](const auto&) -> std::string {
            clear();
            return "";
        }
    });

    // List command
    register_command(ConsoleCommand{
        "list", "List all commands", "list",
        [this](const auto&) -> std::string {
            std::string result;
            for (const auto& cmd : m_commands) {
                result += cmd.name + "\n";
            }
            return result;
        }
    });

    // Echo command
    register_command(ConsoleCommand{
        "echo", "Print text to console", "echo <text>",
        [](const auto& args) -> std::string {
            std::string result;
            for (const auto& arg : args) {
                if (!result.empty()) result += " ";
                result += arg;
            }
            return result;
        }
    });

    // Exec command
    register_command(ConsoleCommand{
        "exec", "Execute commands from file", "exec <filename>",
        [this](const auto& args) -> std::string {
            if (args.empty()) return "Usage: exec <filename>";
            execute_file(args[0]);
            return "Executed: " + args[0];
        },
        1, 1
    });

    // Alias command
    register_command(ConsoleCommand{
        "alias", "Create command alias", "alias <name> <command>",
        [this](const auto& args) -> std::string {
            if (args.size() < 2) return "Usage: alias <name> <command>";
            std::string cmd;
            for (size_t i = 1; i < args.size(); ++i) {
                if (!cmd.empty()) cmd += " ";
                cmd += args[i];
            }
            add_alias(args[0], cmd);
            return "Created alias: " + args[0] + " -> " + cmd;
        },
        2, -1
    });

    // Timescale command
    register_command(ConsoleCommand{
        "timescale", "Get or set time scale", "timescale [value]",
        [](const auto& args) -> std::string {
            if (args.empty()) {
                return "Time scale: " + std::to_string(core::time_manager().get_time_scale());
            }
            try {
                float scale = std::stof(args[0]);
                core::time_manager().set_time_scale(scale);
                return "Time scale set to: " + std::to_string(scale);
            } catch (...) {
                return "Invalid value: " + args[0];
            }
        },
        0, 1
    });

    // Pause command
    register_command(ConsoleCommand{
        "pause", "Toggle game pause", "pause",
        [](const auto&) -> std::string {
            core::time_manager().toggle_pause();
            return core::time_manager().is_paused() ? "Game paused" : "Game unpaused";
        }
    });

    // Watch command
    register_command(ConsoleCommand{
        "watch", "List watched variables", "watch",
        [this](const auto&) -> std::string {
            auto values = get_watched_values();
            if (values.empty()) return "No watched variables";
            std::string result = "Watched variables:\n";
            for (const auto& [name, value] : values) {
                result += "  " + name + " = " + value + "\n";
            }
            return result;
        }
    });

    // Set command
    register_command(ConsoleCommand{
        "set", "Set a watched variable value", "set <name> <value>",
        [this](const auto& args) -> std::string {
            if (args.size() < 2) return "Usage: set <name> <value>";
            auto it = m_watched_vars.find(args[0]);
            if (it == m_watched_vars.end()) {
                return "Variable not found: " + args[0];
            }
            if (it->second.read_only) {
                return "Variable is read-only: " + args[0];
            }
            set_watched_value(it->second, args[1]);
            return args[0] + " = " + args[1];
        },
        2, 2
    });

    // Get command
    register_command(ConsoleCommand{
        "get", "Get a watched variable value", "get <name>",
        [this](const auto& args) -> std::string {
            if (args.empty()) return "Usage: get <name>";
            auto it = m_watched_vars.find(args[0]);
            if (it == m_watched_vars.end()) {
                return "Variable not found: " + args[0];
            }
            return args[0] + " = " + get_watched_value_string(it->second);
        },
        1, 1
    });

    // Quit command
    register_command(ConsoleCommand{
        "quit", "Exit application", "quit",
        [this](const auto&) -> std::string {
            if (m_quit_callback) {
                m_quit_callback();
                return "Quitting...";
            }
            return "Quit callback not set";
        }
    });

    // Version command
    register_command(ConsoleCommand{
        "version", "Show engine version", "version",
        [](const auto&) -> std::string {
            return "Engine v1.0.0";  // TODO: Get from build config
        }
    });

    // FPS command
    register_command(ConsoleCommand{
        "fps", "Show current FPS", "fps",
        [](const auto&) -> std::string {
            float dt = core::time_manager().get_unscaled_delta_time();
            if (dt > 0.0f) {
                return "FPS: " + std::to_string(static_cast<int>(1.0f / dt));
            }
            return "FPS: N/A";
        }
    });
}

uint32_t DebugConsole::get_shortcut_key() const {
    return static_cast<uint32_t>(core::Key::F2);
}

void DebugConsole::on_open() {
    core::add_log_sink(this);
    m_focus_input = true;
}

void DebugConsole::on_close() {
    core::remove_log_sink(this);
}

void DebugConsole::log(core::LogLevel level, const std::string& /*category*/, const std::string& message) {
    add_log(level, message);
}

// ============================================================================
// Command Registration
// ============================================================================

void DebugConsole::register_command(const std::string& name, const std::string& help, SimpleCallback callback) {
    ConsoleCommand cmd;
    cmd.name = name;
    cmd.description = help;
    cmd.usage = name;
    cmd.callback = [callback](const auto& args) -> std::string {
        callback(args);
        return "";
    };
    register_command(cmd);
}

void DebugConsole::register_command(const ConsoleCommand& cmd) {
    // Check for duplicate
    for (auto& existing : m_commands) {
        if (existing.name == cmd.name) {
            existing = cmd;  // Replace
            return;
        }
    }
    m_commands.push_back(cmd);
}

void DebugConsole::unregister_command(const std::string& name) {
    m_commands.erase(
        std::remove_if(m_commands.begin(), m_commands.end(),
            [&name](const ConsoleCommand& cmd) { return cmd.name == name; }),
        m_commands.end()
    );
}

// ============================================================================
// Aliases
// ============================================================================

void DebugConsole::add_alias(const std::string& alias, const std::string& command) {
    m_aliases[alias] = command;
}

void DebugConsole::remove_alias(const std::string& alias) {
    m_aliases.erase(alias);
}

// ============================================================================
// Variable Watching
// ============================================================================

void DebugConsole::watch(const std::string& name, bool* ptr, bool read_only) {
    m_watched_vars[name] = WatchedVariable{name, ptr, read_only};
}

void DebugConsole::watch(const std::string& name, int* ptr, bool read_only) {
    m_watched_vars[name] = WatchedVariable{name, ptr, read_only};
}

void DebugConsole::watch(const std::string& name, float* ptr, bool read_only) {
    m_watched_vars[name] = WatchedVariable{name, ptr, read_only};
}

void DebugConsole::watch(const std::string& name, double* ptr, bool read_only) {
    m_watched_vars[name] = WatchedVariable{name, ptr, read_only};
}

void DebugConsole::watch(const std::string& name, std::string* ptr, bool read_only) {
    m_watched_vars[name] = WatchedVariable{name, ptr, read_only};
}

void DebugConsole::unwatch(const std::string& name) {
    m_watched_vars.erase(name);
}

std::vector<std::pair<std::string, std::string>> DebugConsole::get_watched_values() const {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& [name, var] : m_watched_vars) {
        result.emplace_back(name, get_watched_value_string(var));
    }
    return result;
}

std::string DebugConsole::get_watched_value_string(const WatchedVariable& var) const {
    return std::visit([](auto* ptr) -> std::string {
        if constexpr (std::is_same_v<decltype(ptr), bool*>) {
            return *ptr ? "true" : "false";
        } else if constexpr (std::is_same_v<decltype(ptr), std::string*>) {
            return "\"" + *ptr + "\"";
        } else {
            return std::to_string(*ptr);
        }
    }, var.value_ptr);
}

void DebugConsole::set_watched_value(WatchedVariable& var, const std::string& value) {
    std::visit([&value](auto* ptr) {
        if constexpr (std::is_same_v<decltype(ptr), bool*>) {
            *ptr = (value == "true" || value == "1");
        } else if constexpr (std::is_same_v<decltype(ptr), int*>) {
            *ptr = std::stoi(value);
        } else if constexpr (std::is_same_v<decltype(ptr), float*>) {
            *ptr = std::stof(value);
        } else if constexpr (std::is_same_v<decltype(ptr), double*>) {
            *ptr = std::stod(value);
        } else if constexpr (std::is_same_v<decltype(ptr), std::string*>) {
            *ptr = value;
        }
    }, var.value_ptr);
}

// ============================================================================
// Output
// ============================================================================

void DebugConsole::add_log(core::LogLevel level, const std::string& text) {
    std::lock_guard<std::mutex> lock(m_log_mutex);

    // Collapse duplicates if enabled
    if (m_collapse_duplicates && !m_log_entries.empty()) {
        auto& last = m_log_entries.back();
        if (last.text == text && last.level == level) {
            last.count++;
            return;
        }
    }

    LogEntry entry;
    entry.level = level;
    entry.text = text;

    m_log_entries.push_back(entry);
    m_output_history.push_back(text);

    // Trim to max size
    while (m_log_entries.size() > MAX_LOG_ENTRIES) {
        m_log_entries.pop_front();
    }
    while (m_output_history.size() > MAX_LOG_ENTRIES) {
        m_output_history.pop_front();
    }
}

void DebugConsole::print(const std::string& message) {
    add_log(core::LogLevel::Info, message);
}

void DebugConsole::print_warning(const std::string& message) {
    add_log(core::LogLevel::Warn, message);
}

void DebugConsole::print_error(const std::string& message) {
    add_log(core::LogLevel::Error, message);
}

void DebugConsole::clear() {
    std::lock_guard<std::mutex> lock(m_log_mutex);
    m_log_entries.clear();
}

// ============================================================================
// Execution
// ============================================================================

std::string DebugConsole::execute(const std::string& input) {
    std::string cmd_str = input;

    // Trim whitespace
    while (!cmd_str.empty() && std::isspace(cmd_str.front())) cmd_str.erase(0, 1);
    while (!cmd_str.empty() && std::isspace(cmd_str.back())) cmd_str.pop_back();

    if (cmd_str.empty()) return "";

    // Check for alias
    std::istringstream iss(cmd_str);
    std::string first_token;
    iss >> first_token;

    auto alias_it = m_aliases.find(first_token);
    if (alias_it != m_aliases.end()) {
        // Replace alias with actual command
        std::string rest;
        std::getline(iss, rest);
        cmd_str = alias_it->second + rest;
    }

    // Parse command and args
    iss.clear();
    iss.str(cmd_str);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    if (tokens.empty()) return "";

    std::string cmd_name = tokens[0];
    std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Find and execute command
    for (const auto& cmd : m_commands) {
        if (cmd.name == cmd_name) {
            tokens.erase(tokens.begin()); // Remove command name, leaving only args

            // Check argument count
            int arg_count = static_cast<int>(tokens.size());
            if (arg_count < cmd.min_args) {
                return "Not enough arguments. Usage: " + cmd.usage;
            }
            if (cmd.max_args >= 0 && arg_count > cmd.max_args) {
                return "Too many arguments. Usage: " + cmd.usage;
            }

            try {
                return cmd.callback(tokens);
            } catch (const std::exception& e) {
                return std::string("Error: ") + e.what();
            }
        }
    }

    return "Unknown command: " + cmd_name + ". Type 'help' for available commands.";
}

void DebugConsole::execute_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        print_error("Could not open file: " + path);
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == '/') continue;

        print("> " + line);
        std::string result = execute(line);
        if (!result.empty()) {
            print(result);
        }
    }
}

// ============================================================================
// Auto-complete
// ============================================================================

std::vector<std::string> DebugConsole::get_completions(const std::string& partial) const {
    std::vector<std::string> result;

    std::string lower_partial = partial;
    std::transform(lower_partial.begin(), lower_partial.end(), lower_partial.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto& cmd : m_commands) {
        if (cmd.name.find(lower_partial) == 0) {
            result.push_back(cmd.name);
        }
    }

    for (const auto& [alias, _] : m_aliases) {
        if (alias.find(lower_partial) == 0) {
            result.push_back(alias);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

// ============================================================================
// Drawing
// ============================================================================

void DebugConsole::draw() {
    ImGui::SetNextWindowSize(ImVec2(700, 450), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(get_title(), &m_open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Filter")) {
            ImGui::MenuItem("Trace", nullptr, &m_show_trace);
            ImGui::MenuItem("Debug", nullptr, &m_show_debug);
            ImGui::MenuItem("Info", nullptr, &m_show_info);
            ImGui::MenuItem("Warn", nullptr, &m_show_warn);
            ImGui::MenuItem("Error", nullptr, &m_show_error);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options")) {
            ImGui::MenuItem("Auto-scroll", nullptr, &m_auto_scroll);
            ImGui::MenuItem("Collapse duplicates", nullptr, &m_collapse_duplicates);
            ImGui::MenuItem("Show Watches", nullptr, &m_show_watches);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Watch panel (if enabled)
    if (m_show_watches && !m_watched_vars.empty()) {
        ImGui::BeginChild("WatchPanel", ImVec2(0, 100), true);
        ImGui::Text("Watched Variables:");
        ImGui::Separator();
        for (const auto& [name, var] : m_watched_vars) {
            std::string value = get_watched_value_string(var);
            ImGui::Text("%s = %s%s", name.c_str(), value.c_str(),
                       var.read_only ? " (read-only)" : "");
        }
        ImGui::EndChild();
        ImGui::Separator();
    }

    // Filter input
    ImGui::InputTextWithHint("##Filter", "Filter...", m_filter, sizeof(m_filter));
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        clear();
    }

    ImGui::Separator();

    // Log area
    const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        std::lock_guard<std::mutex> lock(m_log_mutex);

        for (const auto& entry : m_log_entries) {
            // Level filter
            bool show = false;
            switch (entry.level) {
                case core::LogLevel::Trace: show = m_show_trace; break;
                case core::LogLevel::Debug: show = m_show_debug; break;
                case core::LogLevel::Info:  show = m_show_info; break;
                case core::LogLevel::Warn:  show = m_show_warn; break;
                case core::LogLevel::Error:
                case core::LogLevel::Fatal: show = m_show_error; break;
            }
            if (!show) continue;

            // Text filter
            if (m_filter[0] != '\0' && entry.text.find(m_filter) == std::string::npos) {
                continue;
            }

            // Color by level
            ImVec4 color;
            const char* prefix;
            switch (entry.level) {
                case core::LogLevel::Trace:
                    color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                    prefix = "[TRACE]";
                    break;
                case core::LogLevel::Debug:
                    color = ImVec4(0.6f, 0.6f, 0.8f, 1.0f);
                    prefix = "[DEBUG]";
                    break;
                case core::LogLevel::Info:
                    color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                    prefix = "[INFO]";
                    break;
                case core::LogLevel::Warn:
                    color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
                    prefix = "[WARN]";
                    break;
                case core::LogLevel::Error:
                    color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                    prefix = "[ERROR]";
                    break;
                case core::LogLevel::Fatal:
                    color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                    prefix = "[FATAL]";
                    break;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            if (entry.count > 1) {
                ImGui::Text("%s (%ux) %s", prefix, entry.count, entry.text.c_str());
            } else {
                ImGui::Text("%s %s", prefix, entry.text.c_str());
            }
            ImGui::PopStyleColor();
        }

        if (m_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Command input with auto-complete
    bool reclaim_focus = false;
    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                      ImGuiInputTextFlags_EscapeClearsAll |
                                      ImGuiInputTextFlags_CallbackHistory |
                                      ImGuiInputTextFlags_CallbackCompletion;

    auto text_callback = [](ImGuiInputTextCallbackData* data) -> int {
        auto* console = static_cast<DebugConsole*>(data->UserData);

        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (console->m_history_pos < static_cast<int>(console->m_command_history.size()) - 1) {
                    console->m_history_pos++;
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, console->m_command_history[console->m_history_pos].c_str());
                }
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (console->m_history_pos > 0) {
                    console->m_history_pos--;
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, console->m_command_history[console->m_history_pos].c_str());
                } else if (console->m_history_pos == 0) {
                    console->m_history_pos = -1;
                    data->DeleteChars(0, data->BufTextLen);
                }
            }
        } else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
            // Auto-complete on Tab
            std::string partial(data->Buf, data->CursorPos);
            auto completions = console->get_completions(partial);

            if (completions.size() == 1) {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, completions[0].c_str());
                data->InsertChars(data->CursorPos, " ");
            } else if (completions.size() > 1) {
                console->print("Completions:");
                for (const auto& c : completions) {
                    console->print("  " + c);
                }
            }
        }
        return 0;
    };

    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##Command", m_input_buffer, sizeof(m_input_buffer), input_flags, text_callback, this)) {
        execute_command(m_input_buffer);
        m_input_buffer[0] = '\0';
        reclaim_focus = true;
    }
    ImGui::PopItemWidth();

    if (m_focus_input) {
        ImGui::SetKeyboardFocusHere(-1);
        m_focus_input = false;
    }

    ImGui::SetItemDefaultFocus();
    if (reclaim_focus) {
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::End();
}

void DebugConsole::execute_command(const char* command) {
    std::string cmd_str = command;

    // Add to history
    if (!cmd_str.empty()) {
        m_command_history.insert(m_command_history.begin(), cmd_str);
        if (m_command_history.size() > MAX_HISTORY) {
            m_command_history.pop_back();
        }
    }
    m_history_pos = -1;

    // Echo command
    add_log(core::LogLevel::Debug, "> " + cmd_str);

    // Execute and show result
    std::string result = execute(cmd_str);
    if (!result.empty()) {
        // Split result by newlines and print each
        std::istringstream iss(result);
        std::string line;
        while (std::getline(iss, line)) {
            add_log(core::LogLevel::Info, line);
        }
    }
}

} // namespace engine::debug_gui
