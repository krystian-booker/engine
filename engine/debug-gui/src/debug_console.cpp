#include <engine/debug-gui/debug_console.hpp>
#include <engine/core/input.hpp>

#include <imgui.h>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace engine::debug_gui {

DebugConsole::DebugConsole() {
    // Register built-in commands
    register_command("help", "Show available commands", [this](const auto&) {
        add_log(core::LogLevel::Info, "Available commands:");
        for (const auto& cmd : m_commands) {
            add_log(core::LogLevel::Info, "  " + cmd.name + " - " + cmd.help);
        }
    });

    register_command("clear", "Clear console", [this](const auto&) {
        clear();
    });

    register_command("quit", "Exit application", [this](const auto&) {
        if (m_quit_callback) {
            m_quit_callback();
        } else {
            add_log(core::LogLevel::Warn, "Quit callback not set");
        }
    });
}

DebugConsole::~DebugConsole() {
    on_close();
}

uint32_t DebugConsole::get_shortcut_key() const {
    return static_cast<uint32_t>(core::Key::F2);
}

void DebugConsole::on_open() {
    core::add_log_sink(this);
}

void DebugConsole::on_close() {
    core::remove_log_sink(this);
}

void DebugConsole::log(core::LogLevel level, const std::string& /*category*/, const std::string& message) {
    add_log(level, message);
}

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

    // Trim to max size
    while (m_log_entries.size() > MAX_LOG_ENTRIES) {
        m_log_entries.pop_front();
    }
}

void DebugConsole::clear() {
    std::lock_guard<std::mutex> lock(m_log_mutex);
    m_log_entries.clear();
}

void DebugConsole::register_command(const std::string& name, const std::string& help, CommandCallback callback) {
    Command cmd;
    cmd.name = name;
    cmd.help = help;
    cmd.callback = callback;
    m_commands.push_back(cmd);
}

void DebugConsole::draw() {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

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
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
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

    // Command input
    bool reclaim_focus = false;
    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                      ImGuiInputTextFlags_EscapeClearsAll |
                                      ImGuiInputTextFlags_CallbackHistory;

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
        }
        return 0;
    };

    if (ImGui::InputText("##Command", m_input_buffer, sizeof(m_input_buffer), input_flags, text_callback, this)) {
        execute_command(m_input_buffer);
        m_input_buffer[0] = '\0';
        reclaim_focus = true;
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

    // Parse command and args
    std::istringstream iss(cmd_str);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    if (tokens.empty()) return;

    std::string cmd_name = tokens[0];
    std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Find and execute command
    for (const auto& cmd : m_commands) {
        if (cmd.name == cmd_name) {
            tokens.erase(tokens.begin()); // Remove command name, leaving only args
            cmd.callback(tokens);
            return;
        }
    }

    add_log(core::LogLevel::Error, "Unknown command: " + cmd_name);
}

} // namespace engine::debug_gui
